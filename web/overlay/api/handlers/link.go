// api/handlers/link.go
// Liaison compte web (Discord) <-> UID en jeu (Reforger) par CODE à usage unique + expirant.
// 1) Web (session) : POST /link/code -> génère un code 6 chiffres (pending, TTL 10 min).
// 2) Jeu : joueur tape `#link <code>` -> Fleet POST /link (Bearer) {code,uid,name}.
// 3) Web : matche le code -> account.bohemia_uid = uid. Cf. TASK_WEB_LINK.
package handlers

import (
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"net/http"
	"sync"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
)

type pendingLink struct {
	userID    uint64
	expiresAt time.Time
}

var (
	linkMu    sync.Mutex
	linkCodes = make(map[string]pendingLink)
)

const linkCodeTTL = 10 * time.Minute

func genLinkCode() string {
	b := make([]byte, 4)
	rand.Read(b)
	n := binary.BigEndian.Uint32(b) % 1000000
	return fmt.Sprintf("%06d", n)
}

// purge les codes expirés (appelé à la génération, garde la map propre).
func purgeExpiredCodes(now time.Time) {
	for code, pl := range linkCodes {
		if now.After(pl.expiresAt) {
			delete(linkCodes, code)
		}
	}
}

// GenerateLinkCodeHandler : l'utilisateur connecté (session) génère un code de liaison.
func GenerateLinkCodeHandler(c *gin.Context) {
	uidIface, ok := c.Get("user_id")
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not authenticated"})
		return
	}
	userID := uidIface.(uint64)

	now := time.Now()
	code := genLinkCode()
	linkMu.Lock()
	purgeExpiredCodes(now)
	linkCodes[code] = pendingLink{userID: userID, expiresAt: now.Add(linkCodeTTL)}
	linkMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"code": code, "expires_in": int(linkCodeTTL.Seconds())})
}

// PostLinkHandler : Fleet remonte {code, uid, name} quand un joueur tape #link en jeu.
// Bearer (comme les autres push). Toujours 200 (Fleet ne réagit pas au corps).
func PostLinkHandler(c *gin.Context) {
	var body struct {
		Code string `json:"code"`
		UID  string `json:"uid"`
		Name string `json:"name"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusOK, gin.H{"ok": false, "error": "invalid json"})
		return
	}

	now := time.Now()
	linkMu.Lock()
	pl, ok := linkCodes[body.Code]
	if ok {
		delete(linkCodes, body.Code) // usage unique
	}
	linkMu.Unlock()

	if !ok || now.After(pl.expiresAt) || body.UID == "" {
		c.JSON(http.StatusOK, gin.H{"ok": false, "error": "unknown or expired code"})
		return
	}

	// Lie le compte à l'UID en jeu.
	database.DB.Model(&models.User{}).Where("id = ?", pl.userID).Update("bohemia_uid", body.UID)
	// Mémorise le nom en jeu (player_identities) si fourni.
	if body.Name != "" {
		database.DB.Clauses().Save(&models.PlayerIdentity{GUID: body.UID, LastKnownName: body.Name, LastSeenAt: now})
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "user_id": pl.userID, "uid": body.UID})
}
