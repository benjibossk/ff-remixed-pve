// api/handlers/collab.go
// Collaboration intranet : boîte à idées (/idees) + doc de design (/design).
// Tout est réservé aux connectés (groupe private, session Discord). Les actions admin
// (changer le statut d'une idée, éditer le doc) passent par le middleware AdminOnly.
// Cf. TASK_WEB_COLLAB.
package handlers

import (
	"net/http"
	"sort"
	"strconv"
	"strings"
	"time"

	"gtglivemap/database"
	"gtglivemap/models"
	"gtglivemap/pkg/moderation"

	"github.com/gin-gonic/gin"
)

// --- Helpers ---

// sessionUserID récupère l'ID Discord de l'utilisateur courant (posé par SessionAuthMiddleware).
func sessionUserID(c *gin.Context) (uint64, bool) {
	v, ok := c.Get("user_id")
	if !ok {
		return 0, false
	}
	uid, ok := v.(uint64)
	return uid, ok
}

func userIsAdmin(uid uint64) bool {
	var u models.User
	if database.DB.First(&u, uid).Error != nil {
		return false
	}
	return u.AccountType == "admin"
}

// namesFor résout les pseudos Discord pour un lot d'IDs (évite le N+1).
func namesFor(ids []uint64) map[uint64]string {
	out := map[uint64]string{}
	if len(ids) == 0 {
		return out
	}
	var users []models.User
	database.DB.Where("id IN ?", ids).Find(&users)
	for _, u := range users {
		out[u.ID] = u.Username
	}
	return out
}

func fillCommentNames(comments []models.Comment) {
	ids := make([]uint64, 0, len(comments))
	for _, cm := range comments {
		ids = append(ids, cm.AuthorID)
	}
	names := namesFor(ids)
	for i := range comments {
		comments[i].AuthorName = names[comments[i].AuthorID]
	}
}

func loadComments(targetType, targetID string) []models.Comment {
	var comments []models.Comment
	database.DB.Where("target_type = ? AND target_id = ?", targetType, targetID).
		Order("created_at asc").Find(&comments)
	fillCommentNames(comments)
	return comments
}

// createCommentInternal : validation + modération + insertion d'un commentaire.
func createCommentInternal(c *gin.Context, targetType, baseTargetID string) {
	uid, ok := sessionUserID(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "non connecté"})
		return
	}
	var req struct {
		Body   string `json:"body"`
		Anchor string `json:"anchor"`
	}
	_ = c.ShouldBindJSON(&req)
	body := strings.TrimSpace(req.Body)
	if body == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "commentaire vide"})
		return
	}
	if !moderation.IsClean(body) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "contenu inapproprié"})
		return
	}
	tid := baseTargetID
	if targetType == "design" && strings.TrimSpace(req.Anchor) != "" {
		tid = baseTargetID + "#" + strings.TrimSpace(req.Anchor)
	}
	cm := models.Comment{TargetType: targetType, TargetID: tid, AuthorID: uid, Body: body}
	if err := database.DB.Create(&cm).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "échec d'enregistrement"})
		return
	}
	cm.AuthorName = namesFor([]uint64{uid})[uid]
	c.JSON(http.StatusCreated, cm)
}

// --- Idées ---

// GetIdeasHandler : liste des idées, filtrable (status, category) et triable (votes|new).
func GetIdeasHandler(c *gin.Context) {
	uid, _ := sessionUserID(c)

	q := database.DB.Model(&models.Idea{})
	if s := c.Query("status"); s != "" {
		q = q.Where("status = ?", s)
	}
	if cat := c.Query("category"); cat != "" {
		q = q.Where("category = ?", cat)
	}
	var ideas []models.Idea
	q.Find(&ideas)

	// Compteurs de votes (agrégat unique).
	voteCounts := map[uint]int{}
	var vcs []struct {
		IdeaID uint
		N      int
	}
	database.DB.Model(&models.IdeaVote{}).Select("idea_id, count(*) as n").Group("idea_id").Scan(&vcs)
	for _, v := range vcs {
		voteCounts[v.IdeaID] = v.N
	}

	// Votes de l'utilisateur courant.
	myVotes := map[uint]bool{}
	if uid != 0 {
		var mv []models.IdeaVote
		database.DB.Where("user_id = ?", uid).Find(&mv)
		for _, v := range mv {
			myVotes[v.IdeaID] = true
		}
	}

	// Pseudos des auteurs.
	ids := make([]uint64, 0, len(ideas))
	for _, it := range ideas {
		ids = append(ids, it.AuthorID)
	}
	names := namesFor(ids)

	for i := range ideas {
		ideas[i].VoteCount = voteCounts[ideas[i].ID]
		ideas[i].HasVoted = myVotes[ideas[i].ID]
		ideas[i].AuthorName = names[ideas[i].AuthorID]
	}

	if c.DefaultQuery("sort", "votes") == "new" {
		sort.SliceStable(ideas, func(i, j int) bool { return ideas[i].CreatedAt.After(ideas[j].CreatedAt) })
	} else {
		sort.SliceStable(ideas, func(i, j int) bool { return ideas[i].VoteCount > ideas[j].VoteCount })
	}

	c.Header("Cache-Control", "no-store")
	c.JSON(http.StatusOK, ideas)
}

