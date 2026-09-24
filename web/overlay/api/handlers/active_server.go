package handlers

// Mode "serveur unique".
//
// Le projet a ete concu multi-serveurs (colonne server_id sur 8 tables, middleware
// d'ownership, partage d'acces...). Dans notre deploiement il n'y a qu'UN serveur et
// il n'y en aura pas d'autre : le selecteur de serveur et l'ecran de gestion ont ete
// retires de l'interface.
//
// Plutot que d'arracher server_id de toute la base et des ~170 references Go (refonte
// lourde et migration destructive sur des tables contenant les positions et stats
// joueurs), on garde le schema intact et on resout le serveur actif automatiquement.
// Le front n'a donc plus besoin de passer ?server_id= : il demande /public/active-server
// une fois au chargement.
//
// Si un second serveur devait reapparaitre un jour, il suffirait de reafficher le
// selecteur : rien n'a ete supprime.

import (
	"net/http"
	"sync"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

var (
	activeServerMu       sync.RWMutex
	activeServerCached   *models.Server
	activeServerFetchedA time.Time
)

// activeServerTTL evite de retaper la base a chaque requete tout en laissant la valeur
// se rafraichir si le serveur est recree/renomme.
const activeServerTTL = 60 * time.Second

// GetActiveServer renvoie le serveur unique de l'instance.
//
// Ordre de resolution :
//  1. le serveur tague "demo_server_id" (reglage admin) s'il existe encore ;
//  2. sinon le plus ancien serveur cree.
//
// Retourne nil si la base n'en contient aucun (instance vierge) : les appelants
// doivent traiter ce cas plutot que de dereferencer.
func GetActiveServer() *models.Server {
	activeServerMu.RLock()
	if activeServerCached != nil && time.Since(activeServerFetchedA) < activeServerTTL {
		s := activeServerCached
		activeServerMu.RUnlock()
		return s
	}
	activeServerMu.RUnlock()

	var srv models.Server

	// 1) Le serveur explicitement designe comme serveur vedette, s'il est valide.
	var demoSetting models.SystemSetting
	if err := database.DB.First(&demoSetting, "`key` = ?", "demo_server_id").Error; err == nil && demoSetting.Value != "" {
		if id, err := uuid.Parse(demoSetting.Value); err == nil {
			if err := database.DB.Preload("MapConfig").First(&srv, "id = ?", id).Error; err == nil {
				return cacheActiveServer(&srv)
			}
		}
	}

	// 2) Repli : le premier serveur cree.
	if err := database.DB.Preload("MapConfig").Order("created_at ASC").First(&srv).Error; err != nil {
		return nil
	}
	return cacheActiveServer(&srv)
}

func cacheActiveServer(s *models.Server) *models.Server {
	activeServerMu.Lock()
	activeServerCached = s
	activeServerFetchedA = time.Now()
	activeServerMu.Unlock()
	return s
}

// InvalidateActiveServer force la prochaine lecture a retaper la base. A appeler apres
// creation/suppression/modification d'un serveur.
func InvalidateActiveServer() {
	activeServerMu.Lock()
	activeServerCached = nil
	activeServerMu.Unlock()
}

// GetActiveServerHandler expose le serveur actif au front : GET /api/v1/public/active-server
//
// Le front appelle ceci au chargement et se sert de l'id renvoye partout ou les routes
// attendent encore un :server_id.
func GetActiveServerHandler(c *gin.Context) {
	srv := GetActiveServer()
	if srv == nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "no server configured"})
		return
	}

	srv.MapConfig.TilesURL = rewriteTilesURL(srv.MapConfig.TilesURL)

	c.JSON(http.StatusOK, gin.H{
		"id":         srv.ID,
		"name":       srv.Name,
		"map_config": srv.MapConfig,
	})
}
