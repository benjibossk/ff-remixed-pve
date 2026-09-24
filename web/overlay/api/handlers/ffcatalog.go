// api/handlers/ffcatalog.go
// Catalogues de RÉFÉRENCE FF (statiques) capturés UNE FOIS au démarrage du jeu :
// catalogue d'entités par faction (personnages/véhicules/items), etc. Contrairement à /ffstate
// (live toutes les 5s), c'est un snapshot unique stocké en mémoire, lu par l'admin. Cf. TASK_FFSTATE_SENDER.
package handlers

import (
	"encoding/json"
	"io"
	"log"
	"net/http"
	"sync"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

var (
	ffcatalogMu    sync.RWMutex
	ffcatalogStore = make(map[string]json.RawMessage)
)

// PostFFCatalogHandler : le jeu pousse le catalogue de référence (une fois au boot, Bearer).
func PostFFCatalogHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	raw, err := io.ReadAll(c.Request.Body)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "read error"})
		return
	}
	if !json.Valid(raw) {
		// DEBUG : log le corps brut pour diagnostiquer le JSON malforme du jeu.
		log.Printf("[ffcatalog] JSON INVALIDE de %s (%d octets): %.900s", serverID, len(raw), string(raw))
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid ffcatalog json"})
		return
	}

	ffcatalogMu.Lock()
	ffcatalogStore[serverID] = json.RawMessage(raw)
	ffcatalogMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "bytes": len(raw)})
}

// GetFFCatalogHandler : la livemap admin récupère le catalogue de référence (admin/propriétaire only).
func GetFFCatalogHandler(c *gin.Context) {
	serverID := c.Param("id")
	if u, err := uuid.Parse(serverID); err == nil {
		serverID = u.String()
	}
	ffcatalogMu.RLock()
	raw := ffcatalogStore[serverID]
	ffcatalogMu.RUnlock()

	c.Header("Cache-Control", "no-store")
	if raw == nil {
		c.Data(http.StatusOK, "application/json", []byte("{}"))
		return
	}
	c.Data(http.StatusOK, "application/json", raw)
}
