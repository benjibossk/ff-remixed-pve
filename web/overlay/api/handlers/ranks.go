// api/handlers/ranks.go
// Catalogue des grades disponibles par faction, poussé par Fleet (nom <-> index).
// Sert à l'UI admin "Effectifs" pour proposer un menu de grades lisible (sinon fallback
// numéro). Stockage EN MÉMOIRE par serveur. Cf. FLEET_GTG_CONTRACT.md §3.
package handlers

import (
	"encoding/json"
	"net/http"
	"sync"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"gorm.io/gorm/clause"
)

// Rank = un grade : index (== valeur setrank) + libellé.
type Rank struct {
	I    int    `json:"i"`
	Name string `json:"name"`
}

// XpGrade = un palier de la table d'XP, telle que le JEU la définit
// (Flt_XpProgression.GradeFloor / GradeCeiling, mod Fleet).
// Auto = grade obtenu SANS promotion manuelle : seul Soldat l'est, après 30 min de jeu.
// Le site ne recalcule RIEN : cette table était dupliquée en dur dans /soldier, et un
// changement de seuil en jeu rendait la page fausse en silence.
type XpGrade struct {
	G     int    `json:"g"`
	Floor int    `json:"floor"`
	Ceil  int    `json:"ceil"`
	Name  string `json:"name"`
	Auto  bool   `json:"auto"`
}

var (
	rankMu    sync.RWMutex
	rankStore = make(map[string]map[string][]Rank) // serverID -> faction -> grades
	// Paliers d'XP, poussés par Fleet en même temps que les grades. En MÉMOIRE seule,
	// volontairement : Fleet les republie à chaque démarrage du jeu et la page a un repli.
	ladderStore = make(map[string][]XpGrade) // serverID -> paliers
)

type ranksPayload struct {
	Ranks map[string][]Rank `json:"ranks"` // clé = faction ("US", "USSR", …)
	// Optionnel : une version plus ancienne de Fleet ne l'envoie pas.
	Ladder []XpGrade `json:"ladder"`
}

// PostRanksHandler : le jeu (Fleet) pousse le catalogue des grades. server_id via Bearer.
func PostRanksHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	var body ranksPayload
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid ranks json"})
		return
	}

	rankMu.Lock()
	rankStore[serverID] = body.Ranks
	// Vide = on garde la table déjà connue : une version de Fleet sans `ladder` ne doit
	// pas effacer celle qu'une version plus récente aurait publiée.
	if len(body.Ladder) > 0 {
		ladderStore[serverID] = body.Ladder
	}
	rankMu.Unlock()

	// Persiste en base (survit à un redémarrage web ; Fleet ne renvoie qu'1x/reboot).
	if blob, err := json.Marshal(body.Ranks); err == nil {
		database.DB.Clauses(clause.OnConflict{
			Columns:   []clause.Column{{Name: "server_id"}},
			DoUpdates: clause.AssignmentColumns([]string{"data"}),
		}).Create(&models.RankCatalog{ServerID: serverID, Data: string(blob)})
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "factions": len(body.Ranks)})
}

// GetRanksHandler : le frontend récupère le catalogue des grades du serveur.
func GetRanksHandler(c *gin.Context) {
	serverID := c.Param("server_id")
	rankMu.RLock()
	r := rankStore[serverID]
	rankMu.RUnlock()

	// Cache vide (ex. après un redémarrage web sans que Fleet ait re-poussé) -> relire la base.
	if r == nil {
		var cat models.RankCatalog
		if err := database.DB.First(&cat, "server_id = ?", serverID).Error; err == nil && cat.Data != "" {
			var stored map[string][]Rank
			if json.Unmarshal([]byte(cat.Data), &stored) == nil {
				rankMu.Lock()
				rankStore[serverID] = stored
				rankMu.Unlock()
				r = stored
			}
		}
	}
	if r == nil {
		r = map[string][]Rank{}
	}
	c.JSON(http.StatusOK, r)
}

// GetXpLadderHandler : la table des paliers d'XP telle que le jeu la définit.
// Endpoint SÉPARÉ à dessein : /ranks renvoie une map faction->grades à la racine, et
// y ajouter un champ aurait cassé les consommateurs existants (page Effectifs).
func GetXpLadderHandler(c *gin.Context) {
	serverID := c.Param("server_id")

	rankMu.RLock()
	l := ladderStore[serverID]
	rankMu.RUnlock()

	c.Header("Cache-Control", "no-store")
	if l == nil {
		// Fleet n'a pas encore publié (site redémarré, jeu pas relancé). La page sait
		// retomber sur sa table de secours plutôt que d'afficher n'importe quoi.
		c.JSON(http.StatusOK, gin.H{"ladder": []XpGrade{}, "known": false})
		return
	}
	c.JSON(http.StatusOK, gin.H{"ladder": l, "known": true})
}
