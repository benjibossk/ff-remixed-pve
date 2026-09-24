// api/handlers/coverage.go
// Couverture radio (fog of war) : un cercle par faction (centre HQ, rayon = somme des portées
// des bases). Poussé par Fleet, stockage EN MÉMOIRE par serveur. Cf. TASK_WEB_RADIO_FOG Phase 2.
package handlers

import (
	"net/http"
	"sync"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Coverage = cercle de couverture radio d'une faction. x,z = centre monde (mètres), radius = mètres.
// color (ARGB, optionnel) = couleur de faction ; sinon le frontend la déduit des positions.
type Coverage struct {
	Faction string  `json:"faction"`
	X       float64 `json:"x"`
	Z       float64 `json:"z"`
	Radius  float64 `json:"radius"`
	Color   int64   `json:"color"`
}

var (
	coverageMu    sync.RWMutex
	coverageStore = make(map[string][]Coverage)
)

type coveragePayload struct {
	Coverage []Coverage `json:"coverage"`
}

// PostCoverageHandler : le jeu (Fleet) pousse les cercles de couverture. server_id via Bearer.
func PostCoverageHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	var body coveragePayload
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid coverage json"})
		return
	}

	coverageMu.Lock()
	coverageStore[serverID] = body.Coverage
	coverageMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "count": len(body.Coverage)})
}

// GetCoverageHandler : le frontend récupère les cercles de couverture du serveur.
func GetCoverageHandler(c *gin.Context) {
	serverID := c.Param("server_id")
	coverageMu.RLock()
	cov := coverageStore[serverID]
	coverageMu.RUnlock()
	if cov == nil {
		cov = []Coverage{}
	}
	c.JSON(http.StatusOK, cov)
}
