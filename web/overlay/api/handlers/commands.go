// api/handlers/commands.go
// File d'ordres "web → jeu". Le NAVIGATEUR empile un ordre (POST /command, session),
// le JEU (Fleet) le récupère et VIDE la file (GET /commands, Bearer).
// Stockage EN MÉMOIRE par serveur. Cf. FLEET_GTG_CONTRACT.md §2.
package handlers

import (
	"net/http"
	"sync"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

// Command = un ordre à plat (mêmes champs que le contrat §2.1).
// ⚠️ Y = Z monde (nord), PAS l'altitude. X = est, Y = nord.
type Command struct {
	CmdID int     `json:"cmdId"`
	Type  string  `json:"type"` // place|move|edit|remove|setrank
	ID    int     `json:"id"`
	X     float64 `json:"x"`
	Y     float64 `json:"y"`
	Kind  int     `json:"kind"`
	Icon  int     `json:"icon"`
	Ident int     `json:"ident"`
	Dim   int     `json:"dim"`
	Sym   int     `json:"sym"`
	Color int64   `json:"color"`
	Rot   int     `json:"rot"`
	Size  int     `json:"size"`
	Vis   int     `json:"vis"`
	Ch    int     `json:"ch"`
	Text  string  `json:"text"`
	Uid   string  `json:"uid"`
	Rank  int     `json:"rank"`
	// Objectif (type "objective") : cible + titre/description. Cf. TASK_WEB_OBJECTIVES.
	Objtarget string `json:"objtarget"` // "player" | "squad" | "faction"
	Objname   string `json:"objname"`
	Objdesc   string `json:"objdesc"`
	Faction   string `json:"faction"`
	Owner     string `json:"owner"` // UID en jeu du poseur (rempli auto depuis le lien Discord<->jeu)
	Uids      string `json:"uids"`  // group_whitelist : UID autorisés, séparés par ';' ("" = ouvre le groupe)
}

const maxCommandsPerServer = 500

var (
	commandMu    sync.Mutex
	commandStore = make(map[string][]Command)
	commandSeq   = make(map[string]int)
)

// GetCommandsHandler : le jeu (Fleet) récupère la file et GTG la VIDE. server_id via Bearer.
func GetCommandsHandler(c *gin.Context) {
	sid, exists := c.Get("server_id")
	if !exists {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "server_id context missing"})
		return
	}
	serverID := sid.(uuid.UUID).String()

	commandMu.Lock()
	cmds := commandStore[serverID]
	delete(commandStore, serverID) // on vide après lecture
	commandMu.Unlock()

	if cmds == nil {
		cmds = []Command{}
	}
	c.JSON(http.StatusOK, gin.H{"commands": cmds})
}

type commandPost struct {
	ServerID string `json:"server_id"`
	Command
}

// PostCommandHandler : le NAVIGATEUR empile un ordre. Session utilisateur (pas la clé jeu).
// Vérifie que l'utilisateur est propriétaire du serveur OU admin.
func PostCommandHandler(c *gin.Context) {
	userIDIface, ok := c.Get("user_id")
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not authenticated"})
		return
	}
	userID := userIDIface.(uint64)

	var body commandPost
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid command json"})
		return
	}
	serverID, err := uuid.Parse(body.ServerID)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid or missing server_id"})
		return
	}
	if body.Type == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "missing command type"})
		return
	}

	// Contrôle d'accès : propriétaire du serveur ou admin.
	var user models.User
	if err := database.DB.First(&user, userID).Error; err != nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "user not found"})
		return
	}
	var server models.Server
	if err := database.DB.First(&server, serverID).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "server not found"})
		return
	}
	sidStr := serverID.String()

	// Permissions : admin/propriétaire = tout. Sinon, un JOUEUR LIÉ peut poser (place) et
	// gérer SES PROPRES marqueurs (move/edit/remove où marker.ownerUid == son UID). Cf. §5.
	allowed := user.AccountType == "admin" || server.OwnerID == userID
	if !allowed && user.BohemiaUID != "" {
		switch body.Type {
		case "place", "objective":
			allowed = true
		case "move", "edit", "remove":
			markerMu.RLock()
			for _, mk := range markerStore[sidStr] {
				if mk.ID == body.ID && mk.OwnerUid == user.BohemiaUID {
					allowed = true
					break
				}
			}
			markerMu.RUnlock()
		}
	}
	if !allowed {
		c.JSON(http.StatusForbidden, gin.H{"error": "you do not have permission for this action"})
		return
	}

	// Attribution : le point posé depuis le web prend l'UID en jeu du poseur (si lié).
	if user.BohemiaUID != "" {
		if body.Command.Owner == "" {
			body.Command.Owner = user.BohemiaUID
		}
		if body.Type == "place" && body.Command.Uid == "" {
			body.Command.Uid = user.BohemiaUID // uid dans la commande place
		}
	}
	commandMu.Lock()
	commandSeq[sidStr]++
	body.Command.CmdID = commandSeq[sidStr]
	q := append(commandStore[sidStr], body.Command)
	if len(q) > maxCommandsPerServer { // file bornée : on jette les plus vieux
		q = q[len(q)-maxCommandsPerServer:]
	}
	commandStore[sidStr] = q
	commandMu.Unlock()

	c.JSON(http.StatusOK, gin.H{"ok": true, "cmdId": body.Command.CmdID})
}
