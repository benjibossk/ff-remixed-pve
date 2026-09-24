package handlers

// Journal admin : reception des evenements serveur (connexions, chat, actions admin)
// et lecture pour la page /journal.
//
// Source : le plugin Fleet POST ici (POST /api/v1/events/log). On n'a PAS voulu dependre
// du mod "ReforgerJS Support" : il ecrit ses evenements dans des fichiers JSON sur la
// machine du serveur, ce qui imposerait un agent externe et casserait si le mod evolue.
// Le decoupage des evenements (CONNECT/DISCONNECT + plateforme du joueur) lui est
// toutefois emprunte, avec l'accord de son auteur -- voir CREDITS.
//
// Les kills restent dans damage_events (format historique deja exploite par la carte) ;
// c'est la lecture du journal qui fusionne les deux sources.

import (
	"net/http"
	"strconv"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"

	"gtglivemap/database"
	"gtglivemap/models"
)

// PostEventLogHandler recoit un lot d'evenements depuis le jeu.
func PostEventLogHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID)

	var payload models.EventLogPayload
	if err := c.ShouldBindJSON(&payload); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid payload"})
		return
	}
	if len(payload.Events) == 0 {
		c.JSON(http.StatusOK, gin.H{"ok": true, "stored": 0})
		return
	}

	rows := make([]models.ServerEvent, 0, len(payload.Events))
	for _, e := range payload.Events {
		if e.Type == "" {
			continue
		}

		// Le jeu envoie des epoch secondes ; 0 (ou aberrant) = maintenant.
		ts := time.Now().UTC()
		if e.Timestamp > 0 {
			ts = time.Unix(e.Timestamp, 0).UTC()
		}

		rows = append(rows, models.ServerEvent{
			ServerID:       serverID,
			EventTimestamp: ts,
			Type:           e.Type,
			PlayerUID:      e.PlayerUID,
			PlayerName:     e.PlayerName,
			Platform:       e.Platform,
			Faction:        e.Faction,
			Message:        e.Message,
			Details:        e.Details,
		})
	}

	if len(rows) == 0 {
		c.JSON(http.StatusOK, gin.H{"ok": true, "stored": 0})
		return
	}

	if err := database.DB.Create(&rows).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "db insert failed"})
		return
	}

	c.JSON(http.StatusOK, gin.H{"ok": true, "stored": len(rows)})
}

// journalRow = ligne unifiee renvoyee a la page admin (server_events + damage_events).
type journalRow struct {
	Timestamp time.Time `json:"timestamp"`
	Type      string    `json:"type"`
	Player    string    `json:"player"`
	PlayerUID string    `json:"playerUid"`
	Platform  string    `json:"platform"`
	Faction   string    `json:"faction"`
	Message   string    `json:"message"`
	// Renseignes uniquement pour les evenements de combat.
	Target string `json:"target,omitempty"`
	// GUID de la cible : rend le pseudo de la VICTIME cliquable au meme titre que celui du
	// tueur (`playerUid`). Vide quand la victime est une IA.
	TargetUID string  `json:"targetUid,omitempty"`
	Weapon    string  `json:"weapon,omitempty"`
	Distance  float64 `json:"distance,omitempty"`
	IsAI      bool    `json:"isAi,omitempty"`
}

