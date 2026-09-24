# Contrat Fleet ⇄ GTG-livemap — carte tactique unique

But : GTG (port 8080) devient la **seule** carte tactique. Le mod **Fleet** (serveur de jeu)
pousse l'état et applique les ordres. Ce fichier fige les **endpoints, le JSON et les
coordonnées** pour que le côté jeu (Fleet) et le côté web (GTG Go + frontend) restent alignés.

- **Auth côté jeu** : header `Authorization: Bearer <apiKey>` (clé API du serveur GTG).
  `server_id` est déduit de la clé côté GTG (comme pour `/positions` aujourd'hui).
- **Coordonnées** : **mètres monde**, `x` = X (est), `z`/`y` = Z (nord). **Même espace que
  `absolutePosX/absolutePosZ` des positions joueurs.** Le frontend place marqueurs, dessins
  et joueurs avec la **même calibration de carte** (celle déjà utilisée pour les positions).
  → Ne PAS mélanger avec les coords relatives [0,1].

---

## 1. JEU → GTG  (Fleet POST, Bearer)

### 1.1 `POST /api/v1/positions`  *(existant, inchangé)*

### 1.2 `POST /api/v1/markers`  *(existant — struct à ÉTENDRE)*
```json
{ "markers": [
  { "id":12, "x":4520, "z":8110,
    "kind":1, "icon":0, "ident":6, "dim":11, "sym":27,
    "color":-16711936, "vis":3, "rot":0, "size":200, "text":"HQ",
    "author":"Ben", "created":1783420000 } ]
}
```
`author` = nom du poseur (dernier éditeur ; "Web" si posé depuis le site). `created` = **Unix (s)**
de la 1re apparition côté Fleet = date de pose → afficher "posé par `author` le `created`".
`ownerUid` = **UID** du poseur (résolu ; pour un marqueur posé depuis le site, l'`uid` de la
commande `place`). → le web attribue la **propriété** par UID (« le joueur bouge ses propres points »).
Pour poser un marqueur attribué depuis le site : mettre `uid` dans la commande `place`.
⚠️ **Go à faire** : ajouter `icon, ident, dim, sym` au struct `Marker` (handlers/markers.go).
Ce sont eux qui donnent la **même tête** qu'en jeu (icône + symbole APP‑6). Sans eux → marqueur générique.

Champs :
| champ | sens |
|---|---|
| `id` | id serveur du marqueur |
| `x,z` | position monde (mètres) : x=est, z=nord |
| `kind` | 0 = civil, 1 = militaire |
| `icon` | entrée d'icône (marqueurs civils/custom) |
| `ident` | identité APP‑6 (ami/ennemi/neutre/inconnu) |
| `dim` | dimension (sol/air/installation) |
| `sym` | symbole (infanterie/blindé/…) |
| `color` | ARGB (int signé possible) |
| `vis` | 0 local, 1 groupe, 2 camp, 3 tous |
| `rot` | rotation (deg) |
| `size` | taille |
| `text` | libellé |

### 1.3 `POST /api/v1/squads`  *(NOUVEAU — escouades / groupes joueurs)*
Pour le dashboard : afficher les **escouades** et imbriquer les joueurs dedans.
```json
{ "squads": [
  { "id":12, "faction":"US", "name":"Atlas White 1", "callsign":"1",
    "leader":"<uid>", "leaderName":"Ben", "count":3,
    "members":["<uid1>","<uid2>","<uid3>"], "color":-65536,
    "role":"Assaut", "desc":"texte libre" }
] }
```
- `name` = nom COMPLET traduit ("Atlas White 1"). `callsign` = n° court ("1").
- `leaderName` = nom du chef (en plus de `leader` = son UID).
- `role` = spécialité traduite (Assaut, Reconnaissance, Antichar…). `desc` = description du groupe.
- `members` / `leader` = **UID Reforger** (même clé que `playerGuid` des positions) → le
  frontend joint chaque joueur à son escouade sans ambiguïté.
- `color` = **ARGB** (même convention que la couleur des marqueurs). **→ le frontend colore
  le marqueur de chaque joueur avec la couleur de SON escouade** (join par UID). Résolution
  côté jeu : couleur forcée (preset) > table `nom→couleur` > palette auto par escouade.
- Seuls les groupes avec **≥ 1 joueur** sont envoyés. `name` = nom custom, sinon callsign.
- État complet à chaque tick (remplace l'état précédent, comme les marqueurs).

### 1.3bis — Grades (détectés en jeu)
**Rang par joueur** : le push positions (`POST /api/v1/positions`) inclut désormais, dans le
JSON interne de chaque joueur : `"rank":<int>` (valeur `SCR_ECharacterRank`, ex. 3=Sergeant),
`"xp":<int>` (XP courant), et `"factionKey":"US"` (la **clé** de faction, pour joindre le
catalogue grades — `factionName` reste le nom d'affichage, différent de la clé).

**Radio / brouillard de guerre (RP)** : chaque joueur porte aussi `"radio":<0-3>` (couverture :
0=NONE hors portée, 1=RECEIVE, 2=SEND, 3=BOTH_WAYS liaison HQ complète) et `"lastSeen":<Unix s>`
(heure de la position rapportée). ⚠️ **Si `radio < 2` (le HQ ne reçoit pas), la position envoyée
est FIGÉE à la dernière position connue avec radio** (pas la position live). → le web : joueur
`radio>=2` = net/live ; `radio<2` = position figée qui **s'estompe avec l'ancienneté**
(`now - lastSeen`) jusqu'à disparaître. Voir `TASK_WEB_RADIO_FOG.md`.
→ **Go TODO** : ajouter `rank`/`xp`/`factionKey` au struct `models.PositionPayload` + les exposer
sur la réponse live ; le nom lisible du grade se résout `catalog[factionKey] → i==rank → name`.

**Liste des grades dispo** : `POST /api/v1/ranks` *(NOUVEAU)* — statique par scénario, envoyée
**une seule fois par reboot** (Fleet ré-essaie jusqu'à un 200, puis arrête). Le rang de CHAQUE
joueur, lui, part à **chaque tick** dans les positions (pour la synchro continue).
Format = **map par faction** (aligné sur `ranks.go` côté GTG) :
```json
{ "ranks": {
  "US":   [ {"i":0,"name":"Soldat"}, {"i":3,"name":"Sergent"} ],
  "USSR": [ {"i":0,"name":"..."} ]
} }
```
- `i` = valeur enum `SCR_ECharacterRank` (**la clé stable**, celle du `setrank`). `name` = nom résolu.
- Sert : (a) afficher le nom du grade d'un joueur (join `faction`+`rank`), (b) alimenter le menu
  déroulant du `setrank` (grades valides filtrés par faction).

### 1.3ter — Couverture radio (fog of war) : `POST /api/v1/coverage` *(NOUVEAU)*
Un cercle de portée radio par faction, centré sur le HQ, rayon = somme des portées des bases
radio (HQ + relais) → **grandit à chaque relais construit**. Envoyé à chaque tick.
```json
{ "coverage": [ { "faction":"US", "x":4520, "z":8110, "radius":3500 } ] }
```
- `x,z` = position du HQ (mètres monde). `radius` = mètres. → le web dessine un **cercle**
  (clair dedans / fog dehors), filtré par la faction affichée.

### 1.4 `POST /api/v1/drawings`  *(NOUVEAU)*
```json
{ "drawings": [
  { "id":3, "owner":-1, "color":-1, "w":1, "vis":3, "ch":-1, "fill":0,
    "author":"Web", "pts":[4520,8110, 4600,8200, 4700,8150] }
] }
```
`pts` = paires `x,z` **à plat** (mètres monde). `w` = index d'épaisseur, `fill` = rempli (0/1).

---

## 2. GTG → JEU  (Fleet GET, Bearer) — les ordres du navigateur

### 2.1 `GET /api/v1/commands`  *(NOUVEAU)*
Fleet appelle cet endpoint à chaque tick. GTG renvoie la file d'ordres du serveur **et la VIDE** :
```json
{ "commands": [
  { "cmdId":42, "type":"place", "id":0,
    "x":4520, "y":8110, "kind":1, "icon":0, "ident":6, "dim":11, "sym":27,
    "color":-16711936, "rot":0, "size":200, "vis":3, "ch":-1, "text":"HQ",
    "uid":"", "rank":0 }
] }
```
⚠️ **`y` = Z monde (nord)** (pas l'altitude) — c'est la 2e coord planaire. x=est, y=nord.

`type` ∈ :
| type | effet en jeu | champs utiles |
|---|---|---|
| `place`  | crée un marqueur | x,y,kind,icon,ident,dim,sym,color,rot,size,vis,ch,text |
| `move`   | déplace | id, x, y |
| `edit`   | édite | id + tous les champs |
| `remove` | supprime | id |
| `setrank`| grade fixe Discord | uid, rank |
| `objective`| crée un objectif (tâche J) assigné | objtarget, objname, objdesc, x, y + (uid \| id \| faction) |

**`objective`** : crée une tâche qui apparaît dans le **menu J** du/des joueur(s).
`objtarget` = `"player"` (→ `uid`) \| `"squad"` (→ `id` = id d'escouade) \| `"faction"` (→ `faction` = clé).
`objname`/`objdesc` = titre/description. `x,y` = position monde (mètres ; x=est, y=Z nord ; 0,0 = sans position).

Si la file est vide → `{ "commands": [] }` (ou 204). Fleet ignore silencieusement.

### 2.2 `POST /api/v1/command`  *(NOUVEAU — appelé par le NAVIGATEUR)*
Le frontend empile un ordre (auth session/utilisateur, **pas** la clé jeu). GTG l'ajoute à la
file du `server_id` visé. Corps = un objet ordre **à plat** (mêmes champs que 2.1, sans `cmdId`
que GTG attribue). Garde‑fou : file bornée (ex. 500), plus vieux supprimé.

---

## 3. Phase 2 — grades (déjà prêt côté jeu, à brancher ensuite)
- Fleet peut publier la **liste des grades dispo par faction** + par joueur `uid/rank/xp`
  (déjà codé dans `Flt_MarkerBridge`). À ajouter au push GTG quand on fera l'UI grades.
- `setrank` (2.1) fonctionne déjà côté jeu : registre persistant `$profile:Fleet/Ranks.json`,
  application immédiate si le joueur est en ligne, sinon au prochain spawn. L'XP ne change
  plus jamais le grade (grade 100 % manuel).

---

## 4. Récap côté jeu (Fleet) — déjà implémenté
- `Flt_GTGPositions.c` : POST markers (complets) + POST drawings + GET commands (pull).
- Application des ordres : `Flt_MarkerBridge.ApplyCommandsJson` (place/move/edit/remove/setrank).
- Config : `$profile:Fleet/GTG.json` → `url, markersUrl, drawingsUrl, commandsUrl, apiKey, intervalSec`
  (les 3 URL sont auto‑déduites de `url` si laissées vides : `/positions` → `/markers|/drawings|/commands`).