// CreateIdeaHandler : crée une idée (auteur = utilisateur courant).
func CreateIdeaHandler(c *gin.Context) {
	uid, ok := sessionUserID(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "non connecté"})
		return
	}
	var req struct {
		Title    string `json:"title"`
		Category string `json:"category"`
		Body     string `json:"body"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "requête invalide"})
		return
	}
	req.Title = strings.TrimSpace(req.Title)
	if req.Title == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "titre requis"})
		return
	}
	if r := []rune(req.Title); len(r) > 180 {
		req.Title = string(r[:180])
	}
	if !moderation.IsClean(req.Title) || !moderation.IsClean(req.Body) {
		c.JSON(http.StatusBadRequest, gin.H{"error": "contenu inapproprié"})
		return
	}
	idea := models.Idea{
		AuthorID: uid,
		Title:    req.Title,
		Category: strings.TrimSpace(req.Category),
		Body:     strings.TrimSpace(req.Body),
		Status:   "open",
	}
	if err := database.DB.Create(&idea).Error; err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "échec d'enregistrement"})
		return
	}
	idea.AuthorName = namesFor([]uint64{uid})[uid]
	idea.VoteCount = 0
	c.JSON(http.StatusCreated, idea)
}

// GetIdeaHandler : détail d'une idée + ses commentaires.
func GetIdeaHandler(c *gin.Context) {
	id := c.Param("id")
	var idea models.Idea
	if err := database.DB.First(&idea, id).Error; err != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "idée introuvable"})
		return
	}
	uid, _ := sessionUserID(c)
	var vcount int64
	database.DB.Model(&models.IdeaVote{}).Where("idea_id = ?", idea.ID).Count(&vcount)
	idea.VoteCount = int(vcount)
	if uid != 0 {
		var n int64
		database.DB.Model(&models.IdeaVote{}).Where("idea_id = ? AND user_id = ?", idea.ID, uid).Count(&n)
		idea.HasVoted = n > 0
	}
	idea.AuthorName = namesFor([]uint64{idea.AuthorID})[idea.AuthorID]

	c.Header("Cache-Control", "no-store")
	c.JSON(http.StatusOK, gin.H{"idea": idea, "comments": loadComments("idea", id)})
}

// ToggleIdeaVoteHandler : ajoute/retire le vote de l'utilisateur courant.
func ToggleIdeaVoteHandler(c *gin.Context) {
	uid, ok := sessionUserID(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "non connecté"})
		return
	}
	id64, err := strconv.ParseUint(c.Param("id"), 10, 64)
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "id invalide"})
		return
	}
	ideaID := uint(id64)
	if database.DB.First(&models.Idea{}, ideaID).Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "idée introuvable"})
		return
	}

	var existing models.IdeaVote
	found := database.DB.Where("idea_id = ? AND user_id = ?", ideaID, uid).First(&existing).Error == nil
	hasVoted := false
	if found {
		database.DB.Where("idea_id = ? AND user_id = ?", ideaID, uid).Delete(&models.IdeaVote{})
	} else {
		database.DB.Create(&models.IdeaVote{IdeaID: ideaID, UserID: uid})
		hasVoted = true
	}
	var count int64
	database.DB.Model(&models.IdeaVote{}).Where("idea_id = ?", ideaID).Count(&count)
	c.JSON(http.StatusOK, gin.H{"voteCount": count, "hasVoted": hasVoted})
}

// CreateIdeaCommentHandler : ajoute un commentaire à une idée.
func CreateIdeaCommentHandler(c *gin.Context) {
	id := c.Param("id")
	if database.DB.First(&models.Idea{}, id).Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "idée introuvable"})
		return
	}
	createCommentInternal(c, "idea", id)
}

// PatchIdeaHandler : change le statut d'une idée (ADMIN, via middleware).
func PatchIdeaHandler(c *gin.Context) {
	id := c.Param("id")
	var req struct {
		Status string `json:"status"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "requête invalide"})
		return
	}
	valid := map[string]bool{"open": true, "planned": true, "in_progress": true, "done": true, "declined": true}
	if !valid[req.Status] {
		c.JSON(http.StatusBadRequest, gin.H{"error": "statut invalide"})
		return
	}
	res := database.DB.Model(&models.Idea{}).Where("id = ?", id).Update("status", req.Status)
	if res.RowsAffected == 0 {
		c.JSON(http.StatusNotFound, gin.H{"error": "idée introuvable"})
		return
	}
	c.JSON(http.StatusOK, gin.H{"ok": true, "status": req.Status})
}

