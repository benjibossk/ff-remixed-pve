// api/handlers/loadouts.go
// Loadouts nommés : Fleet POST (capture) / GET (application), l'admin gère nom/spécialité/suppression.
// data = blob JSON OPAQUE stocké VERBATIM. Cf. TASK_WEB_LOADOUTS.
package handlers

import (
	"net/http"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"

	"github.com/gin-gonic/gin"
	"gorm.io/gorm/clause"
)

const maxLoadoutBody = 256 * 1024 // ~256 Ko

// PostLoadoutsHandler : Fleet publie un loadout. Upsert par (faction, name). Bearer.
func PostLoadoutsHandler(c *gin.Context) {
	c.Request.Body = http.MaxBytesReader(c.Writer, c.Request.Body, maxLoadoutBody)

	var body struct {
		Name    string `json:"name"`
		Faction string `json:"faction"`
		Data    string `json:"data"`
		Owner   string `json:"owner"` // "" = dotation de faction ; sinon = tenue perso (UID)
		// Optionnel, et JAMAIS envoyé par le jeu. Sert à l'outillage d'admin pour poser
		// la catégorie sans session web. Vide = catégorie INCHANGÉE : une republication
		// depuis le jeu ne doit pas effacer ce que l'admin a réglé.
		// Rappel : la catégorie sert aussi de réservation d'escouade côté jeu
		// (cf. FFRX_LoadoutSquad) — « ECHO » réserve la dotation au génie.
		Specialty string `json:"specialty"`
	}
	if err := c.ShouldBindJSON(&body); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid loadout json (or too large)"})
		return
	}
	if body.Name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "name required"})
		return
	}

	lo := models.Loadout{
		OwnerUID:  body.Owner,
		Faction:   body.Faction,
		Name:      body.Name,
		Specialty: body.Specialty,
		Data:      body.Data, // VERBATIM, jamais reformaté
		UpdatedAt: time.Now(),
	}
	// Upsert par (owner, faction, name) : on remplace data + updated_at, et la catégorie
	// UNIQUEMENT si elle a été fournie (sinon on écraserait le réglage admin à chaque
	// publication depuis le jeu, qui ne l'envoie pas).
	updates := []string{"data", "updated_at"}
	if body.Specialty != "" {
		updates = append(updates, "specialty")
	}
	if err := database.DB.Clauses(clause.OnConflict{
		Columns:   []clause.Column{{Name: "owner_uid"}, {Name: "faction"}, {Name: "name"}},
		DoUpdates: clause.AssignmentColumns(updates),
	}).Create(&lo).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "failed to save loadout"})
		return
	}

	// Tenues perso : cap par joueur (on garde les 5 plus récentes).
	if body.Owner != "" {
		enforcePersonalCap(body.Owner, 5)
	}

	c.JSON(http.StatusOK, gin.H{"ok": true})
}

// enforcePersonalCap supprime les tenues perso les plus anciennes d'un joueur au-delà de `max`.
func enforcePersonalCap(owner string, max int) {
	var ids []uint
	database.DB.Model(&models.Loadout{}).
		Where("owner_uid = ?", owner).
		Order("updated_at desc").
		Offset(max).
		Pluck("id", &ids)
	if len(ids) > 0 {
		database.DB.Delete(&models.Loadout{}, ids)
	}
}

type loadoutResponse struct {
	ID        uint      `json:"id"`
	Owner     string    `json:"owner"`
	Name      string    `json:"name"`
	Faction   string    `json:"faction"`
	Specialty string    `json:"specialty"`
	Data      string    `json:"data"`
	UpdatedAt time.Time `json:"updated_at"`
}

func listLoadouts() []loadoutResponse {
	var rows []models.Loadout
	database.DB.Order("faction asc, specialty asc, name asc").Find(&rows)
	out := make([]loadoutResponse, len(rows))
	for i, l := range rows {
		out[i] = loadoutResponse{ID: l.ID, Owner: l.OwnerUID, Name: l.Name, Faction: l.Faction, Specialty: l.Specialty, Data: l.Data, UpdatedAt: l.UpdatedAt}
	}
	return out
}

// GetLoadoutsHandler : Fleet récupère tous les loadouts (data VERBATIM pour l'application). Bearer.
func GetLoadoutsHandler(c *gin.Context) {
	out := listLoadouts()
	// Le contrat attend {name,faction,data,specialty,owner}. data VERBATIM. owner="" = dotation faction.
	list := make([]gin.H, len(out))
	for i, l := range out {
		list[i] = gin.H{"name": l.Name, "faction": l.Faction, "data": l.Data, "specialty": l.Specialty, "owner": l.Owner}
	}
	c.JSON(http.StatusOK, gin.H{"loadouts": list})
}

// --- Tenues perso : endpoints session (le joueur lié Discord gère SES tenues) ---------------

// currentUserBohemiaUID résout l'UID Reforger lié au compte de session (ou "" si non lié).
func currentUserBohemiaUID(c *gin.Context) string {
	uidIface, ok := c.Get("user_id")
	if !ok {
		return ""
	}
	var user models.User
	if err := database.DB.First(&user, uidIface).Error; err != nil {
		return ""
	}
	return user.BohemiaUID
}

