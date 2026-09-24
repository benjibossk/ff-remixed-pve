// api/handlers/soldier.go
// Fiche "Mon Soldat" : agrège pour l'utilisateur connecté (via son UID lié) toutes ses stats
// PlayerStat (temps de jeu / marche / conduite, grade, xp), tous serveurs confondus.
package handlers

import (
	"encoding/json"
	"net/http"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
)

// résout le libellé d'un grade via le catalogue persistant (n'importe quel serveur ayant la faction).
func resolveRankName(factionKey string, rank int) string {
	if factionKey == "" {
		return ""
	}
	var cats []models.RankCatalog
	database.DB.Find(&cats)
	for _, cat := range cats {
		var m map[string][]Rank
		if json.Unmarshal([]byte(cat.Data), &m) != nil {
			continue
		}
		for _, r := range m[factionKey] {
			if r.I == rank {
				return r.Name
			}
		}
	}
	return ""
}

func GetSoldierHandler(c *gin.Context) {
	c.Header("Cache-Control", "no-store")
	uidIface, ok := c.Get("user_id")
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "not authenticated"})
		return
	}
	userID := uidIface.(uint64)

	var user models.User
	if err := database.DB.First(&user, userID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "user not found"})
		return
	}
	if user.BohemiaUID == "" {
		c.JSON(http.StatusOK, gin.H{"linked": false})
		return
	}

	c.JSON(http.StatusOK, buildSoldierPayload(user.BohemiaUID))
}

// GetPlayerSoldierHandler renvoie la fiche soldat d'un AUTRE joueur, désigné par son GUID.
//
// Route : GET /api/v1/servers/:id/soldier/:guid
//
// Le périmètre d'accès est volontairement celui du JOURNAL (ServerManagerMiddleware :
// propriétaire, admin global ou co-admin) plutôt qu'un droit distinct : la fiche s'ouvre en
// cliquant un pseudo dans le journal, donc quiconque peut lire le journal peut lire la
// fiche. Un droit séparé produirait des liens qui renvoient 403 — pire qu'une absence de
// lien.
//
// Aucune donnée de COMPTE n'est exposée (ni email, ni identité du site) : uniquement des
// statistiques de jeu.
func GetPlayerSoldierHandler(c *gin.Context) {
	c.Header("Cache-Control", "no-store")

	guid := c.Param("guid")
	if guid == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "guid required"})
		return
	}

	// On ne vérifie pas que le GUID "existe" : un joueur peut n'avoir encore aucune stat
	// (première connexion, aucun kill). La fiche sort alors à zéro, ce qui est une réponse
	// correcte — et évite un 404 trompeur sur un pseudo pourtant bien réel.
	c.JSON(http.StatusOK, buildSoldierPayload(guid))
}

