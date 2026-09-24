# Tâche — RP radio / brouillard de guerre sur la livemap

Fleet envoie maintenant, par joueur (dans `/api/v1/positions`), 2 champs :
- **`radio`** (0-3) : couverture radio. `0`=NONE (hors portée), `1`=RECEIVE, `2`=SEND,
  `3`=BOTH_WAYS (liaison HQ complète). Tient déjà compte des **relais** construits en jeu.
- **`lastSeen`** (Unix secondes) : heure de la position rapportée.

⚠️ **Position figée** : si `radio < 2` (le HQ ne reçoit pas la position live), Fleet envoie la
**dernière position connue AVEC radio** (pas la position live). Donc côté web on n'a rien à
recalculer — on affiche juste la position reçue avec un rendu qui reflète l'ancienneté.

## Go — positions.go
Ajouter au struct de payload interne + à la réponse live (comme `rank`/`xp`) :
```go
Radio    int   `json:"radio"`
LastSeen int64 `json:"lastSeen"`
```
(pass-through, pas de persistance — c'est du live.)

## Frontend map.html — rendu du marqueur joueur
Dans `renderPositions`, calculer une **opacité** selon la couverture + l'ancienneté :
```js
function radioOpacity(p){
  if (p.radio >= 2) return 1.0;                 // liaison HQ -> plein
  // hors liaison : on estompe avec le temps depuis la dernière position connue
  const age = (Date.now()/1000) - (p.lastSeen || 0);   // secondes
  const FADE_START = 30;    // s : commence à pâlir
  const FADE_END   = 180;   // s : disparaît
  if (age <= FADE_START) return (p.radio === 1) ? 0.7 : 0.6;  // RECEIVE un peu plus visible
  if (age >= FADE_END)   return 0.0;            // disparu (fog)
  const t = (age - FADE_START) / (FADE_END - FADE_START);
  return 0.6 * (1 - t);                         // fondu linéaire
}
```
Appliquer l'opacité au marqueur (`icon` divIcon → `style="opacity:..."` sur le SVG, ou
`marker.setOpacity(...)`). Si opacité 0 → ne pas ajouter le marqueur (ou le retirer).

Suggestions visuelles (optionnel mais RP+) :
- Petit **icône radio** / barres de signal à côté du nom selon `radio` (0 barré, 1-2 partiel, 3 plein).
- Les joueurs figés (`radio<2`) : bordure pointillée ou libellé « dernière position ».
- Dans le panneau UNITÉS : trier/marquer « hors radio » les joueurs `radio<2`.

## Phase 2 (LIVRÉE côté mod) — cercle de portée (fog of war visuel)
Fleet envoie maintenant `POST /api/v1/coverage` : **un cercle par faction**, centré sur le HQ,
rayon = somme des portées radio des bases (HQ + relais) → **grandit à chaque relais construit**.
```json
{ "coverage": [ { "faction":"US", "x":4520, "z":8110, "radius":3500 } ] }
```
**Go** : endpoint `POST /api/v1/coverage` (Bearer, in-mem par serveur) + `GET /api/v1/public/coverage/:sid`.
**Frontend** : `L.circle(armaToLeaflet([x,z]), { radius, color, fillOpacity })` pour la faction
affichée (fetch en live comme squads). Effet fog : soit un cercle clair sur fond assombri, soit
un masque sombre percé au niveau du cercle. Rayon en mètres monde.

## Notes
- `radio>=2` = position **live** ; `radio<2` = position **figée** (dernière connue) + fade.
- Seulement en mode **LIVE**.
- Latence : `radio`/position s'actualisent au rythme du push (~5 s).