// GetJournalHandler renvoie le flux chronologique fusionne.
// Filtres : ?type=CONNECT,CHAT  ?player=<texte>  ?hours=24  ?limit=500
func GetJournalHandler(c *gin.Context) {
	// Route admin : /api/v1/servers/:id/journal (protegee par ServerManagerMiddleware).
	serverID := c.Param("id")
	if serverID == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "server id required"})
		return
	}

	hours := 24
	if h, err := strconv.Atoi(c.Query("hours")); err == nil && h > 0 && h <= 24*30 {
		hours = h
	}
	limit := 500
	if l, err := strconv.Atoi(c.Query("limit")); err == nil && l > 0 && l <= 5000 {
		limit = l
	}
	since := time.Now().UTC().Add(-time.Duration(hours) * time.Hour)

	player := c.Query("player")
	wantTypes := map[string]bool{}
	for _, t := range c.QueryArray("type") {
		if t != "" {
			wantTypes[t] = true
		}
	}
	// Un seul parametre "type=A,B" est aussi accepte.
	if raw := c.Query("type"); raw != "" && len(wantTypes) <= 1 {
		for _, t := range splitCSV(raw) {
			wantTypes[t] = true
		}
	}

	rows := []journalRow{}

	// --- evenements serveur ---
	if len(wantTypes) == 0 || !onlyCombat(wantTypes) {
		q := database.DB.Model(&models.ServerEvent{}).
			Where("server_id = ? AND event_timestamp >= ?", serverID, since)
		if player != "" {
			q = q.Where("player_name LIKE ?", "%"+player+"%")
		}
		if len(wantTypes) > 0 {
			q = q.Where("type IN ?", keys(wantTypes))
		}
		var evts []models.ServerEvent
		q.Order("event_timestamp DESC").Limit(limit).Find(&evts)
		for _, e := range evts {
			rows = append(rows, journalRow{
				Timestamp: e.EventTimestamp,
				Type:      e.Type,
				Player:    e.PlayerName,
				PlayerUID: e.PlayerUID,
				Platform:  e.Platform,
				Faction:   e.Faction,
				Message:   e.Message,
			})
		}
	}

	// --- combats (table historique) ---
	if len(wantTypes) == 0 || wantTypes["KILL"] {
		q := database.DB.Model(&models.DamageEvent{}).
			Where("server_id = ? AND event_timestamp >= ? AND is_kill = ?", serverID, since, true)
		if player != "" {
			q = q.Where("killer_name LIKE ? OR victim_name LIKE ?", "%"+player+"%", "%"+player+"%")
		}
		var kills []models.DamageEvent
		q.Order("event_timestamp DESC").Limit(limit).Find(&kills)
		for _, k := range kills {
			rows = append(rows, journalRow{
				Timestamp: k.EventTimestamp,
				Type:      "KILL",
				Player:    k.KillerName,
				PlayerUID: k.KillerGUID,
				Faction:   k.KillerFaction,
				Target:    k.VictimName,
				TargetUID: k.VictimGUID,
				Weapon:    k.WeaponName,
				Distance:  k.Distance,
				IsAI:      k.IsAI,
				Message:   friendlyFireNote(k.IsFriendlyFire),
			})
		}
	}

	sortByTimeDesc(rows)
	if len(rows) > limit {
		rows = rows[:limit]
	}

	c.JSON(http.StatusOK, gin.H{"events": rows, "count": len(rows)})
}

func friendlyFireNote(ff bool) string {
	if ff {
		return "TIR AMI"
	}
	return ""
}

func onlyCombat(m map[string]bool) bool {
	return len(m) == 1 && m["KILL"]
}

func keys(m map[string]bool) []string {
	out := make([]string, 0, len(m))
	for k := range m {
		out = append(out, k)
	}
	return out
}

func splitCSV(s string) []string {
	out := []string{}
	cur := ""
	for _, r := range s {
		if r == ',' {
			if cur != "" {
				out = append(out, cur)
			}
			cur = ""
			continue
		}
		cur += string(r)
	}
	if cur != "" {
		out = append(out, cur)
	}
	return out
}

// tri decroissant simple (insertion) : les lots restent petits (<= limit).
func sortByTimeDesc(rows []journalRow) {
	for i := 1; i < len(rows); i++ {
		j := i
		for j > 0 && rows[j].Timestamp.After(rows[j-1].Timestamp) {
			rows[j], rows[j-1] = rows[j-1], rows[j]
			j--
		}
	}
}
