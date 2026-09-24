# TASK_FFSTATE_SENDER — Émettre l'état live FF + Réoccupation vers la livemap admin

## But
Envoyer périodiquement (≈5 s) un snapshot de l'état Freedom Fighters + Réoccupation à la web-app
GTG-livemap, pour une **livemap admin** qui montre : POIs capturables (villes/bases/usines/tours
radio) colorés par faction + progression de capture, batailles en cours, **contre-attaques
planifiées** (cible + timing + recon), **patrouilles**, **recon** (position/route), couverture radio.

## ✅ Côté WEB : DÉJÀ FAIT (par le Claude web, en ligne)
- `POST /api/v1/ffstate` (Bearer = apiKey, `server_id` déduit de la clé) → stocke le snapshot verbatim.
- `GET /api/v1/servers/:id/ffstate` (session + owner/admin **uniquement** — PAS public, révèle les
  positions ennemies) → renvoie le dernier snapshot.
- Le blob JSON est stocké **verbatim** : tu peux faire évoluer le schéma sans changer le Go.
- Reste à faire côté web : le **rendu dans map.html** (couche admin) — voir §Rendu.

## Architecture recommandée (où mettre le code)
⚠️ **Fleet ne dépend PAS de FF** (deps = base `58D0FB3206B6F859` + Anarchy `69A510CE600D1126`).
Le mettre dans Fleet le **couplerait à FF** (or Fleet est générique). **Cet addon (FF - REMIXED - PVE)
dépend DÉJÀ de Réoccupation `69A47272BCF14AD5`** (qui tire FF core `CAFEBEEFF0CACC1A`) → **c'est ici
qu'il faut coder le lecteur + l'émetteur**. On réutilise juste le MÊME transport que Fleet (RestApi
POST + Bearer) et la MÊME config `$profile:Fleet/GTG.json` (url/apiKey), en dérivant l'URL `ffstate`.

## Convention de coordonnées
Monde en **mètres**, **x = est, z = nord** (identique à ce que Fleet envoie pour positions/markers ;
la carte web est calibrée là-dessus). Toujours `entity.GetOrigin()` → prendre `.x` et `.z`.

---

## 1) Lire l'état FF/Réoccupation (hooks)
> ⚠️ Les signatures marquées **(à vérifier)** viennent d'une extraction de sources ; confirmer avec
> `api_search`/Shift+F7 avant de t'appuyer dessus. Celles marquées **(sûr)** viennent du cheat-sheet FF.

### POIs capturables
- Énumérer : `JWK_IndexSystem.Get().GetAll(JWK_TownEntity)` puis idem `JWK_MilitaryBaseEntity`,
  `JWK_FactoryEntity`, `JWK_RadioSiteEntity`, `JWK_CheckpointEntity` **(GetAll = sûr ; noms de types = à vérifier)**.
- Entité → `IEntity e = GetGame().GetWorld().FindEntityByID(id)` ; `vector p = e.GetOrigin()`.
- Faction/rôle : `JWK_FactionControlComponent fcc = JWK_CompTU<JWK_FactionControlComponent>.FindIn(e)`
  → `fcc.GetFactionRole()` → `JWK_EFactionRole` (UNDEFINED=0,NONE=1,PLAYER=2,ENEMY=3,SUPPORTING=4,AMBIENT=5) **(enum = sûr)**.
- Nom : `JWK_NamedLocationComponent.FindIn(e).GetName()` **(à vérifier)**.
- **Alternative territoire (sûr)** : `JWK.GetTerritoryControl().GetNodeAt(pos)` →
  `JWK_TerritoryControlNodeComponent` (`GetFactionRole()`, `IsBorder()`, `GetOwner().GetOrigin()`).
  Utile pour la notion de **frontline** (`IsBorder()`).
- **Progression de capture** : chercher côté `*Controller` / node un champ % ou état « en capture ».
  Si introuvable, se contenter du rôle propriétaire (capture = 0/1) en v1.

