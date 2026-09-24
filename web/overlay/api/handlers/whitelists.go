// api/handlers/whitelists.go
// Whitelists d'escouade (contrôle d'accès à un groupe, ex. KILO Commandement).
// L'admin/propriétaire gère la liste des UID autorisés par groupe : on PERSISTE en DB
// ET on pousse un ordre "group_whitelist" au jeu (live) via la file de commandes.
// UID == BohemiaUID (lien Discord<->jeu). Côté jeu : FFRX_Groups.c allowedPlayerUIDs
// + FFRX_GroupsManager.SetGroupWhitelistCsv. Routes sous /servers/:id (owner/admin).
package handlers

import (
	"net/http"
	"strings"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"gorm.io/gorm/clause"
)

// GET /servers/:id/whitelists — whitelists persistées du serveur.
func GetWhitelistsHandler(c *gin.Context) {
	serverID, err := uuid.Parse(c.Param("id"))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid server ID"})
		return
	}
	var wls []models.SquadWhitelist
	if err := database.DB.Where("server_id = ?", serverID.String()).Find(&wls).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "whitelist query failed"})
		return
	}
	if wls == nil {
		wls = []models.SquadWhitelist{}
	}
	c.JSON(http.StatusOK, wls)
}

// POST /servers/:id/whitelist — upsert d'une whitelist + push live au jeu.
// body { "groupId": 1011, "groupName": "KILO — Commandement", "uids": ["uid1","uid2"] }
// uids vide = efface la liste (le groupe redevient ouvert à tous).
func SetWhitelistHandler(c *gin.Context) {
	serverID, err := uuid.Parse(c.Param("id"))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid server ID"})
		return
	}
	var body struct {
		GroupID   int      `json:"groupId"`
		GroupName string   `json:"groupName"`
		Uids      []string `json:"uids"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid whitelist json"})
		return
	}

	// Nettoyage + dédup des UID.
	seen := map[string]bool{}
	clean := make([]string, 0, len(body.Uids))
	for _, u := range body.Uids {
		u = strings.TrimSpace(u)
		if u == "" || seen[u] {
			continue
		}
		seen[u] = true
		clean = append(clean, u)
	}
	csv := strings.Join(clean, ";")
	sidStr := serverID.String()

	// Persistance (upsert par (server_id, group_id)).
	wl := models.SquadWhitelist{
		ServerID:  sidStr,
		GroupID:   body.GroupID,
		GroupName: body.GroupName,
		Uids:      csv,
		UpdatedAt: time.Now(),
	}
	if err := database.DB.Clauses(clause.OnConflict{
		Columns:   []clause.Column{{Name: "server_id"}, {Name: "group_id"}},
		DoUpdates: clause.AssignmentColumns([]string{"group_name", "uids", "updated_at"}),
	}).Create(&wl).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to save whitelist"})
		return
	}

	// Push live au jeu : ordre group_whitelist dans la file de commandes (même
	// mécanique que PostCommandHandler ; Fleet la récupère sur GET /commands).
	cmd := Command{Type: "group_whitelist", ID: body.GroupID, Uids: csv}
	commandMu.Lock()
	commandSeq[sidStr]++
	cmd.CmdID = commandSeq[sidStr]
	q := append(commandStore[sidStr], cmd)
	if len(q) > maxCommandsPerServer {
		q = q[len(q)-maxCommandsPerServer:]
	}
	commandStore[sidStr] = q
	commandMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "count": len(clean)})
}
