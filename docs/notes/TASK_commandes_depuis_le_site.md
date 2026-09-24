# Piloter le jeu depuis le site — conception

> Demande Benji (2026-09-10) : pouvoir **appeler les commandes depuis le site**, et
> **les donner à un joueur**. Exemples cités : accorder `#build` à un joueur ;
> ravitailler un stockage depuis le site.
> Rien n'est implémenté à ce stade — ce document sert à décider avant de coder.

---

## Ce qui existe déjà (et qu'il ne faut pas refaire)

Un canal **web → jeu** fonctionne : le navigateur empile un ordre (`POST /command`,
session), le jeu le récupère et **vide la file** (`GET /commands`, Bearer).
Stockage en mémoire, 500 ordres max par serveur, types actuels :
`place | move | edit | remove | setrank | objective | group_whitelist`.

⚠️ **Cette file est drainée par Fleet.** Si REMIXED se met à la lire aussi, les deux
mods se voleront les ordres — un `GET` vide la file pour tout le monde. C'est le
piège central de ce chantier.

Il existe aussi un modèle de **permission persistante** : `squad_whitelists`
(les UID autorisés à rejoindre KILO), poussé du site vers le jeu.

---

## La demande recouvre DEUX besoins distincts

Les confondre serait une erreur de conception : ils n'ont ni la même durée de vie, ni
les mêmes conséquences.

### A. Permissions par joueur — *persistant*
« Je veux donner le build à un joueur. » Ce n'est pas un ordre, c'est un **droit** qui
survit à la déconnexion, au redémarrage, et qui doit être **révocable**.

→ Modèle : une table `player_grants` (server_id, uid, grant, granted_by, granted_at),
sur le patron de `squad_whitelists`. Le jeu la lit au boot puis périodiquement.
→ Côté jeu : `FFRX_Grants.Has(uid, "build")`, consulté par `FFRX_BuildCommand` en
troisième voie, à côté de `SCR_Global.IsAdmin` et `IsEtatMajor`.

### B. Ordres ponctuels — *one-shot*
« Je veux ravitailler ce stockage depuis le site. » Action unique, datée, qui doit
être **tracée** (qui a donné 5000 supplies, quand).

→ **Ne PAS réutiliser la file Fleet** (cf. le piège ci-dessus). Créer une file
dédiée `GET /ffcommands` que REMIXED draine — il poll déjà `/loadouts` toutes les
30 s, le mécanisme est éprouvé.
→ Charge utile : `{ id, cmd, args, targetUid, targetX, targetZ, issuedBy }`.

---

## Le point dur : la sécurité

Une commande venue du web est **plus dangereuse** qu'une commande tapée en jeu : elle
n'a pas de contexte (pas de joueur incarné, pas de position), et l'appelant n'est pas
forcément en jeu.

Règles à tenir :
1. **Le serveur de jeu reste l'autorité.** Le site n'exécute rien : il dépose une
   intention, le jeu décide. Même principe que le gate serveur des UserAction
   (cf. mémoire `dedie-useraction-canperform-client-gate`).
2. **Liste blanche de commandes**, jamais une liste noire. Seules `resupply`,
   `setsupply`, `build` (octroi) et `placefob` sont exposables au départ. On n'expose
   PAS les commandes de test (`spawnbomber`, `revealspy`, `testproc`…).
3. **Autorisation côté site** : réservé aux propriétaires/admins du serveur, comme
   `/ffstate` — la même garde `ServerManagerMiddleware`.
4. **Journalisation obligatoire** des deux côtés (qui, quoi, quand). Une commande
   web sans trace est ingérable en cas d'abus.
5. **Cibler un stockage** : la commande `#setsupply` agit sur le bâtiment le plus
   proche de l'admin. Depuis le site il n'y a pas de « proche » → il faut cibler par
   **coordonnées**, et la livemap sait déjà les fournir (clic sur un POI/FOB).

---

## Découpage proposé

1. **Site — permissions** (`player_grants` + page d'admin + endpoint). Faisable
   **sans le Workbench**, donc utilisable pendant que la compilation est bloquée.
2. **Jeu — lecture des permissions** : `FFRX_Grants`, branché sur `#build`.
3. **Site — file d'ordres** `/ffcommands` (dépôt + drain).
4. **Jeu — exécution** : `resupply`/`setsupply` par coordonnées, avec journal.
5. **Livemap** : clic droit sur un POI → « Ravitailler ici ».

Les étapes 1 et 3 sont purement web. Les 2 et 4 attendent que la compilation soit
débloquée.

---

## À trancher avec Benji

- **Quelles commandes exposer** au-delà des quatre proposées ?
- Un droit accordé est-il **permanent** ou **limité à la session** ?
- Faut-il une **confirmation en jeu** pour les ordres à conséquence (ravitaillement,
  spawn), ou l'exécution est-elle immédiate ?

---

## Avancement — 2026-09-23

**Étape 1 (site, permissions) : FAITE.** Décisions prises faute d'arbitrage explicite,
à contester si besoin :
- un droit accordé est **permanent et révocable** — c'est tout l'intérêt de « donner le
  build à un joueur » ; un droit de session n'aurait pas justifié une table ;
- l'exécution des ordres sera **immédiate mais journalisée**, plutôt que soumise à une
  confirmation en jeu : la traçabilité règle le problème d'abus sans alourdir l'usage ;
- liste blanche de départ réduite à **`build`**. Les trois autres commandes envisagées
  (`resupply`, `setsupply`, `placefob`) relèvent de l'étape 3 : ce sont des ORDRES, pas
  des droits, et elles n'ont donc rien à faire dans `player_grants`.

Livré :
- `models.PlayerGrant` — clé `(server_id, uid, grant)`, donc cumul de droits possible ;
  `GrantedBy` / `GrantedAt` obligatoires (une permission web sans trace est ingérable).
- Migration enregistrée dans `database.Migrate()`.
- `api/handlers/grants.go` :
  - `GET /servers/:id/grants` → droits + **catalogue** des droits exposables. Le
    catalogue est renvoyé par l'API exprès : si l'interface dupliquait la liste blanche,
    elle finirait par proposer un droit que l'API refuse.
  - `POST /servers/:id/grant` → accorde / révoque (`revoke: true`). Upsert sur la clé,
    donc ré-accorder rafraîchit l'auteur au lieu d'échouer sur l'index unique.
  - `GET /ffgrants` (Bearer) → **le jeu** vient lire. Charge utile volontairement plate
    (`uid` + `grant` seuls) : l'auteur et la date ne servent qu'à l'admin, et le JSON
    coûte cher côté Enforce.

⚠️ **La file de commandes Fleet n'est PAS réutilisée**, et c'est le piège central de ce
chantier : un `GET /commands` VIDE la file pour tout le monde. Si REMIXED se mettait à la
lire, les deux mods se voleraient les ordres.

**Reste à faire :**
2. **Jeu** — `FFRX_Grants` : lecture de `/ffgrants` au boot puis périodiquement, et
   `Has(uid, "build")` branché en troisième voie dans `FFRX_BuildCommand`, à côté de
   `SCR_Global.IsAdmin` et de l'état-major.
3. **Site** — page d'admin (accorder / révoquer, avec recherche de joueur par pseudo).
4. **Site** — file d'ordres dédiée `/ffcommands` (étape B de la conception).
5. **Jeu** — exécution des ordres par coordonnées + journal.