// DeleteIdeaHandler : supprime une idée (ADMIN ou auteur) + ses votes/commentaires.
func DeleteIdeaHandler(c *gin.Context) {
	uid, ok := sessionUserID(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "non connecté"})
		return
	}
	id := c.Param("id")
	var idea models.Idea
	if database.DB.First(&idea, id).Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "idée introuvable"})
		return
	}
	if idea.AuthorID != uid && !userIsAdmin(uid) {
		c.JSON(http.StatusForbidden, gin.H{"error": "action non autorisée"})
		return
	}
	database.DB.Delete(&models.Idea{}, idea.ID)
	database.DB.Where("idea_id = ?", idea.ID).Delete(&models.IdeaVote{})
	database.DB.Where("target_type = ? AND target_id = ?", "idea", id).Delete(&models.Comment{})
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

// DeleteCommentHandler : supprime un commentaire (ADMIN ou auteur).
func DeleteCommentHandler(c *gin.Context) {
	uid, ok := sessionUserID(c)
	if !ok {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "non connecté"})
		return
	}
	id := c.Param("id")
	var cm models.Comment
	if database.DB.First(&cm, id).Error != nil {
		c.JSON(http.StatusNotFound, gin.H{"error": "commentaire introuvable"})
		return
	}
	if cm.AuthorID != uid && !userIsAdmin(uid) {
		c.JSON(http.StatusForbidden, gin.H{"error": "action non autorisée"})
		return
	}
	database.DB.Delete(&models.Comment{}, id)
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

// --- Doc de design ---

// GetDesignHandler : renvoie le markdown du doc (ou un doc vide si pas encore créé).
func GetDesignHandler(c *gin.Context) {
	slug := c.Param("slug")
	c.Header("Cache-Control", "no-store")
	var doc models.DesignDoc
	if database.DB.First(&doc, "slug = ?", slug).Error != nil {
		c.JSON(http.StatusOK, gin.H{"slug": slug, "title": "", "markdown": "", "exists": false})
		return
	}
	c.JSON(http.StatusOK, gin.H{
		"slug":      doc.Slug,
		"title":     doc.Title,
		"markdown":  doc.Markdown,
		"updatedAt": doc.UpdatedAt,
		"updatedBy": doc.UpdatedBy,
		"exists":    true,
	})
}

// PutDesignHandler : enregistre le markdown (ADMIN, via middleware). Upsert par slug.
func PutDesignHandler(c *gin.Context) {
	uid, _ := sessionUserID(c)
	slug := c.Param("slug")
	var req struct {
		Title    string `json:"title"`
		Markdown string `json:"markdown"`
	}
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "requête invalide"})
		return
	}
	var count int64
	database.DB.Model(&models.DesignDoc{}).Where("slug = ?", slug).Count(&count)
	if count == 0 {
		doc := models.DesignDoc{Slug: slug, Title: req.Title, Markdown: req.Markdown, UpdatedBy: uid, UpdatedAt: time.Now()}
		if err := database.DB.Create(&doc).Error; err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": "échec d'enregistrement"})
			return
		}
	} else {
		database.DB.Model(&models.DesignDoc{}).Where("slug = ?", slug).Updates(map[string]interface{}{
			"title": req.Title, "markdown": req.Markdown, "updated_by": uid, "updated_at": time.Now(),
		})
	}
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

// GetDesignCommentsHandler : commentaires du doc (globaux + par ancre de section).
func GetDesignCommentsHandler(c *gin.Context) {
	slug := c.Param("slug")
	c.Header("Cache-Control", "no-store")
	var comments []models.Comment
	database.DB.Where("target_type = ? AND (target_id = ? OR target_id LIKE ?)", "design", slug, slug+"#%").
		Order("created_at asc").Find(&comments)
	fillCommentNames(comments)
	c.JSON(http.StatusOK, comments)
}

// CreateDesignCommentHandler : ajoute un commentaire au doc (ancre optionnelle).
func CreateDesignCommentHandler(c *gin.Context) {
	createCommentInternal(c, "design", c.Param("slug"))
}

// GetTestCommentsHandler : retours des soldats sur un test (clé stable définie côté page).
func GetTestCommentsHandler(c *gin.Context) {
	key := c.Param("key")
	c.Header("Cache-Control", "no-store")
	var comments []models.Comment
	database.DB.Where("target_type = ? AND target_id = ?", "test", key).
		Order("created_at asc").Find(&comments)
	fillCommentNames(comments)
	c.JSON(http.StatusOK, comments)
}

// CreateTestCommentHandler : un soldat écrit son retour sous un test.
func CreateTestCommentHandler(c *gin.Context) {
	createCommentInternal(c, "test", c.Param("key"))
}
