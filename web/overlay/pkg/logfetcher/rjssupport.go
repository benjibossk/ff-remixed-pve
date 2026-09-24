package logfetcher

// Lecture des evenements du mod "ReforgerJS Support" (workshop 685448E58B08F021).
//
// POURQUOI UN FICHIER SEPARE : le mod a change de format entre deux generations.
//
//	ancien (RJS Logging, 6572332E971992EF) : $profile:/RJS/events/*.log
//	                                         "[ts] TYPE = cle = valeur, cle = valeur"
//	                                         -> parse par regex dans fetcher.go
//	nouveau (RJS Support, 685448E58B08F021) : $profile:RJSSupport/events/*.json
//	                                          une ligne = un objet JSON complet
//
// Les deux coexistent : fetcher.go continue de lire l'ancien format si un serveur
// tourne encore avec, et ce fichier gere le nouveau. Rien n'a ete supprime.
//
// L'APPORT DECISIF : l'ancien mod ne loguait que la distance du tir. Support fournit
// killerPosition et victimPosition, donc on peut enfin poser les accrochages sur la
// carte, et il marque explicitement les kills d'IA (isAI / type "AI_KILL") -- ce qui
// compte en PVE ou l'essentiel des morts sont des IA.

import (
	"encoding/json"
	"fmt"
	"strings"
	"time"

	"gtglivemap/models"

	"github.com/google/uuid"
)

// rjsKillEntry reflete une ligne de kills.json.
//
// Le mod serialise les vecteurs en tableau "[x, y, z]" de flottants (AddVector,
// RJSSupport_Main.c) : on decode donc en []float64 et non en objet.
// Les champs joueur sont prefixes par AddPlayer() : <prefix>_id/_name/_uid/
// _platform/_faction. Pour une victime IA, playerId <= 0 et le mod ecrit des chaines
// vides -- d'ou victim_uid vide, qu'il faut traiter (voir plus bas).
type rjsKillEntry struct {
	Timestamp string `json:"timestamp"`
	Type      string `json:"type"` // KILL | TEAMKILL | AI_KILL

	KillerID      int    `json:"killer_id"`
	KillerName    string `json:"killer_name"`
	KillerUID     string `json:"killer_uid"`
	KillerFaction string `json:"killer_faction"`

	VictimID      int    `json:"victim_id"`
	VictimName    string `json:"victim_name"`
	VictimUID     string `json:"victim_uid"`
	VictimFaction string `json:"victim_faction"`

	Weapon     string `json:"weapon"`
	Distance   float64 `json:"distance"`
	IsTeamkill bool    `json:"isTeamkill"`
	IsAI       bool    `json:"isAI"`

	KillerPosition []float64 `json:"killerPosition"`
	VictimPosition []float64 `json:"victimPosition"`
}

// RJSSupportEventsDir construit le repertoire des evenements du mod.
// Cote jeu la constante vaut "$profile:RJSSupport/events" ; $profile: est resolu
// vers le dossier passe en -profile au serveur dedie.
func RJSSupportEventsDir(server models.Server) string {
	return fmt.Sprintf("%s/RJSSupport/events", strings.TrimRight(server.ProfileFolderPath, "/"))
}

// ProcessRJSSupportLogs lit kills.json et enregistre les nouveaux kills.
//
// Reutilise le meme garde-fou temporel que l'ancien parseur : on ne garde que les
// lignes strictement posterieures au dernier timestamp traite, ce qui rend la
// fonction idempotente meme si le fichier n'est jamais purge (auto_clear_events est
// a false par defaut cote mod).
func ProcessRJSSupportLogs(server models.Server) {
	killsPath := RJSSupportEventsDir(server) + "/kills.json"

	last := time.Time{}
	if server.LastProcessedKillTimestamp != nil {
		last = *server.LastProcessedKillTimestamp
	}

	events, highest := fetchAndParseLogs(server, killsPath, last, parseRJSSupportKillLine)
	if len(events) > 0 {
		SaveEventsInBatches(server, events, highest, "rjssupport-kill")
	}
}

// parseRJSSupportKillLine convertit une ligne JSON en DamageEvent.
// Signature compatible LogParserFunc, pour reutiliser tel quel fetchAndParseLogs
// (lecture locale/FTP/SFTP, filtrage par timestamp, suivi du plus recent).
func parseRJSSupportKillLine(line string, serverID uuid.UUID) (*models.DamageEvent, bool) {
	line = strings.TrimSpace(line)
	if line == "" || !strings.HasPrefix(line, "{") {
		return nil, false
	}

	var e rjsKillEntry
	if err := json.Unmarshal([]byte(line), &e); err != nil {
		return nil, false
	}

	// Le mod ecrit un ISO 8601 UTC : "2026-09-07T14:22:31Z".
	ts, err := time.Parse(time.RFC3339, e.Timestamp)
	if err != nil {
		return nil, false
	}

	// Une victime IA n'a pas d'UID. On lui donne un identifiant stable et non vide
	// car VictimGUID est NOT NULL en base, et une chaine vide empecherait de
	// distinguer une IA d'une donnee manquante.
	victimGUID := e.VictimUID
	isAI := e.IsAI || e.Type == "AI_KILL"
	if victimGUID == "" {
		if isAI {
			victimGUID = "AI"
		} else {
			victimGUID = "UNKNOWN"
		}
	}
	killerGUID := e.KillerUID
	if killerGUID == "" {
		killerGUID = "UNKNOWN"
	}

	ev := &models.DamageEvent{
		ServerID:       serverID,
		EventTimestamp: ts,
		KillerGUID:     killerGUID,
		VictimGUID:     victimGUID,
		WeaponName:     e.Weapon,
		Distance:       e.Distance,
		IsFriendlyFire: e.IsTeamkill || e.Type == "TEAMKILL",
		IsKill:         true,
		IsAI:           isAI,
		KillerName:     e.KillerName,
		VictimName:     e.VictimName,
		KillerFaction:  e.KillerFaction,
		VictimFaction:  e.VictimFaction,
	}

	// Vecteurs Enfusion : [x, y, z] avec y = altitude. La carte est en 2D, on ne
	// conserve donc que x et z. Un tableau plus court signifie une position absente
	// (ex. tueur deconnecte) : on laisse les champs a zero plutot que d'indexer hors
	// bornes.
	if len(e.KillerPosition) >= 3 {
		ev.KillerX = e.KillerPosition[0]
		ev.KillerY = e.KillerPosition[2]
	}
	if len(e.VictimPosition) >= 3 {
		ev.VictimX = e.VictimPosition[0]
		ev.VictimY = e.VictimPosition[2]
	}

	return ev, true
}
