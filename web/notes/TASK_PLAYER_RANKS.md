# Tâche — grades des joueurs (catalogue + affichage par joueur)

Le mod **Fleet** pousse maintenant les grades. Deux flux (cf. `FLEET_GTG_CONTRACT.md` §1.3bis) :

1. **Catalogue des grades** (`POST /api/v1/ranks`) — 1× par reboot, **déjà géré par `ranks.go`**.
   Fleet envoie EXACTEMENT le format attendu :
   ```json
   { "ranks": { "US":[{"i":0,"name":"Soldat"}], "USSR":[{"i":0,"name":"..."}] } }
   ```
   → **Rien à changer côté Go**, ça remplit déjà le menu setrank de l'admin. **À vérifier** : dès
   que Fleet tourne, le catalogue arrive → le dropdown « Effectifs & Grades » n'est plus vide.

2. **Rang + XP par joueur** (dans `POST /api/v1/positions`, à chaque tick) — **NOUVEAU, à brancher.**
   Le JSON interne de chaque joueur contient maintenant :
   ```json
   { "playerGuid":"...", "playerName":"...", "factionName":"...",
     "factionKey":"US", "factionColor":{...},
     "rank":3, "xp":540, "positions":[...] }
   ```

## Ce qu'il faut faire

### A. Go — capter rank/xp/factionKey
Dans `models.PositionPayload` (parse du JSON interne, `positions.go` ligne ~147), ajouter :
```go
Rank        int    `json:"rank"`
XP          int    `json:"xp"`
FactionKey  string `json:"factionKey"`
```
Puis **exposer ces valeurs sur la réponse live** que le frontend interroge (la `PositionResponse`
dans `updateLatestPositionsCache`, + le champ correspondant du cache/latest-positions) :
```go
Rank       int    `json:"rank"`
XP         int    `json:"xp"`
FactionKey string `json:"factionKey"`
```
`rank`/`xp` sont des valeurs **live** (pas besoin de les persister en DB — juste les faire passer
dans le cache des dernières positions servi au frontend). `factionKey` sert au join grades
(⚠️ `factionName` est le nom d'affichage, PAS la clé — utiliser `factionKey` pour le catalogue).

### B. Frontend — afficher le grade dans le panneau « UNITÉS » (map.html)
1. En live, récupérer le catalogue : `GET public/ranks/:serverId` (même endpoint que l'admin) →
   `{ "US":[{"i","name"}], ... }`. Le stocker (ex. `rankCatalog`).
2. Pour chaque joueur, résoudre le nom : `rankCatalog[p.factionKey]?.find(r => r.i === p.rank)?.name`.
3. L'afficher à côté du nom du joueur (liste UNITÉS et/ou tooltip du marqueur). Ex. `Sgt · Alpha`.
4. (option) afficher l'XP, et préparer la **vue « à engueuler »** : joueur dont l'XP est
   descendu sous le seuil de son grade (le seuil n'est pas encore envoyé → étape ultérieure).

## Notes
- Le `setrank` (web→jeu) marche déjà : `POST /api/v1/command {server_id,type:'setrank',uid,rank}`
  → Fleet applique et **persiste** (grade fixe, l'XP ne le change plus jamais).
- rank/xp ne sont dispo qu'en **LIVE** (comme les escouades) ; en relecture, absents → ne rien afficher.
- Join faction : toujours via `factionKey` (US/USSR), jamais via `factionName`.
