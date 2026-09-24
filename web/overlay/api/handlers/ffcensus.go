// api/handlers/ffcensus.go
// Recensement des SPAWNS ennemis : ce que le jeu fait réellement apparaître (types de groupe,
// soldats, véhicules) et dans quelles proportions. Sert à juger si l'ennemi rencontré est
// représentatif et équilibré.
//
// Le jeu (FFRX_SpawnCensus) POSTe un cumul depuis le démarrage du serveur, toutes les 10 min.
// Chaque envoi REMPLACE l'instantané précédent — c'est un cumul, pas un delta, donc rien à
// additionner côté site et un redémarrage du site se répare tout seul au prochain envoi.
//
// Stockage en MÉMOIRE volontairement (même choix que /ffcatalog) : la donnée est reconstruite
// par le jeu à chaque cycle et n'a aucune valeur historique — pas de table, pas de migration.
package handlers

import (
	"encoding/json"
	"io"
	"log"
	"net/http"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

const maxCensusBody = 512 * 1024 // ~512 Ko : une longue session peut cumuler beaucoup de prefabs distincts

// censusEntry = une ligne du recensement. cat: group | soldier | vehicle | source
type censusEntry struct {
	Cat   string `json:"cat"`
	Name  string `json:"n"`
	Count int    `json:"c"`
}

type censusPayload struct {
	DurationSec int           `json:"duration_sec"`
	GroupSpawns int           `json:"group_spawns"`
	Entries     []censusEntry `json:"entries"`
	ReceivedAt  time.Time     `json:"received_at"`
}

var (
	ffcensusMu    sync.RWMutex
	ffcensusStore = make(map[string]*censusPayload)
)

// PostFFCensusHandler : le jeu pousse le recensement cumulé (Bearer).
func PostFFCensusHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	c.Request.Body = http.MaxBytesReader(c.Writer, c.Request.Body, maxCensusBody)
	raw, err := io.ReadAll(c.Request.Body)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "read error (or too large)"})
		return
	}

	var p censusPayload
	if err := json.Unmarshal(raw, &p); err != nil {
		// Log du corps brut : c'est comme ça qu'on a diagnostiqué les JSON malformés du jeu.
		log.Printf("[ffcensus] JSON INVALIDE de %s (%d octets): %.900s", serverID, len(raw), string(raw))
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid ffcensus json"})
		return
	}
	p.ReceivedAt = time.Now()

	ffcensusMu.Lock()
	ffcensusStore[serverID] = &p
	ffcensusMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "entries": len(p.Entries)})
}

// censusBucket = une catégorie prête à afficher, triée du plus fréquent au moins fréquent.
type censusBucket struct {
	Total int           `json:"total"`
	Items []censusEntry `json:"items"`
}

// GetFFCensusHandler : la livemap admin lit le recensement (admin/propriétaire only).
// Renvoie les entrées déjà regroupées par catégorie et triées — la page n'a plus qu'à afficher.
func GetFFCensusHandler(c *gin.Context) {
	serverID := c.Param("id")
	if u, err := uuid.Parse(serverID); err == nil {
		serverID = u.String()
	}

	ffcensusMu.RLock()
	p := ffcensusStore[serverID]
	ffcensusMu.RUnlock()

	c.Header("Cache-Control", "no-store")
	if p == nil {
		c.JSON(http.StatusOK, gin.H{"empty": true})
		return
	}

	buckets := map[string]*censusBucket{}
	for _, e := range p.Entries {
		b, ok := buckets[e.Cat]
		if !ok {
			b = &censusBucket{}
			buckets[e.Cat] = b
		}
		b.Items = append(b.Items, e)
		b.Total += e.Count
	}
	for _, b := range buckets {
		items := b.Items
		// Tri décroissant par count (insertion : quelques dizaines d'entrées au plus).
		for i := 1; i < len(items); i++ {
			cur := items[i]
			j := i - 1
			for j >= 0 && items[j].Count < cur.Count {
				items[j+1] = items[j]
				j--
			}
			items[j+1] = cur
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"empty":        false,
		"duration_sec": p.DurationSec,
		"group_spawns": p.GroupSpawns,
		"received_at":  p.ReceivedAt,
		"buckets":      buckets,
	})
}