### Batailles en cours
- `JWK_BattleManagerComponent` : `IsBattleActive()`, `GetController()` → `JWK_BattleControllerEntity`
  (`GetSubject_S()`, `GetAttackingFaction_S()`), `GetPoints()` (0–200 : déploiement→domination),
  position de bataille **(à vérifier l'accès exact — via GetInstance()/JWK.*)**.

### Réoccupation (contre-attaques / patrouilles / recon)
- Singleton : `FF_ReoccupationManager.Get()` **(à vérifier)**.
- File de contre-attaques `m_aQueue` (array de `FF_ReoccupationEntry`) — **protected** → exposer via
  **modded class** (voir §2). Champs de `FF_ReoccupationEntry` (publics d'après extraction) :
  `m_Subject` (cible → `.GetOwner().GetOrigin()`), `m_iTriggerTime`, `m_iReconState`
  (0 DISABLED,1 WAITING,2 SPAWN_REQ,3 ACTIVE,4 NEUTRALIZED,5 SUCCESS,6 CANCELLED),
  `m_vReconLastKnownPosition`, `m_vReconTargetPosition`, `m_aReconRoute` (array<vector>),
  `m_fReconAttackStrengthBonus`, phases artillerie (`m_bArtillery*`).
- Patrouilles : `FF_PatrolManager` (via le manager), liste `m_aPatrols` de `FF_PatrolTask` —
  **protected** → modded getter. Champs `FF_PatrolTask` : `m_iState` (1 SPAWN_REQ,2 ACTIVE,3 RETURNING,
  4 FINISHED,5 FAILED), `m_vLastKnownPosition`, `m_vStartPosition`, `m_vTargetPosition`, `m_Route`.

### Tours radio (intel)
- `JWK_RadioSiteEntity` : `IsOperable()`, `GetFactionControl()`, `m_iSignalRange` **(à vérifier)**.

## 2) Modded classes pour exposer les collections protected
```enfusion
modded class FF_ReoccupationManager {
    array<ref FF_ReoccupationEntry> FFR_GetQueue() { return m_aQueue; }
    FF_PatrolManager FFR_GetPatrolManager() { return m_PatrolManager; } // adapter au vrai nom du champ
}
modded class FF_PatrolManager {
    array<ref FF_PatrolTask> FFR_GetPatrols() { return m_aPatrols; } // adapter au vrai nom
}
```
(Vérifier les noms EXACTS des champs protected via l'extraction / Shift+F7.)

## 3) Émetteur (calqué sur Flt_GTGPositions)
Créer un `GameSystem`/singleton côté serveur (`Replication.IsServer()`), tick `CallLater(..., 5000, true)` :
- Lire la config Fleet `$profile:Fleet/GTG.json` (`url`, `apiKey`). Dériver l'URL : remplacer le
  suffixe `positions` par `ffstate` (même logique que Fleet `DeriveUrl`).
- POST :
  ```enfusion
  RestApi api = GetGame().GetRestApi();
  RestContext rc = api.GetContext(ffstateUrl);
  rc.SetHeaders(string.Format("Authorization, Bearer %1", apiKey));
  rc.POST(callback, "", body);   // callback = RestCallback avec SetOnSuccess/SetOnError (PAS override OnSuccess)
  ```
  (⚠️ toujours `SetOnSuccess()/SetOnError()` dans le constructeur, jamais override — sinon spam natif ;
  cf. mémoire projet [[restcallback-onsuccess-warning-benign]].)

## 4) Contrat JSON à POSTER (corps de /ffstate)
Coords monde (x=est, z=nord). `role` = string du `JWK_EFactionRole`. Champs manquants = omettre.
```json
{
  "ts": 1783420005,
  "summary": { "playerPct": 42.0, "enemyPct": 51.0, "phase": "" },
  "pois": [
    { "id": "town_12", "type": "town", "name": "Montignac", "x": 4520, "z": 8110,
      "role": "PLAYER", "subtype": "CITY", "capture": 0.0, "border": true }
  ],
  "battles": [
    { "x": 4600, "z": 8200, "attacker": "USSR", "points": 120, "name": "Alpha Base", "state": "active" }
  ],
  "counterattacks": [
    { "targetName": "Factory-2", "targetX": 4700, "targetZ": 8300, "state": "artillery",
      "triggerIn": 320, "strength": 1.8,
      "recon": { "state": 3, "x": 4650, "z": 8250, "route": [[4650,8250],[4680,8280]] } }
  ],
  "patrols": [
    { "x": 4400, "z": 8000, "state": "active", "fromX": 4300, "fromZ": 7900,
      "targetX": 4520, "targetZ": 8110, "route": [[4400,8000],[4450,8050]] }
  ],
  "radio": [
    { "x": 4500, "z": 8100, "radius": 3500, "role": "ENEMY", "operable": true }
  ]
}
```
`type` ∈ town|base|factory|radio|checkpoint. `state` contre-attaque ∈ scheduled|recon|artillery|imminent
(libre). Recon `state` = l'entier `m_iReconState`. Tout est optionnel sauf `pois`.

## 5) Rendu (map.html) — fait ensuite par le Claude web
Couche « FF admin » (toggle, visible aux owner/admin) fetchée dans la boucle live via
`GET /api/v1/servers/:id/ffstate` : POIs colorés par rôle (bleu=PLAYER, rouge=ENEMY, gris=NONE) + anneau
de capture + label ; batailles = cercle pulsant + barre de points ; contre-attaques = marqueur cible +
compte à rebours + icône/route recon ; patrouilles = marqueur + route ; radio = cercles. Développable
contre un faux snapshot en attendant le jeu.

## Vérification
Benji compile (Shift+F7) et lance une partie ; lire le contenu réel via l'admin (ou `curl` authentifié
`GET /api/v1/servers/<id>/ffstate`). Corriger les signatures FF non confirmées au fur et à mesure.
