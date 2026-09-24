# FFRX — répartition du travail (2 sessions en parallèle)

Généré le 2026-09-16. **Point fait le 2026-09-18** (encadré ci-dessous).

> ## Où on en est au 18/09
>
> | | État |
> |---|---|
> | **A1** restauration tolérante | ✅ compilée |
> | **A2** code mort | ✅ fait (1719 → 1359 lignes) |
> | **A3** dotations à regénérer | ⬜ **toujours bloqué** — demande une session en jeu (~20 min, procédure écrite plus bas) |
> | **A4** camo désert | ⬜ **décision à prendre** (substitution après apply, ou dotations par théâtre) |
> | **A5** site | ✅ changelog + page Tests déployés |
> | **B1** vision nocturne | ✅ écrit (`FFRX_NightVision.c`) — bonus de perception scripté, distribution rare, commande `#nvg` |
> | **B2** gilets suicide | ✅ écrit (`FFRX_SuicideVest.c`) — charge à 70 m, détonation à 3,5 m |
> | **B3** classes de diagnostic | ⬜ **arbitrage attendu** — voir ci-dessous, la liste a changé |
> | **B4** dépendances externes | ⬜ **toujours ouvert, et plus urgent** : TacticalFlava est redevenue une dépendance |
>
> **B3 — précision importante.** La liste d'origine incluait `FFRX_SpawnCensus` : **ne pas le supprimer**, il alimente désormais la page « recensement » du site (avec son bouton de purge). Restent candidats : `FFRX_BuildDiag`, `FFRX_MenuInputDiag`, `FFRX_SpecialistSpawnDebug`, `FFRX_DroneDebug`.
>
> **Écrit depuis le 16/09, non listé dans les lots** : menace adaptative, pondération réglable des spécialistes, niveaux d'escouade, AT-4, jumelles Sophie, compat Réoccupation 6.2.0, correctifs d'arsenal et de parking Anizay.
>
> ⚠️ **Rien de tout ça n'est en jeu** : le pak du dédié date du 10/09. Compiler puis republier avant tout test.

## ⚠️ Règles communes aux deux lots

1. **Le Workbench n'est pas lancé** → `wb_compile` échoue (NET API injoignable + pas de fenêtre
   Script Editor). Tout le travail de ce matin est donc **écriture de code non compilée**.
   Ne pas conclure « ça marche » : écrire, relire, et attendre la compilation de cet après-midi.
2. **Une seule session compile à la fois** quand le Workbench sera ouvert. Deux Shift+F7
   simultanés = résultats ininterprétables.
3. **Le Script Editor compile son buffer mémoire, pas le disque.** Si un onglet FFRX est ouvert,
   les erreurs signalées peuvent être périmées. En cas d'erreur incohérente avec le fichier :
   suspecter le buffer avant de suspecter le code.
4. **Plafond de compilation** : on est juste sous la limite (45 addons, ~240 classes FFRX).
   Toute nouvelle classe compte. Préférer étendre un fichier existant à en créer un.
5. Pas de ternaire `? :`, pas de `%` sur des flottants, `out`/`owned`/`in` sont des mots réservés,
   `string.Format` perd les arguments au-delà de ~6 et tronque à 8 Ko.

---

## LOT A — ETAT AU 16/09 (matin) — tout est ECRIT mais RIEN N'EST COMPILE

| | Etat |
|---|---|
| A1 restauration tolerante | Ecrite + **verifiee ligne a ligne** contre WCS. Compile en attente. |
| A2 code mort | **Fait** : 1719 -> 1359 lignes, 2 classes recuperees. |
| A3 dotations a regenerer | Bloque (besoin de Benji en jeu). Mesure faite : 13 tenues en base. |
| A4 camo desert | **Resolu** : ce n'etait pas une perte, juste un appel perdu. Rebranche. |
| A5 site | Changelog **redige mais NON DEPLOYE** (on n'annonce pas avant le test). |

### Trouve en chemin (3 choses qui auraient casse en silence)
1. **`BuildSummary` etait devenu muet** : il parsait l'ancien format maison -> tous les
   apercus d'action seraient sortis vides, sans erreur. Reecrit sur le nouveau format.
   Le comptage de chargeurs est exact ; le nom d'arme repose sur une heuristique de chemin
   (`/Weapons/` hors `/Attachments/`) -> **a verifier en jeu sur les armes de mods**.
2. **`Publish()` tronquait a 8 Ko** (`string.Format`). Mesure reelle sur la base : la plus
   grosse tenue produit un corps POST de **7442 car. sur 8192**, soit 91 % du seuil. Le bug
   n'etait pas encore declenche, mais le nouveau format (plus verbeux) le franchissait
   des la premiere tenue fournie. Corrige en concatenation incrementale.
3. **Le theatre desert etait debranche depuis la refonte d'hier** (l'appel a `SetCurrent`
   avait disparu), pas depuis le nettoyage. Rebranche dans `ApplyJson`.

