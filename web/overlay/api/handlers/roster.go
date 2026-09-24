// api/handlers/roster.go
// Roster admin : TOUS les joueurs déjà connectés sur un serveur (fiche PlayerStat),
// avec nom, grade/xp connus, temps de jeu cumulé et statut en ligne. Session owner/admin.
package handlers

import (
	"net/http"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

func GetServerRosterHandler(c *gin.Context) {
	serverID, err := uuid.Parse(c.Param("id"))
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid server ID"})
		return
	}

	var stats []models.PlayerStat
	if err := database.DB.Where("server_id = ?", serverID).Find(&stats).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "roster query failed"})
		return
	}

	// Résolution des noms (player_identities).
	nameMap := make(map[string]string)
	if len(stats) > 0 {
		guids := make([]string, 0, len(stats))
		for _, s := range stats {
			guids = append(guids, s.PlayerGUID)
		}
		var idents []models.PlayerIdentity
		database.DB.Where("guid IN ?", guids).Find(&idents)
		for _, id := range idents {
			nameMap[id.GUID] = id.LastKnownName
		}
	}

	type RosterEntry struct {
		PlayerGUID      string `json:"playerGuid"`
		Name            string `json:"name"`
		Rank            int    `json:"rank"`
		XP              int    `json:"xp"`
		FactionKey      string `json:"factionKey"`
		PlaytimeSeconds int64   `json:"playtimeSeconds"`
		LastSeen        int64   `json:"lastSeen"`
		Online          bool    `json:"online"`
		Left            bool    `json:"left"`
		LastPosX        float64 `json:"lastPosX"`
		LastPosZ        float64 `json:"lastPosZ"`
	}

	sidStr := serverID.String()
	now := time.Now().UTC()
	out := make([]RosterEntry, 0, len(stats))
	for _, s := range stats {
		out = append(out, RosterEntry{
			PlayerGUID:      s.PlayerGUID,
			Name:            nameMap[s.PlayerGUID],
			Rank:            s.LastRank,
			XP:              s.LastXP,
			FactionKey:      s.FactionKey,
			PlaytimeSeconds: s.PlaytimeSeconds,
			LastSeen:        s.LastSeen.Unix(),
			Online:          now.Sub(s.LastSeen) < 30*time.Second,
			Left:            isLeft(sidStr, s.PlayerGUID),
			LastPosX:        s.LastPosX,
			LastPosZ:        s.LastPosZ,
		})
	}

	c.Header("Cache-Control", "no-store")
	c.JSON(http.StatusOK, out)
}
