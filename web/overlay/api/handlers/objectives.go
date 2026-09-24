// api/handlers/objectives.go
// Objectifs ACTIFS poussés par Fleet pour affichage sur la carte (les tâches créées en jeu).
// Stockage EN MÉMOIRE par serveur (comme markers/squads). Cf. TASK_WEB_OBJECTIVES (affichage).
package handlers

import (
	"net/http"
	"sync"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Objective = tâche affichée sur la carte. x,z = position monde (mètres) ; 0,0 = sans lieu.
type Objective struct {
	ID      int     `json:"id"`
	Faction string  `json:"faction"`
	Name    string  `json:"name"`
	Desc    string  `json:"desc"`
	X       float64 `json:"x"`
	Z       float64 `json:"z"`
	State   int     `json:"state"` // optionnel (0 actif…)
}

var (
	objectiveMu    sync.RWMutex
	objectiveStore = make(map[string][]Objective)
)

type objectivesPayload struct {
	Objectives []Objective `json:"objectives"`
}

// PostObjectivesHandler : Fleet pousse l'état complet des objectifs actifs. server_id via Bearer.
func PostObjectivesHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	var body objectivesPayload
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid objectives json"})
		return
	}

	objectiveMu.Lock()
	objectiveStore[serverID] = body.Objectives
	objectiveMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "count": len(body.Objectives)})
}

// GetObjectivesHandler : le frontend récupère les objectifs du serveur.
func GetObjectivesHandler(c *gin.Context) {
	serverID := c.Param("server_id")
	objectiveMu.RLock()
	o := objectiveStore[serverID]
	objectiveMu.RUnlock()
	if o == nil {
		o = []Objective{}
	}
	c.JSON(http.StatusOK, o)
}
