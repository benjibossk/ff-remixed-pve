# Tâche — colorer les marqueurs joueurs par escouade

Fleet (le mod) envoie maintenant un champ **`color`** (int **ARGB**, même convention que
`Marker.Color`, peut être négatif) dans chaque escouade de `POST /api/v1/squads`.
Cf. `FLEET_GTG_CONTRACT.md` §1.3. But : **le marqueur de chaque joueur sur la carte prend la
couleur de SON escouade** (repli sur la couleur de faction si pas d'escouade / pas de couleur).

Tout le nécessaire existe déjà côté frontend (`squadByGuid`, `fltArgbToCss`). 2 changements.

---

## 1. Go — `api/handlers/squads.go`
Ajouter le champ `Color` au struct `Squad` (le store le repasse tel quel au GET, rien d'autre à faire) :

```go
type Squad struct {
	ID       int      `json:"id"`
	Faction  string   `json:"faction"`
	Name     string   `json:"name"`
	Callsign string   `json:"callsign"`
	Leader   string   `json:"leader"`
	Count    int      `json:"count"`
	Members  []string `json:"members"`
	Color    int64    `json:"color"` // ARGB (peut être négatif) — couleur d'escouade
}
```
→ `docker compose up --build -d` (changement Go).

## 2. Frontend — `static/map.html`, fonction `renderPositions` (~ligne 672)
Remplacer la ligne :
```js
const color = getFactionColor(p.faction);
```
par (jointure UID → escouade, repli faction) :
```js
const sq = squadByGuid.get(p.playerGuid);
const color = (sq && sq.color) ? fltArgbToCss(sq.color) : getFactionColor(p.faction);
```
`squadByGuid` (rempli dans `setSquads`) et `fltArgbToCss(v)` (ARGB int → `rgba(...)`) existent déjà.
Le reste du bloc (SVG `fill="${color}"`) est inchangé.

### (optionnel) panneau « UNITÉS »
Dans `updatePlayerList`, pour la ligne d'en-tête d'escouade (`.squad-head`), tu peux teinter le
point/entête avec `fltArgbToCss(sq.color)` pour un rappel visuel de la couleur.

---

## Notes
- La couleur ne s'applique **qu'en mode LIVE** (les escouades ne sont récupérées qu'en live ;
  en relecture, `setSquads([])` → repli automatique sur la couleur de faction). C'est OK.
- Aucune migration DB : squads sont en mémoire.
- Test : avec Fleet qui tourne, 2 joueurs dans « Alpha » → même couleur d'escouade ; un joueur
  sans groupe → couleur de faction.
