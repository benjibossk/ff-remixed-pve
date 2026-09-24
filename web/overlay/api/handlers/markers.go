// api/handlers/markers.go
// Marqueurs live (ajout Fleet). Stockage EN MÉMOIRE par serveur — pas de persistance,
// juste l'état courant poussé par le jeu. Suffisant pour l'affichage live.
package handlers

import (
	"net/http"
	"sync"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Marker = un marqueur Anarchy pour l'affichage web.
// icon/ident/dim/sym donnent la MÊME tête qu'en jeu (icône + symbole APP-6). Cf. FLEET_GTG_CONTRACT.md §1.2.
type Marker struct {
	ID    int     `json:"id"`
	X     float64 `json:"x"`     // world X (est)
	Z     float64 `json:"z"`     // world Z (nord)
	Kind  int     `json:"kind"`  // 0 civil, 1 militaire
	Icon  int     `json:"icon"`  // entrée d'icône (marqueurs civils/custom)
	Ident int     `json:"ident"` // identité APP-6 (ami/ennemi/neutre/inconnu)
	Dim   int     `json:"dim"`   // dimension (sol/air/installation)
	Sym   int     `json:"sym"`   // symbole (infanterie/blindé/…)
	Color  int64  `json:"color"` // ARGB (peut être négatif)
	Vis    int    `json:"vis"`   // 0 local,1 group,2 side,3 all
	Rot    int    `json:"rot"`
	Size   int    `json:"size"`
	Text   string `json:"text"`
	Author   string `json:"author"`   // nom du poseur ("Web" si posé depuis le site)
	Created  int64  `json:"created"`  // Unix secondes = date de pose (figée à la 1re réception)
	OwnerUid string `json:"ownerUid"` // UID en jeu du poseur (pour les permissions web)
}

var (
	markerMu    sync.RWMutex
	markerStore = make(map[string][]Marker)
)

type markersPayload struct {
	Markers []Marker `json:"markers"`
}

// PostMarkersHandler : le jeu (Fleet) pousse l'état complet des marqueurs.
// server_id vient du middleware clé API (Bearer).
func PostMarkersHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverUUID := sid.(uuid.UUID)
	serverID := serverUUID.String()

	var body markersPayload
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid marker json"})
		return
	}

	// Fige author/created d'origine : la 1re fois qu'on voit (server_id, marker_id) on enregistre
	// les valeurs reçues ; ensuite on ÉCRASE les valeurs reçues par celles stockées (Fleet
	// re-stampe `created` après un redémarrage). Cf. TASK_WEB_MARKERS §5bis.
	var metas []models.MarkerMeta
	database.DB.Where("server_id = ?", serverUUID).Find(&metas)
	metaByID := make(map[int]models.MarkerMeta, len(metas))
	for _, m := range metas {
		metaByID[m.MarkerID] = m
	}

	now := time.Now().Unix()
	var newMetas []models.MarkerMeta
	for i := range body.Markers {
		mk := &body.Markers[i]
		if meta, ok := metaByID[mk.ID]; ok {
			// Déjà connu -> on garde la date/auteur d'origine.
			mk.Created = meta.Created
			mk.Author = meta.Author
		} else {
			// 1re réception -> on fige les valeurs reçues (created reçu, sinon maintenant).
			created := mk.Created
			if created == 0 {
				created = now
			}
			mk.Created = created
			newMetas = append(newMetas, models.MarkerMeta{
				ServerID: serverUUID, MarkerID: mk.ID, Created: created, Author: mk.Author,
			})
			metaByID[mk.ID] = models.MarkerMeta{ServerID: serverUUID, MarkerID: mk.ID, Created: created, Author: mk.Author}
		}
	}
	if len(newMetas) > 0 {
		database.DB.Create(&newMetas)
	}

	markerMu.Lock()
	markerStore[serverID] = body.Markers
	markerMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "count": len(body.Markers)})
}

// GetMarkersHandler : le frontend récupère les marqueurs du serveur.
func GetMarkersHandler(c *gin.Context) {
	serverID := c.Param("server_id")
	markerMu.RLock()
	m := markerStore[serverID]
	markerMu.RUnlock()
	if m == nil {
		m = []Marker{}
	}
	c.JSON(http.StatusOK, m)
}