### Verifie et sain (ne pas y retoucher)
- Cote site : colonne `data` en `mediumtext` (16 Mo), POST plafonne a 256 Ko -> large.
- `JsonEsc` n'utilise pas `string.Format` -> pas de troncature.
- `FFRX_LoadoutRefund.c` est decouple de la refonte (n'appelle ni CaptureJson ni ApplyJson).
- Aucune tenue actuellement en base n'est corrompue (13/13 en JSON valide).

### A surveiller (pas une action, un risque)
`GET /loadouts` renvoie **toutes** les tenues avec leur `data`, sans filtre ni pagination,
et le jeu refetche periodiquement. Aujourd'hui 39,6 Ko au total ; au nouveau format compter
un facteur 2 a 4. Ca reste tenable, mais si le nombre de tenues perso grimpe il faudra
filtrer par faction cote site. **Ne pas changer le contrat d'API sans arbitrage de Benji.**

---

## LOT A — Tenues, arsenal, site web

**Fichiers réservés à ce lot :** `Scripts/Game/FFRX/FFRX_Loadout.c`,
`FFRX_LoadoutRefund.c`, `FFRX_ArsenalLoadoutTolerant.c`, les prefabs d'arsenal,
et **tout** `D:\Serveur\GTG-livemap` (site).

### A1. Vérifier la restauration tolérante (priorité 1)
`FFRX_ArsenalLoadoutTolerant.c` vient d'être écrit : il rend l'apply du jeu de base tolérant
aux pannes (porté de WCS Loadout Editor, qui utilise la même API base-game).
- Le relire ligne à ligne contre la référence : `C:\Users\benbo\tools\PakInspector\wcs_extract\scripts\Game\GameMode\Loadout\WCS_LoadoutEditor_PlayerArsenalLoadout.c`
- Point critique : les trois `FFRX_Skip*` doivent consommer **exactement** les données de
  l'objet sauté, sinon tout ce qui suit est lu de travers.
- Compiler dès que le Workbench est dispo.

### A2. Nettoyer le code mort de `FFRX_Loadout.c`
L'ancien chemin de reconstruction objet-par-objet n'est plus appelé. À supprimer :
`SpawnPlace`, `ClearStorage`, `FFRX_AlreadyAttached`, `FFRX_SlotOccupied`,
`FFRX_HasChildForStorage`, `CaptureStorage`, `CaptureChildren`,
`FFRX_LoadoutItem`, `FFRX_LoadoutRoot`.
Gain double : lisibilité **et** classes récupérées contre le plafond de compilation.
⚠️ Vérifier chaque suppression par une recherche d'usage avant de retirer.

### A3 — PROCEDURE (a derouler en jeu, ~20 min)
Les 9 dotations de faction en base (Fusilier, Anti-char, Eclaireur, Grenadier, Medecin,
Mitrailleur, Tireur, Genie + les tenues perso) sont a l'ANCIEN format : elles refuseront de
s'equiper. Il faut les recreer une par une.

Pour chacune :
1. A la caisse, s'equiper exactement comme la dotation doit l'etre (arme + optique, chargeurs,
   tenue, sac). Le cout en ravitaillement est calcule a partir de ce qu'on porte.
2. Action **« Enregistrer ma tenue »** -> elle part sur le site sous « Ma tenue N ».
3. Sur le site, la **renommer** avec le nom de la dotation et **vider le champ proprietaire**
   pour en faire une dotation de faction (une tenue avec un proprietaire reste privee).
4. Remettre la **categorie** : c'est elle qui reserve la dotation a une escouade
   (« ECHO » reserve au genie). La republication depuis le jeu ne l'efface pas.

Verifier a la fin : `GET /api/v1/loadouts` doit montrer 8 dotations avec `owner` vide.
Ne PAS supprimer les anciennes avant d'avoir valide les nouvelles.

### A3. Regénérer les 8 dotations de faction sur le site
Le format des tenues a changé (on stocke maintenant la chaîne du sérialiseur du jeu).
Les dotations enregistrées avant la refonte produisent
`tenue au FORMAT OBSOLETE -> a re-enregistrer` et n'équipent rien.
⚠️ **Tâche bloquée** : il faut capturer les tenues en jeu. À faire avec Benji cet après-midi,
pas en autonomie. Préparer seulement la procédure.

### A4. Camo désert non réappliqué (décision à prendre)
La substitution de camo vivait dans `SpawnPlace`, qui ne sert plus. Une tenue capturée en
woodland ressortira en woodland sur Anizay. Deux options à instruire (ne pas coder sans arbitrage) :
soit une passe de substitution après l'apply, soit des dotations distinctes par théâtre.

### A5. Site — changelog + page Tests
- Ajouter une entrée « Journal des mises à jour » (`static/index.html`, tableau `CHANGELOG` inline)
  sur la refonte des tenues. Formulation joueur, pas jargon.
- Statuts à revoir sur `/tests` (`static/tests.html`, tableau `TESTS` inline) :
  `loadout-arme-accessoires` reste `a_tester`.
- Frontend seul → **scp uniquement, pas de rebuild**.

---

## LOT B — IA, ennemis, nettoyage

**Fichiers réservés à ce lot :** `FFRX_AIAssault.c`, `FFRX_AntiCamping.c`,
`FFRX_AdaptiveThreat.c`, les fichiers de diagnostic, et tout nouveau fichier IA.
**Ne pas toucher** aux fichiers du lot A ni au site.

### B1. Vision nocturne pour ennemis rares
Candidat identifié : `Dedal_Narodovolec` `{D8147F3F740C29DF}` dans TacticalFlava — c'est une
**optique d'arme**, donc pas besoin d'un slot casque sur le personnage.
- Distribution **rare** (Benji insiste) : quelques % des soldats, de nuit uniquement.
- ⚠️ **Le moteur ignore les JVN dans la perception de l'IA** : porter l'optique ne rend pas
  l'IA meilleure de nuit. Le bonus doit être **scripté** (facteur de perception), cible ~80 %
  de la vision de jour. Voir `SCR_AICombatComponent` / `PerceptionComponent`, setters
  `SetAISkill` / `SetPerceptionFactor` (déjà utilisés pour le réglage de difficulté).

### B2. Gilets suicide : charger au lieu de tirer
Fonctionnalité notée, jamais commencée. Une unité en gilet explosif doit foncer sur le joueur
pour exploser, pas tirer de loin. Le mécanisme de rush existe déjà dans `FFRX_AIAssault.c`
(`FFRX_DispatchTo`, priorité 95) — c'est surtout une question de détection du porteur
et de comportement terminal.

### B3. Récupérer des classes contre le plafond de compilation
Fichiers de pur diagnostic, ~13 classes au total, candidats à la suppression :
`FFRX_SpawnCensus`, `FFRX_BuildDiag`, `FFRX_MenuInputDiag`, `FFRX_SpecialistSpawnDebug`,
`FFRX_DroneDebug`.
⚠️ Demander à Benji avant de supprimer : certains servent encore à des tests en cours.
Commencer par lister ce que chacun fait réellement.

**Avancement.** `FFRX_MenuInputDiag` supprimé le 17/09. `FFRX_SpecialistSpawnDebug` supprimé
le 19/09 (accord Benji) : sa sonde s'accrochait à `OnPostInit` des composants WCS
anti-heli / anti-vehicule, donc **avant** que l'entité soit posée dans le monde — `GetOrigin()`
y rend `<0, 0, 0>`. Elle prouvait que les spécialistes spawnent (ce qui était déjà acté le
23/07), jamais **où**. 2 `modded class` rendues. Le suivi passe désormais par
`FFRX_SpawnCensus` et la page Recensement du site.
Restent : `FFRX_BuildDiag`, `FFRX_DroneDebug`. `FFRX_SpawnCensus` = **à garder**.

### B4. Dépendances externes — arbitrage (P1)
Toujours ouvert, et c'est devenu un problème de budget de compilation autant que de poids.
Livrable attendu : un tableau « mod / ce qu'il apporte / coût en scripts / verdict »,
pas une décision unilatérale.

---

## Bloqué sur la présence de Benji (ne pas tenter en autonomie)

- Reconstruire une caisse d'arsenal neuve (l'ancien enregistrement a 0 enfant)
- Tests en jeu : `boutique-batiment-supprime`, `loadout-arme-accessoires`, `ia-anticamping`,
  `construction-refus-explique`, `messages-dans-le-chat`
- Question de conception ouverte : le remboursement crédite l'équipement ennemi ramassé
  → ferme à ravitaillement possible ?