// buildSoldierPayload agrège toutes les stats d'un GUID donné.
//
// Partagé par « Mon Soldat » et par la consultation admin : une seule définition de la
// fiche, donc aucun risque que les deux vues divergent avec le temps.
func buildSoldierPayload(bohemiaUID string) gin.H {
	user := models.User{BohemiaUID: bohemiaUID}

	var stats []models.PlayerStat
	database.DB.Where("player_guid = ?", user.BohemiaUID).Find(&stats)

	var playtime, walk, drive int64
	var distance float64
	var lastRank, lastXP int
	var factionKey string
	var lastSeen, firstSeen time.Time
	for _, s := range stats {
		playtime += s.PlaytimeSeconds
		walk += s.WalkSeconds
		drive += s.DriveSeconds
		distance += s.DistanceMeters
		if s.LastSeen.After(lastSeen) {
			lastSeen = s.LastSeen
			lastRank = s.LastRank
			lastXP = s.LastXP
			factionKey = s.FactionKey
		}
		if !s.FirstSeen.IsZero() && (firstSeen.IsZero() || s.FirstSeen.Before(firstSeen)) {
			firstSeen = s.FirstSeen
		}
	}

	// Statistiques de combat (DamageEvent, par GUID).
	var kills, deaths, friendlyKills int64
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ? AND is_kill = ? AND is_friendly_fire = ?", user.BohemiaUID, true, false).Count(&kills)
	database.DB.Model(&models.DamageEvent{}).Where("victim_guid = ? AND is_kill = ?", user.BohemiaUID, true).Count(&deaths)
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ? AND is_kill = ? AND is_friendly_fire = ?", user.BohemiaUID, true, true).Count(&friendlyKills)

	var longestKill float64
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ? AND is_kill = ?", user.BohemiaUID, true).Select("COALESCE(MAX(distance),0)").Scan(&longestKill)

	var favWeapon struct {
		WeaponName string
		N          int64
	}
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ? AND is_kill = ?", user.BohemiaUID, true).
		Select("weapon_name, count(*) as n").Group("weapon_name").Order("n desc").Limit(1).Scan(&favWeapon)

	// Combat étendu / précision / défense (tout dérivé des DamageEvent).
	uid := user.BohemiaUID
	var hitsDealt, timesHit, headshots, teamKilledDeaths, distinctVictims, distinctKillers int64
	var damageDealt, damageTaken, avgKillDist float64
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ?", uid).Count(&hitsDealt)
	database.DB.Model(&models.DamageEvent{}).Where("victim_guid = ?", uid).Count(&timesHit)
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ? AND is_kill = ? AND LOWER(hit_zone) LIKE ?", uid, true, "%head%").Count(&headshots)
	database.DB.Model(&models.DamageEvent{}).Where("victim_guid = ? AND is_kill = ? AND is_friendly_fire = ?", uid, true, true).Count(&teamKilledDeaths)
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ? AND is_kill = ?", uid, true).Distinct("victim_guid").Count(&distinctVictims)
	database.DB.Model(&models.DamageEvent{}).Where("victim_guid = ? AND is_kill = ?", uid, true).Distinct("killer_guid").Count(&distinctKillers)
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ?", uid).Select("COALESCE(SUM(damage_amount),0)").Scan(&damageDealt)
	database.DB.Model(&models.DamageEvent{}).Where("victim_guid = ?", uid).Select("COALESCE(SUM(damage_amount),0)").Scan(&damageTaken)
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ? AND is_kill = ?", uid, true).Select("COALESCE(AVG(distance),0)").Scan(&avgKillDist)

	var favHitZone struct {
		HitZone string
		N       int64
	}
	database.DB.Model(&models.DamageEvent{}).Where("killer_guid = ? AND is_kill = ? AND hit_zone <> ''", uid, true).
		Select("hit_zone, count(*) as n").Group("hit_zone").Order("n desc").Limit(1).Scan(&favHitZone)

	// Activité : dernière position connue + nb d'échantillons de position.
	var lastPos models.PlayerPosition
	hasPos := database.DB.Where("player_guid = ?", uid).Order("event_timestamp desc").First(&lastPos).Error == nil
	var posSamples int64
	database.DB.Model(&models.PlayerPosition{}).Where("player_guid = ?", uid).Count(&posSamples)

	name := ""
	var ident models.PlayerIdentity
	if database.DB.First(&ident, "guid = ?", user.BohemiaUID).Error == nil {
		name = ident.LastKnownName
	}

	var lastSeenUnix, firstSeenUnix int64
	online := false
	if !lastSeen.IsZero() {
		lastSeenUnix = lastSeen.Unix()
		online = time.Since(lastSeen) < 30*time.Second
	}
	if !firstSeen.IsZero() {
		firstSeenUnix = firstSeen.Unix()
	}

	return gin.H{
		"linked":          true,
		"uid":             user.BohemiaUID,
		"name":            name,
		"playtimeSeconds": playtime,
		"walkSeconds":     walk,
		"driveSeconds":    drive,
		"distanceMeters":  distance,
		"rank":            lastRank,
		"rankName":        resolveRankName(factionKey, lastRank),
		"xp":              lastXP,
		"factionKey":      factionKey,
		"lastSeen":        lastSeenUnix,
		"firstSeen":       firstSeenUnix,
		"online":          online,
		"servers":         len(stats),
		"kills":           kills,
		"deaths":          deaths,
		"friendlyKills":   friendlyKills,
		"longestKill":     longestKill,
		"favWeapon":       favWeapon.WeaponName,
		"hitsDealt":       hitsDealt,
		"damageDealt":     damageDealt,
		"timesHit":        timesHit,
		"damageTaken":     damageTaken,
		"headshots":       headshots,
		"avgKillDist":     avgKillDist,
		"favHitZone":      favHitZone.HitZone,
		"teamKilledDeaths": teamKilledDeaths,
		"distinctVictims": distinctVictims,
		"distinctKillers": distinctKillers,
		"posSamples":      posSamples,
		"hasLastPos":      hasPos,
		"lastPosX":        lastPos.AbsolutePosX,
		"lastPosZ":        lastPos.AbsolutePosZ,
	}
}

// ---------------------------------------------------------------------------------------
//  Carte soldat PUBLIQUE — pour l'ÉCRAN DE CHARGEMENT du jeu
// ---------------------------------------------------------------------------------------
//
// GET /api/v1/public/soldiercard/:uid
//
// POURQUOI PUBLIC, alors que /user/soldier est authentifié. Pendant l'écran de chargement,
// le client n'a AUCUNE clé d'API : la clé Bearer vit côté serveur de jeu, dans
// $profile:Fleet/GTG.json. Un endpoint authentifié serait donc inutilisable — mesuré le
// 18/09, aucune session joueur n'existe encore à ce moment-là (cf. FFRX_LoadingFeed.c).
//
// CE QU'ON EXPOSE, ET CE QU'ON N'EXPOSE PAS. Uniquement le strict nécessaire à une ligne
// d'accroche : nom en jeu, grade, XP, temps de jeu. PAS de position, PAS de statistiques de
// combat, PAS de lien avec le compte du site. Il faut déjà connaître le GUID pour
// interroger — ce n'est pas un annuaire : aucune route ne liste les UID.
func GetPublicSoldierCardHandler(c *gin.Context) {
	c.Header("Cache-Control", "no-store")

	uid := c.Param("uid")
	if uid == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "uid required"})
		return
	}

	var stats []models.PlayerStat
	database.DB.Where("player_guid = ?", uid).Find(&stats)

	// Inconnu = réponse 200 avec known:false, PAS un 404. Le jeu doit pouvoir distinguer
	// « joueur jamais vu » (on affiche le message par défaut) d'une panne réseau.
	if len(stats) == 0 {
		c.JSON(http.StatusOK, gin.H{"known": false})
		return
	}

	var playtime int64
	var rank, xp int
	var factionKey string
	var lastSeen time.Time
	for _, s := range stats {
		playtime += s.PlaytimeSeconds
		if s.LastSeen.After(lastSeen) {
			lastSeen = s.LastSeen
			rank = s.LastRank
			xp = s.LastXP
			factionKey = s.FactionKey
		}
	}

	name := ""
	var ident models.PlayerIdentity
	if database.DB.First(&ident, "guid = ?", uid).Error == nil {
		name = ident.LastKnownName
	}

	c.JSON(http.StatusOK, gin.H{
		"known":           true,
		"name":            name,
		"rank":            rank,
		"rankName":        resolveRankName(factionKey, rank),
		"xp":              xp,
		"playtimeSeconds": playtime,
	})
}