// GetMyLoadoutsHandler : liste les tenues perso du joueur connecté (owner == son UID lié).
func GetMyLoadoutsHandler(c *gin.Context) {
	c.Header("Cache-Control", "no-store")
	uid := currentUserBohemiaUID(c)
	if uid == "" {
		c.JSON(http.StatusOK, gin.H{"loadouts": []loadoutResponse{}, "linked": false})
		return
	}
	var rows []models.Loadout
	database.DB.Where("owner_uid = ?", uid).Order("updated_at desc").Find(&rows)
	out := make([]loadoutResponse, len(rows))
	for i, l := range rows {
		out[i] = loadoutResponse{ID: l.ID, Owner: l.OwnerUID, Name: l.Name, Faction: l.Faction, Specialty: l.Specialty, UpdatedAt: l.UpdatedAt}
	}
	c.JSON(http.StatusOK, gin.H{"loadouts": out, "linked": true})
}

// UpdateMyLoadoutHandler : renommer UNE de mes tenues perso. PUT /user/loadouts/:id {name}.
func UpdateMyLoadoutHandler(c *gin.Context) {
	uid := currentUserBohemiaUID(c)
	if uid == "" {
		c.JSON(http.StatusForbidden, gin.H{"error": "compte non lié à un perso (#link en jeu)"})
		return
	}
	var req struct {
		Name string `json:"name"`
	}
	if err := c.ShouldBindJSON(&req); err != nil || req.Name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "name required"})
		return
	}
	res := database.DB.Model(&models.Loadout{}).
		Where("id = ? AND owner_uid = ?", c.Param("id"), uid).
		Updates(map[string]interface{}{"name": req.Name, "updated_at": time.Now()})
	if res.Error != nil {
		c.JSON(http.StatusConflict, gin.H{"error": "renommage échoué (nom déjà pris ?)"})
		return
	}
	if res.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "tenue introuvable (pas la tienne ?)"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

// DeleteMyLoadoutHandler : supprimer UNE de mes tenues perso. DELETE /user/loadouts/:id.
func DeleteMyLoadoutHandler(c *gin.Context) {
	uid := currentUserBohemiaUID(c)
	if uid == "" {
		c.JSON(http.StatusForbidden, gin.H{"error": "compte non lié à un perso (#link en jeu)"})
		return
	}
	res := database.DB.Where("id = ? AND owner_uid = ?", c.Param("id"), uid).Delete(&models.Loadout{})
	if res.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "suppression échouée"})
		return
	}
	if res.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "tenue introuvable (pas la tienne ?)"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

// AdminGetLoadoutsHandler : liste complète pour la page admin (session admin).
func AdminGetLoadoutsHandler(c *gin.Context) {
	c.Header("Cache-Control", "no-store")
	c.JSON(http.StatusOK, gin.H{"loadouts": listLoadouts()})
}

// SessionListLoadoutsHandler : liste des dotations pour TOUT utilisateur connecté (intranet).
// Ne renvoie QUE les dotations de faction (owner == "") + les tenues perso du demandeur.
// Les tenues perso des AUTRES joueurs sont privées (cf. la caisse en jeu, qui filtre déjà
// par UID : le site ne doit pas les exposer non plus).
func SessionListLoadoutsHandler(c *gin.Context) {
	c.Header("Cache-Control", "no-store")
	uid := currentUserBohemiaUID(c)
	all := listLoadouts()
	out := make([]loadoutResponse, 0, len(all))
	for _, l := range all {
		if l.Owner == "" || (uid != "" && l.Owner == uid) {
			out = append(out, l)
		}
	}
	c.JSON(http.StatusOK, gin.H{"loadouts": out})
}

// AdminUpdateLoadoutHandler : renommer / changer la spécialité. PUT /admin/loadouts/:id (session admin).
func AdminUpdateLoadoutHandler(c *gin.Context) {
	id := c.Param("id")
	var req struct {
		Name      *string `json:"name"`
		Specialty *string `json:"specialty"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "invalid request"})
		return
	}
	updates := map[string]interface{}{"updated_at": time.Now()}
	if req.Name != nil {
		if *req.Name == "" {
			c.JSON(http.StatusBadRequest, gin.H{"error": "name cannot be empty"})
			return
		}
		updates["name"] = *req.Name
	}
	if req.Specialty != nil {
		updates["specialty"] = *req.Specialty
	}
	res := database.DB.Model(&models.Loadout{}).Where("id = ?", id).Updates(updates)
	if res.Error != nil {
		// probable collision (faction,name) unique
		c.JSON(http.StatusConflict, gin.H{"error": "update failed (nom déjà pris pour cette faction ?)"})
		return
	}
	if res.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "loadout not found"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

// AdminDeleteLoadoutHandler : DELETE /admin/loadouts/:id (session admin).
func AdminDeleteLoadoutHandler(c *gin.Context) {
	id := c.Param("id")
	res := database.DB.Delete(&models.Loadout{}, id)
	if res.Error != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "delete failed"})
		return
	}
	if res.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "loadout not found"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"ok": true})
}
