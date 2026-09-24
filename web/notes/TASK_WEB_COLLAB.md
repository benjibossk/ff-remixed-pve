# TASK_WEB_COLLAB — Boîte à idées (/idees) + Doc de design (/design) NATIFS

## But
Intégrer DANS l'app Go (pas d'outil externe) deux pages collaboratives pour préparer le scénario
**FF - REMIXED - PVE**, en réutilisant l'existant : **session Discord déjà en place** (aucun 2e login)
et **`static/theme.css`** (DA C2 SCORPION). Objectif de Benji : « se connecter une fois, accès à tout,
tout à la charte ».

Remplace la solution provisoire HedgeDoc (qui tournait sur `arma.collectifxxl.fr/design` via un
conteneur séparé) : une fois ces pages natives en ligne, on **retire HedgeDoc** (cf. §Cutover).

Deux fonctionnalités :
1. **`/idees`** — boîte à idées : proposer / voter / commenter, avec statuts (roadmap).
2. **`/design`** — document de design en markdown, affiché + commentable (édition réservée admin).

Public visé = les connectés de l'intranet (comme `/soldier`, `/loadouts`). Voir aussi le doc de
référence `GAME_DESIGN.md` (dans l'addon FF - REMIXED - PVE) que Benji collera dans /design via l'UI.

---

## Conventions à respecter (déjà en place dans le repo)
- **Auth session** : groupe `private` dans `main.go` (`middleware.SessionAuthMiddleware()`). Récupérer
  l'utilisateur courant comme ailleurs : `session, _ := CookieStore.Get(c.Request, "gtg-livemap-session")`
  → `userID := session.Values["user_id"].(uint64)` → `database.DB.First(&user, userID)`.
  Admin = `user.AccountType == "admin"` (ou middleware `AdminOnlyMiddleware()` sur les routes admin).
- **DB** : GORM `database.DB` ; nouveaux modèles dans `models/models.go` ; les ajouter à la liste
  `AutoMigrate(...)` dans `database/database.go`.
- **Pages** : fichier statique servi via `r.GET("/idees", func(c){ c.File("./static/idees.html") })`
  (+ `/idees.html`), idem `/design`. `static/` est déjà monté ; le NoRoute renvoie index.html.
- **Modération** : passer TOUT texte utilisateur (titre/corps idée, commentaires) dans le filtre
  existant (`pkg/moderation`, cf. usage dans markers/commands) avant insertion.
- **Style** : copier la navbar + entête + classes DA depuis `static/soldier.html` ou `dashboard.html`
  (bandeau "Diffusion Restreinte", crest, `theme.css`). Pas de nouveau login : la navbar montre déjà
  l'avatar via `/api/v1/users/me`.
- Commentaires de code en français (comme le reste), style handlers existants.

---

## 1) Modèles (models/models.go) + AutoMigrate
```go
// Idée proposée par un membre.
type Idea struct {
    ID        uint      `gorm:"primaryKey" json:"id"`
    AuthorID  uint64    `json:"authorId"`                 // User.ID (Discord)
    Title     string    `gorm:"size:180" json:"title"`
    Body      string    `gorm:"type:text" json:"body"`
    Category  string    `gorm:"size:40;index" json:"category"` // ex: "Renseignement","Logistique","IA",...
    Status    string    `gorm:"size:20;index;default:open" json:"status"` // open|planned|in_progress|done|declined
    CreatedAt time.Time `json:"createdAt"`
    UpdatedAt time.Time `json:"updatedAt"`
    // champs calculés renvoyés à l'API (pas en base) :
    VoteCount int  `gorm:"-" json:"voteCount"`
    HasVoted  bool `gorm:"-" json:"hasVoted"`   // l'utilisateur courant a voté ?
    AuthorName string `gorm:"-" json:"authorName"`
}

// Un vote = 1 par (idée, user).
type IdeaVote struct {
    IdeaID uint   `gorm:"primaryKey" json:"ideaId"`
    UserID uint64 `gorm:"primaryKey" json:"userId"`
}

// Commentaire générique (idées ET doc de design).
type Comment struct {
    ID         uint      `gorm:"primaryKey" json:"id"`
    TargetType string    `gorm:"size:16;index" json:"targetType"` // "idea" | "design"
    TargetID   string    `gorm:"size:64;index" json:"targetId"`   // idea.ID en texte, ou ancre de section design
    AuthorID   uint64    `json:"authorId"`
    Body       string    `gorm:"type:text" json:"body"`
    CreatedAt  time.Time `json:"createdAt"`
    AuthorName string    `gorm:"-" json:"authorName"`
}

// Le doc de design (un seul par slug ; markdown édité par l'admin).
type DesignDoc struct {
    Slug      string    `gorm:"primaryKey;size:40" json:"slug"` // ex: "ff-remixed-pve"
    Title     string    `gorm:"size:180" json:"title"`
    Markdown  string    `gorm:"type:longtext" json:"markdown"`
    UpdatedBy uint64    `json:"updatedBy"`
    UpdatedAt time.Time `json:"updatedAt"`
}
```
Ajouter `&models.Idea{}, &models.IdeaVote{}, &models.Comment{}, &models.DesignDoc{}` à `AutoMigrate`.

VoteCount : `SELECT idea_id, COUNT(*) FROM idea_votes GROUP BY idea_id` (batch), ou sous-requête.
AuthorName : joindre `users.username` par AuthorID (batch map pour éviter le N+1).

---

## 2) Endpoints (nouveau handler `api/handlers/collab.go`)
Tout sous le groupe `private` (session requise = intranet). Routes admin sous le sous-groupe `admin`.

**Idées**
- `GET  /api/v1/ideas?status=&category=&sort=votes|new` → liste (avec voteCount + hasVoted pour le user courant).
- `POST /api/v1/ideas` `{title,category,body}` → crée (auteur = user courant ; modération du texte).
- `GET  /api/v1/ideas/:id` → détail + commentaires (Comment where target_type='idea' target_id=:id).
- `POST /api/v1/ideas/:id/vote` → **toggle** (insère ou supprime le IdeaVote du user) → renvoie voteCount+hasVoted.
- `POST /api/v1/ideas/:id/comments` `{body}` → ajoute un commentaire (modération).
- `PATCH /api/v1/ideas/:id` `{status}` → **admin** : change le statut (roadmap).
- `DELETE /api/v1/ideas/:id` → **admin** OU auteur : supprime (et ses votes/commentaires).
- `DELETE /api/v1/comments/:id` → **admin** OU auteur du commentaire.

**Doc de design**
- `GET /api/v1/design/:slug` → `{title,markdown,updatedAt,updatedBy}` (défaut slug `ff-remixed-pve`).
- `PUT /api/v1/design/:slug` `{title,markdown}` → **admin** : enregistre le markdown (upsert).
- `GET  /api/v1/design/:slug/comments` → commentaires (target_type='design', target_id = slug ou `slug#ancre`).
- `POST /api/v1/design/:slug/comments` `{anchor,body}` → commentaire (anchor = id de section, optionnel).

Réponses JSON style existant. 403 si non connecté (le middleware s'en charge), 403 explicite si action
admin demandée par un non-admin.

---

## 3) Pages front (static/idees.html + static/design.html)
Reprendre l'entête/nav/`theme.css` de `soldier.html`. Libs déjà dispo dans `static/cdn/` (Bootstrap,
bootstrap-icons) ; pour le rendu markdown de /design, ajouter `marked` (petit, à mettre dans `static/cdn/`)
et **assainir** le HTML rendu (DOMPurify) avant injection.

**idees.html**
- Barre d'outils : filtre par statut + catégorie, tri (votes / récents), bouton "＋ Proposer".
- Liste de cartes idée : compteur de votes cliquable (toggle via `/vote`), titre, catégorie, statut
  (pastille colorée : open/planned/in_progress/done/declined), auteur, date. Clic → détail + fil de commentaires.
- Formulaire de création (modal) : titre, catégorie (liste = les piliers du GAME_DESIGN.md), corps.
- Vue admin : sur chaque idée, un sélecteur de statut (PATCH) + suppression.
- Utilise `/api/v1/users/me` pour connaître user courant + `account_type` (afficher les contrôles admin).

**design.html**
- Charge `GET /api/v1/design/ff-remixed-pve` → rend le markdown (marked+DOMPurify) dans la DA.
- Chaque section (titre `##`) reçoit une ancre → bouton "💬 commenter" ouvrant le fil de la section.
- Fil de commentaires (global + par ancre) via les endpoints design/comments.
- Si admin : bouton "✏️ Éditer" → textarea markdown → `PUT` (aperçu live optionnel).
- **Amorçage du contenu** : pas de seed en code. L'admin (Benji) ouvre /design, clique Éditer, colle le
  contenu de `GAME_DESIGN.md`, enregistre. (Le doc vit alors en base, éditable.)

---

## 4) Lien dans la navigation
Ajouter une entrée **"💡 Design / Idées"** dans la navbar commune (index/map/dashboard/soldier) vers
`/idees` et `/design`.

---

## 5) Cutover (retirer HedgeDoc) — à faire APRÈS mise en ligne des pages natives
Aujourd'hui le Caddyfile de la VM détourne `/idees` et `/design` vers des conteneurs externes. Quand
les pages natives sont servies par l'app Go, remettre le bloc arma à sa forme simple pour que l'app
serve tout :
```caddy
arma.collectifxxl.fr {
    reverse_proxy gtg-livemap-app:8080
}
```
Puis : `docker exec caddy caddy reload --config /etc/caddy/Caddyfile --adapter caddyfile`, et
`cd ~/collectif-collab && docker compose down` (arrêt de HedgeDoc + sa base). Backups du Caddyfile :
`~/caddy/Caddyfile.bak-collab*`. Contexte complet côté mémoire projet : `collab-tools-collectifxxl`.

---

## Notes / limites
- Pas de co-édition temps réel du doc (contrairement à HedgeDoc) : l'édition /design est admin-only,
  la collaboration se fait via les **commentaires** + la **boîte à idées**. C'est le compromis choisi
  pour avoir login unique + DA parfaite.
- Réutiliser le filtre de modération existant sur tout texte utilisateur.
- Rester cohérent avec le style des handlers/pages existants (Gin, GORM, gestion d'erreurs, i18n FR).
