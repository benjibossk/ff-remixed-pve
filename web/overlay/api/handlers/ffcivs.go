// api/handlers/ffcivs.go
// Fichier des CIVILS rencontrés : qui a été croisé, où, avec quelle confiance, et ce qu'ont
// donné les contrôles d'identité (fouilles).
//
// Le jeu (FFRX_CivSender) POSTe l'intégralité de son registre toutes les 5 min. Chaque envoi
// REMPLACE l'instantané précédent — c'est un état complet, pas un delta : rien à additionner
// ici, et un redémarrage du site se répare tout seul au prochain envoi.
//
// ⚠️ CE QU'UN "CIVIL" EST VRAIMENT, et pourquoi la page doit le dire.
// Côté jeu, un civil n'est PAS une entité suivie. Chaque ZONE (carré d'1 km, "E4") possède
// une liste d'HABITANTS persistés ; quand la résistance croise un civil, il reçoit l'identité
// d'un habitant de sa zone qui n'est pas déjà incarné. Les mêmes visages reviennent donc dans
// leur village avec leur mémoire, SANS que le mod ait à reprendre le spawn de FF (tentative
// abandonnée : elle cassait la compilation).
// Conséquence à afficher honnêtement : on ne garantit pas qu'un habitant donné soit présent à
// un instant donné, ni qu'un même groupe se reforme. On garantit qu'un civil croisé dans une
// zone est quelqu'un de cette zone — un nom qui réapparaît est le BUT, pas un bug.
//
// Stockage en MÉMOIRE (même choix que /ffcensus et /ffcatalog) : la source de vérité est le
// fichier JSON du serveur de jeu ($profile:FFRX_civilians.json), pas nous. Pas de table, pas
// de migration, pas de risque de divergence.
package handlers

import (
	"encoding/json"
	"io"
	"log"
	"net/http"
	"sort"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Un registre peut contenir beaucoup de civils sur une longue campagne : on prévoit large,
// tout en gardant une borne pour ne pas se faire saturer par un envoi malformé.
const maxCivsBody = 1024 * 1024 // 1 Mo

// civEntry = un civil du registre. Noms de champs COURTS : le jeu construit ce JSON à la main
// et une longue campagne en envoie des centaines — chaque octet compte côté Enforce.
type civEntry struct {
	Key          string  `json:"k"`  // identifiant unique de la PERSONNE ("E4#12"), jamais réutilisé
	Name         string  `json:"n"`  // nom généré, déterministe à partir de la clé
	Trust        float64 `json:"t"`  // 0..100, 50 = neutre
	Interactions int     `json:"i"`  // nombre d'échanges
	X            float64 `json:"x"`  // DERNIÈRE position vue — sert à placer la fiche, pas à l'identifier
	Z            float64 `json:"z"`
	Frisks       int     `json:"f"`  // contrôles d'identité subis
	SpyFound     int     `json:"s"`  // dont contrôles ayant démasqué un espion
	Zone         string  `json:"zn"` // zone d'appartenance ("E4") — c'est L'ANCRAGE de l'identité
}

type civsPayload struct {
	Entries    []civEntry `json:"entries"`
	ReceivedAt time.Time  `json:"received_at"`
}

var (
	ffcivsMu    sync.RWMutex
	ffcivsStore = make(map[string]*civsPayload)
)

// PostFFCivsHandler : le jeu pousse son registre civil complet (Bearer).
func PostFFCivsHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	c.Request.Body = http.MaxBytesReader(c.Writer, c.Request.Body, maxCivsBody)
	raw, err := io.ReadAll(c.Request.Body)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "read error (or too large)"})
		return
	}

	var p civsPayload
	if err := json.Unmarshal(raw, &p); err != nil {
		// On logue le corps brut : c'est comme ça qu'on a diagnostiqué les JSON tronqués
		// du jeu (string.Format coupe à 8 Ko côté Enforce).
		log.Printf("[ffcivs] JSON INVALIDE de %s (%d octets): %.900s", serverID, len(raw), string(raw))
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid ffcivs json"})
		return
	}
	p.ReceivedAt = time.Now()

	ffcivsMu.Lock()
	ffcivsStore[serverID] = &p
	ffcivsMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "entries": len(p.Entries)})
}

// GetFFCivsHandler : la page Civils lit le fichier (admin / propriétaire / co-admin).
// Renvoie les entrées TRIÉES et les totaux déjà calculés — la page n'a plus qu'à afficher.
func GetFFCivsHandler(c *gin.Context) {
	serverID := c.Param("id")
	if u, err := uuid.Parse(serverID); err == nil {
		serverID = u.String()
	}

	ffcivsMu.RLock()
	p := ffcivsStore[serverID]
	ffcivsMu.RUnlock()

	c.Header("Cache-Control", "no-store")
	if p == nil {
		c.JSON(http.StatusOK, gin.H{"empty": true})
		return
	}

	entries := make([]civEntry, len(p.Entries))
	copy(entries, p.Entries)

	// Tri : les plus "travaillés" d'abord (interactions), puis les plus méfiants. C'est
	// l'ordre utile pour un officier de renseignement — pas l'ordre alphabétique.
	sort.SliceStable(entries, func(i, j int) bool {
		if entries[i].Interactions != entries[j].Interactions {
			return entries[i].Interactions > entries[j].Interactions
		}
		return entries[i].Trust < entries[j].Trust
	})

	var hostile, neutral, friendly, frisks, spies, touched int
	zones := map[string]int{}
	for _, e := range entries {
		switch {
		case e.Trust < 35:
			hostile++
		case e.Trust > 65:
			friendly++
		default:
			neutral++
		}
		frisks += e.Frisks
		spies += e.SpyFound
		if e.Interactions > 0 {
			touched++
		}
		if e.Zone != "" {
			zones[e.Zone]++
		}
	}

	c.JSON(http.StatusOK, gin.H{
		"empty":       false,
		"received_at": p.ReceivedAt,
		"entries":     entries,
		"totals": gin.H{
			"known":    len(entries),
			"touched":  touched,
			"hostile":  hostile,
			"neutral":  neutral,
			"friendly": friendly,
			"frisks":   frisks,
			"spies":    spies,
		},
		"zones": zones,
	})
}
