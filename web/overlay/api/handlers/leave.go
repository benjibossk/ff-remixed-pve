// api/handlers/leave.go
// Départ VOLONTAIRE d'un joueur (Fleet POST /leave quand timeout<=0 = départ propre/kick).
// On marque l'UID "parti" -> retiré immédiatement du live (latest positions) et de l'affichage
// hors-ligne. Une position reçue plus tard (reconnexion) efface le flag. Les crashs (timeout>0)
// n'appellent PAS /leave -> le web garde son fondu 3 min.
package handlers

import (
	"net/http"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

var (
	leftMu      sync.Mutex
	leftPlayers = make(map[string]map[string]time.Time) // serverID -> uid -> leftAt
)

func markLeft(serverID, uid string) {
	leftMu.Lock()
	defer leftMu.Unlock()
	if leftPlayers[serverID] == nil {
		leftPlayers[serverID] = make(map[string]time.Time)
	}
	leftPlayers[serverID][uid] = time.Now()
}

func clearLeft(serverID, uid string) {
	leftMu.Lock()
	defer leftMu.Unlock()
	if m := leftPlayers[serverID]; m != nil {
		delete(m, uid)
	}
}

func isLeft(serverID, uid string) bool {
	leftMu.Lock()
	defer leftMu.Unlock()
	m := leftPlayers[serverID]
	if m == nil {
		return false
	}
	_, ok := m[uid]
	return ok
}

// PostLeaveHandler : Fleet signale un départ volontaire. Bearer (server_id via la clé).
func PostLeaveHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	var body struct {
		UID string `json:"uid"`
	}
	if err := c.ShouldBindJSON(&body); err != nil || body.UID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "uid required"})
		return
	}
	markLeft(serverID, body.UID)
	c.JSON(http.StatusOK, gin.H{"ok": true})
}
