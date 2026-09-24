// api/handlers/ffstate.go
// État live Freedom Fighters + Réoccupation pour la livemap admin : POIs (villes/bases/usines/
// tours radio) avec faction + progression de capture, batailles en cours, contre-attaques
// planifiées, patrouilles, recon, couverture radio. Poussé par le jeu (Fleet ou l'addon FF-REMIXED),
// stockage EN MÉMOIRE par serveur (snapshot le plus récent). Le blob JSON est stocké VERBATIM :
// le jeu peut faire évoluer le schéma sans toucher au Go. Cf. TASK_WEB_FFSTATE.
package handlers

import (
	"encoding/json"
	"log"
	"net/http"
	"strconv"
	"sync"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

var (
	ffstateMu    sync.RWMutex
	ffstateStore = make(map[string]json.RawMessage)
)

// PostFFStateHandler : le jeu pousse le snapshot d'état FF (server_id via Bearer/APIKey).
func PostFFStateHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	var raw json.RawMessage
	if err := c.ShouldBindJSON(&raw); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid ffstate json"})
		return
	}

	ffstateMu.Lock()
	ffstateStore[serverID] = raw
	ffstateMu.Unlock()

	// Historique echantillonne (voir models.FFStateSample). Le snapshot ci-dessus ne
	// garde que l'instant present ; on veut aussi la TENDANCE pour juger de l'avancement.
	sampleFFState(sid.(uuid.UUID), raw)

	c.JSON(http.StatusOK, gin.H{"ok": true, "bytes": len(raw)})
}

// --- Historique echantillonne ---

// Le jeu pousse toutes les ~5 s. Tout enregistrer ferait ~17 000 lignes/jour pour une
// courbe qui n'a besoin que d'une resolution a la minute.
const ffstateSampleEvery = time.Minute

var (
	ffSampleMu   sync.Mutex
	ffLastSample = make(map[string]time.Time)
)

// ffStatePayload : seuls les champs necessaires aux agregats. Le reste du blob est
// ignore volontairement, pour que le jeu puisse faire evoluer son schema sans casser
// le Go (meme principe que le stockage verbatim du snapshot).
type ffStatePayload struct {
	Summary struct {
		PlayerPct     float64 `json:"playerPct"`
		EnemyPct      float64 `json:"enemyPct"`
		CampaignPct   float64 `json:"campaignPct"`
		EnemyManpower int     `json:"enemyManpower"`

		// Economie deja agregee par le jeu, sur les sites TENUS uniquement.
		// `prod`/`fuelProd` sont des debits MESURES (cf. FFRX_ProductionTracker),
		// pas des maximums theoriques.
		Supplies          int `json:"supplies"`
		SuppliesMax       int `json:"suppliesMax"`
		Prod              int `json:"prod"`
		Factories         int `json:"factories"`
		FactoriesTotal    int `json:"factoriesTotal"`
		Fuel              int `json:"fuel"`
		FuelMax           int `json:"fuelMax"`
		FuelProd          int `json:"fuelProd"`
		FuelStations      int `json:"fuelStations"`
		FuelStationsTotal int `json:"fuelStationsTotal"`

		// Serie "carte entiere" (voir models.FFStateSample).
		SuppliesAll    int `json:"suppliesAll"`
		SuppliesMaxAll int `json:"suppliesMaxAll"`
		ProdAll        int `json:"prodAll"`
		FuelAll        int `json:"fuelAll"`
		FuelMaxAll     int `json:"fuelMaxAll"`
		FuelProdAll    int `json:"fuelProdAll"`
	} `json:"summary"`
	// Les POIs ne servent plus qu'au comptage de territoire : l'economie vient du
	// summary (voir plus bas), qui porte aussi les debits.
	Pois []struct {
		Type     string `json:"type"`
		Role     string `json:"role"`
		Garrison int    `json:"garrison"`
	} `json:"pois"`
	Players []struct {
		Name string `json:"name"`
	} `json:"players"`
}

func sampleFFState(serverID uuid.UUID, raw json.RawMessage) {
	key := serverID.String()

	ffSampleMu.Lock()
	last, seen := ffLastSample[key]
	if seen && time.Since(last) < ffstateSampleEvery {
		ffSampleMu.Unlock()
		return
	}
	ffLastSample[key] = time.Now()
	ffSampleMu.Unlock()

	var p ffStatePayload
	if err := json.Unmarshal(raw, &p); err != nil {
		return // payload inattendu : pas d'echantillon, mais le snapshot reste servi
	}

	s := models.FFStateSample{
		ServerID:    serverID,
		RecordedAt:  time.Now(),
		CampaignPct: p.Summary.CampaignPct,
		PlayerPct:   p.Summary.PlayerPct,
		EnemyPct:    p.Summary.EnemyPct,
	}
	// Economie : on reprend les agregats du JEU tels quels. Ils portent la meme mesure
	// que ce qui s'affiche en jeu (donc aucune divergence possible) et surtout les
	// DEBITS, qu'on ne peut pas reconstituer depuis les POIs -- la production reelle de
	// FF depend d'un facteur horaire et d'un alea, elle est mesuree cote jeu.
	s.SuppliesPlayer = p.Summary.Supplies
	s.SuppliesMax = p.Summary.SuppliesMax
	s.FuelPlayer = p.Summary.Fuel
	s.FuelMax = p.Summary.FuelMax
	s.ProdSupplies = p.Summary.Prod
	s.ProdFuel = p.Summary.FuelProd
	s.FactoriesPlayer = p.Summary.Factories
	s.FactoriesTotal = p.Summary.FactoriesTotal
	s.FuelStationsPlayer = p.Summary.FuelStations
	s.FuelStationsTotal = p.Summary.FuelStationsTotal
	s.SuppliesAll = p.Summary.SuppliesAll
	s.SuppliesMaxAll = p.Summary.SuppliesMaxAll
	s.ProdSuppliesAll = p.Summary.ProdAll
	s.FuelAll = p.Summary.FuelAll
	s.FuelMaxAll = p.Summary.FuelMaxAll
	s.ProdFuelAll = p.Summary.FuelProdAll

	// Le territoire, lui, n'est pas resume par le jeu au-dela des pourcentages : on
	// compte les POIs. Les bases militaires n'ont pas d'equivalent dans le summary.
	for _, poi := range p.Pois {
		s.PoisTotal++
		isPlayer := poi.Role == "PLAYER"
		if isPlayer {
			s.PoisPlayer++
		} else if poi.Role == "ENEMY" {
			s.PoisEnemy++
			s.EnemyManpower += poi.Garrison
		}
		if poi.Type == "base" {
			s.BasesTotal++
			if isPlayer {
				s.BasesPlayer++
			}
		}
	}
	s.PlayersOnline = len(p.Players)

	if err := database.DB.Create(&s).Error; err != nil {
		log.Printf("ffstate: echec ecriture echantillon: %v", err)
	}
}

