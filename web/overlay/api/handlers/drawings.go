// api/handlers/drawings.go
// Dessins (crayons) live poussés par Fleet — stockage EN MÉMOIRE par serveur, pas de
// persistance : juste l'état courant pour l'affichage web. Cf. FLEET_GTG_CONTRACT.md §1.3.
package handlers

import (
	"net/http"
	"sync"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Drawing = une polyligne "crayon". pts = paires x,z à plat (mètres monde).
type Drawing struct {
	ID     int       `json:"id"`
	Owner  int       `json:"owner"`
	Color  int64     `json:"color"` // ARGB (peut être négatif)
	W      int       `json:"w"`     // index d'épaisseur
	Vis    int       `json:"vis"`   // 0 local,1 group,2 side,3 all
	Ch     int       `json:"ch"`
	Fill   int       `json:"fill"` // 0/1 rempli
	Author string    `json:"author"`
	Pts    []float64 `json:"pts"` // [x0,z0, x1,z1, ...] mètres monde
}

var (
	drawingMu    sync.RWMutex
	drawingStore = make(map[string][]Drawing)
)

type drawingsPayload struct {
	Drawings []Drawing `json:"drawings"`
}

// PostDrawingsHandler : le jeu (Fleet) pousse l'état complet des dessins. server_id via Bearer.
func PostDrawingsHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	var body drawingsPayload
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid drawing json"})
		return
	}

	drawingMu.Lock()
	drawingStore[serverID] = body.Drawings
	drawingMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "count": len(body.Drawings)})
}

// GetDrawingsHandler : le frontend récupère les dessins du serveur.
func GetDrawingsHandler(c *gin.Context) {
	serverID := c.Param("server_id")
	drawingMu.RLock()
	d := drawingStore[serverID]
	drawingMu.RUnlock()
	if d == nil {
		d = []Drawing{}
	}
	c.JSON(http.StatusOK, d)
}
