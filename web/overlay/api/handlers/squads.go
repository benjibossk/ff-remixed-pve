// api/handlers/squads.go
// Escouades / groupes joueurs poussés par Fleet — stockage EN MÉMOIRE par serveur (comme
// markers). Le dashboard/carte affiche les escouades puis imbrique les joueurs par UID.
// Cf. FLEET_GTG_CONTRACT.md §1.3.
package handlers

import (
	"net/http"
	"sync"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Squad = un groupe joueur. leader/members = UID Reforger (== playerGuid des positions).
// color = ARGB (même convention que les marqueurs) → le frontend colore le marqueur de
// chaque joueur avec la couleur de SON escouade. Cf. FLEET_GTG_CONTRACT.md §1.3.
type Squad struct {
	ID       int      `json:"id"`
	Faction  string   `json:"faction"`
	Name     string   `json:"name"`
	Callsign string   `json:"callsign"`
	Leader   string   `json:"leader"`
	Count    int      `json:"count"`
	Members  []string `json:"members"`
	Color    int64    `json:"color"` // ARGB (peut être négatif)
}

var (
	squadMu    sync.RWMutex
	squadStore = make(map[string][]Squad)
)

type squadsPayload struct {
	Squads []Squad `json:"squads"`
}

// PostSquadsHandler : le jeu (Fleet) pousse l'état complet des escouades. server_id via Bearer.
func PostSquadsHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	var body squadsPayload
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid squad json"})
		return
	}

	squadMu.Lock()
	squadStore[serverID] = body.Squads
	squadMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "count": len(body.Squads)})
}

// GetSquadsHandler : le frontend récupère les escouades du serveur.
func GetSquadsHandler(c *gin.Context) {
	serverID := c.Param("server_id")
	squadMu.RLock()
	s := squadStore[serverID]
	squadMu.RUnlock()
	if s == nil {
		s = []Squad{}
	}
	c.JSON(http.StatusOK, s)
}