// GetFFStateHistoryHandler : série temporelle des échantillons FF (page Économie).
// Même garde que GetFFStateHandler — les stocks et les sites tenus renseignent
// directement sur nos points faibles, ça ne doit pas être public.
//
// `hours` borne la fenêtre (défaut 24, plafond 720 = 30 j). `step` sous-échantillonne
// côté serveur : à 1 point/minute, 30 jours feraient 43 000 lignes pour un graphe qui
// n'en affiche que quelques centaines. On garde 1 ligne sur N plutôt que de moyenner,
// pour que les valeurs restent des mesures réelles et non des artefacts de lissage.
func GetFFStateHistoryHandler(c *gin.Context) {
	serverID := c.Param("id")
	u, err := uuid.Parse(serverID)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid server id"})
		return
	}

	hours := 24
	if v, err := strconv.Atoi(c.Query("hours")); err == nil && v > 0 {
		hours = v
	}
	if hours > 720 {
		hours = 720
	}

	var rows []models.FFStateSample
	if err := database.DB.
		Where("server_id = ? AND recorded_at >= ?", u, time.Now().Add(-time.Duration(hours)*time.Hour)).
		Order("recorded_at asc").
		Find(&rows).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to read ffstate history"})
		return
	}

	// Garde au plus maxPoints échantillons, régulièrement espacés.
	const maxPoints = 600
	if len(rows) > maxPoints {
		step := (len(rows) + maxPoints - 1) / maxPoints
		kept := rows[:0:0]
		for i := 0; i < len(rows); i += step {
			kept = append(kept, rows[i])
		}
		// Le dernier point est le plus important (état courant) : on le force.
		if last := rows[len(rows)-1]; kept[len(kept)-1].ID != last.ID {
			kept = append(kept, last)
		}
		rows = kept
	}

	out := make([]gin.H, len(rows))
	for i, r := range rows {
		out[i] = gin.H{
			"t":                 r.RecordedAt.Unix(),
			"campaignPct":       r.CampaignPct,
			"playerPct":         r.PlayerPct,
			"enemyPct":          r.EnemyPct,
			"supplies":          r.SuppliesPlayer,
			"suppliesMax":       r.SuppliesMax,
			"prodSupplies":      r.ProdSupplies,
			"fuel":              r.FuelPlayer,
			"fuelMax":           r.FuelMax,
			"prodFuel":          r.ProdFuel,
			"suppliesAll":       r.SuppliesAll,
			"suppliesMaxAll":    r.SuppliesMaxAll,
			"prodSuppliesAll":   r.ProdSuppliesAll,
			"fuelAll":           r.FuelAll,
			"fuelMaxAll":        r.FuelMaxAll,
			"prodFuelAll":       r.ProdFuelAll,
			"factories":         r.FactoriesPlayer,
			"factoriesTotal":    r.FactoriesTotal,
			"fuelStations":      r.FuelStationsPlayer,
			"fuelStationsTotal": r.FuelStationsTotal,
			"bases":             r.BasesPlayer,
			"basesTotal":        r.BasesTotal,
			"pois":              r.PoisPlayer,
			"poisTotal":         r.PoisTotal,
			"poisEnemy":         r.PoisEnemy,
			"enemyManpower":     r.EnemyManpower,
			"players":           r.PlayersOnline,
		}
	}

	c.Header("Cache-Control", "no-store")
	c.JSON(http.StatusOK, gin.H{"samples": out, "hours": hours})
}

// GetFFStateHandler : la livemap admin récupère le dernier snapshot FF du serveur.
// Route sous /servers/:id (session + ServerOwnerOrAdminMiddleware) — jamais public
// (révèle les positions ennemies : contre-attaques, recon...).
func GetFFStateHandler(c *gin.Context) {
	serverID := c.Param("id")
	if u, err := uuid.Parse(serverID); err == nil {
		serverID = u.String() // forme canonique = même clé que le POST
	}
	ffstateMu.RLock()
	raw := ffstateStore[serverID]
	ffstateMu.RUnlock()

	c.Header("Cache-Control", "no-store")
	if raw == nil {
		c.Data(http.StatusOK, "application/json", []byte("{}"))
		return
	}
	c.Data(http.StatusOK, "application/json", raw)
}
