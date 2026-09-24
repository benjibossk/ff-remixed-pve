# FF - REMIXED - PVE — Document technique (choix & logique)

> **But de ce doc** : expliquer **le POURQUOI** des choix — de gameplay et d'architecture — pour qu'une **IA** ou un **dev** comprenne comment la logique du mod est pensée, avant de toucher au code.
> La **vision** est dans [GAME_DESIGN.md](GAME_DESIGN.md) · le **quoi/quand** dans [ROADMAP.md](ROADMAP.md).
> FF-REMIXED = conversion coop-PVE de **Freedom Fighters** (+ Réoccupation) sur Everon. On **greffe** sur le natif FF autant que possible plutôt que de réinventer.

---

## 1. Décisions de GAMEPLAY (D1–D12)
> Choix tranchés avec Benji. Ils orientent toutes les features.

| # | Décision | Pourquoi |
|---|---|---|
| **D1** | **Victoire** = libérer 100 % du théâtre ; **Défaite** = chute du QG principal (bases secondaires reprenables sans game over) ; **Rotation** = deadline trimestrielle → bilan + wipe | Un but clair à toute la boucle + un seul point de rupture + du renouvellement régulier |
| **D2** | Rôles **variés par joueur** dans l'escouade (milsim) ; **capacités exclusives** conservées (Génie/Santé/ALAT/Artillerie/Radio) ; déblocage des rôles **par progression de campagne, pas par grade** ; armurerie façon FF (achat, armurier, fallback loadouts) | Interdépendance = cœur du coop ; un nouveau joueur n'est jamais bloqué par son rang ; réalisme logistique |
| **D3** | Escalade **combinée** (temps + progression) avec **plancher** ; **QG ennemi = les bases militaires FF** ; **offensives proactives** | Ennemi toujours dangereux même en fin de partie ; reprendre une base affaiblit l'ennemi sans le neutraliser ; ennemi vivant, pas que défensif |
| **D4** | État-major = **plusieurs joueurs** (admin ou non) via l'**escouade dédiée** (KILO) | Co-commandement sans flag admin serveur |
| **D5** | 3 revenus (marine **axe central**, pillage/captures, usines) ; **logistique vulnérable** | Crée du jeu défensif / escorte ; la marine rythme la progression |
| **D6** | **Persiste** carrière/prestige (grade, stats, cosmétique) ; **wipe** l'état de guerre | Les vétérans ne dominent pas le gameplay des nouveaux |
| **D7** | Intel **périssable** + **fausses pistes** + **fiabilité selon la source** ; **contre-espionnage** par les civils hostiles | Force à agir vite et à juger ; maltraiter la population coûte cher (fuite d'intel) |
| **D8** | Tension **scriptée par phase** + événements aléatoires ; 4 types d'événements (DARC, civils, embuscades, logistiques) | Narratif prévisible + surprise |
| **D9** | Mort des civils → chute d'attitude, perte d'intel, représailles | Punit le jeu bourrin |
| **D10** | Radio = canaux par escouade + VON de proximité ; restrictions de rôle **plus tard** | Simple et immersif d'abord |
| **D11** | Outils admin : événements manuels, dashboard Fleet, TP/gestion joueurs, gestion slots/rôles | Animer et opérer le serveur |
| **D12** | Menu J = 7 entrées gardées ; features solo traitées à part (repos/timeskip = 1 réglage OFF) ; **procurement = demande → validation** état-major | Dé-solo-ifier FF sans casser le natif |

---

## 2. Décisions d'ARCHITECTURE (le comment, et pourquoi comme ça)
> **À lire avant de coder.** Ces choix évitent des pièges récurrents ; les ignorer casse le build ou le dédié.

- **Greffe sur le natif FF, pas de réinvention.** Victoire/défaite = FF natif ; timer de campagne = les animateurs ; escalade = réglages Réoccupation. → **Campaign Manager custom ABANDONNÉ** (inutile, source de bugs).
- **MCD (dialogue civil) entièrement rapatrié dans FF-REMIXED.** Pourquoi : dépendre de MCD/FF directement cassait l'ordre de compilation EPF (`EPF_CharacterSaveData` forward-decl) et créait des conflits de pak. Rapatrier = zéro dépendance, plus de bug EPF.
- **Placement AA / drones / spécialistes via override des PREFABS DE GROUPES base-game** (`Group_USSR_*.et`, GUID stable), **jamais via `FF_USSR.conf`.** Pourquoi : `FF_USSR.conf` est une config FF qui évolue → un override d'elle devient périmé au prochain update FF (même piège que `GameSettings.conf`). Les prefabs de groupes sont stables. FF **et** DARC spawnent alors les groupes avec nos ajouts, sans câblage.
- **Overrides de prefabs = `+ { }` (append), jamais `{ }` (replace).** Un même-GUID override d'un tableau (`Slots`, `additionalActions`, `m_aUnitPrefabSlots`, `m_aCategories`…) **empile** avec `+` mais **écrase** sans. Ex. `Character_Base` Slots `+` (slot lunettes) : un replace vide les 8 slots de base → le joueur ne peut plus s'équiper.
- **Reskins = approche « personnages seulement ».** On reskine les persos (même GUID, merge de l'équipement) et on **garde les armes vanilla FF** + les compositions de groupes FF intactes. Pourquoi : charge serveur + éviter les références cassées.
- **Verrou de véhicule = 100 % système natif FF** (`JWK_OwnershipAccessComponent`, owner UID + liste autorisée déjà répliqués). Pas de réplication custom : l'état-major passe-partout = on pousse les UID dans la liste native `m_aAllowedPersId`.
- **Civils RPG = code-only, aucun modding de classe FF interne.** Une tentative de hook du spawn FF (`SpawnNpc_S`) a cassé le build (l'installé diffère de l'extract). Choix : un **scanner** serveur qui observe les civils et lie une identité — robuste (le visage reste FF, mais nom + mémoire persistent).
- **Explosifs = un seul écosystème ACE** dans REMIXED (FFMI reste un mod public séparé). Cohérence + un seul comportement de mine.
- **Field Manual : chapitre REMIXED autoritatif ajouté** (append) plutôt que réécrire les 61 entrées FF. Moins risqué ; l'entrée « Vue d'ensemble » dit que REMIXED fait foi en cas de contradiction.
- **Décision aérienne** : les joueurs pilotent, la menace vient de l'ennemi. **DARC Chopper = le spawner** (hard-dep de DarcMissions, non retirable) ; **REAPER gardé en réserve** pour l'air-air. Tout l'écosystème **drones/jammers est côté ENNEMI** (les joueurs subissent et doivent casser les jammers pour utiliser LEURS drones).
- **Flux drone vidéo longue-portée = impossible** (mur moteur Enfusion : le feed est un rendu 3D live côté client, l'entité + la scène doivent être streamées au spectateur). Pour « le QG suit un drone » → **télémétrie/marqueurs sur la livemap**, pas de vidéo. Détail : [RECHERCHE_DroneTV_relais.md](RECHERCHE_DroneTV_relais.md).

---

## 3. Conventions & pièges techniques (Enforce / Workbench / dédié)
> Règles dures, apprises à la dure. À respecter systématiquement.

- **Build = Benji lui-même, Shift+F7** (Validate and Reload) dans le Script Editor. Après reload/Play, **lire le `console.log` du dossier de logs le plus récent** (mtime, pas nom) pour les `SCRIPT (E)`.
- **`SCRIPT (E): Leaked '<type>'`** à chaque hot-reload = **normal** (pas une erreur).
- **Erreurs en cascade** : quand le module « Game » casse, le compilateur crache des erreurs bidon dans des fichiers base-game sans rapport (`replication`, `sortSlots`, `friendlyDefaultMapping`, `Tuple1/2`, `auto`, `switch fall through`…). Chercher la **vraie** erreur (souvent la première, dans un fichier FFRX), corriger, les cascades disparaissent.
- **Mojibake** `File.c(NN): error: <charabia>` + numéro de ligne souvent **faux** = un vrai bug de syntaxe qui cascade. Causes fréquentes : **mot réservé en identifiant** (`out`, `in`) ou **octet non-ASCII dans une string**.
- **Enforce : pas de ternaire `? :`** → utiliser if/else. Pas de `ref` vers un ScriptComponent (« Strong ref not allowed ») → handle simple ou re-fetch.
- **`string.Format()` tronque à ~8192 octets** → construire les grosses strings (JSON, textes) avec `+`, pas Format. (avait cassé les endpoints livemap.)
- **Dédié = strict ASCII dans les strings in-script.** Un caractère accentué en dur → desync/erreur sur le dédié que le Workbench tolère. Les accents passent par la **localisation** (StringTable), jamais en dur.
- **Prefab `.et` édité à la main = IDs non-bakés** → crash de réplication `SYSTEM_FAILURE` au spawn sur le dédié (invisible en mono-machine). **Règle : après toute édition manuelle d'un `.et`, re-baker** via clic-droit → « Edit Prefab(s) » → **« Force Save All »** (PAS « Reimport ») avant de déployer. Détecter les non-bakés : `rg --files-without-match -g '*.et' '^\s*ID "'`.
- **Après un update FF/mods majeur** : re-extraire les paks (PakInspector), diffuser les signatures, corriger uniquement les vraies erreurs. Les classes FF/mods changent (ex. FF 0.70 : `Enqueue` multi-variantes, H&M string→enum, `m_aOfficerCharacters` protégé, système AI streaming réécrit).
- **Un mod publié (même GUID) shadow le dev local** ; et **ouvrir le jeu trop vite après un build/publish** peut corrompre le pak client → `SCRIPT_MISMATCH` au join du dédié. Fix léger : re-sync le pak depuis le serveur (bat `SYNC_mods_depuis_serveur.bat`), pas re-DL 60 mods.
- **Réglages en jeu** = pattern `modded enum JWK_EGameSetting` + `modded JWK_GameSettingsCache` + injection coexistence-safe d'un groupe (ne JAMAIS posséder le `.conf` base `{051DF73D6EB0D933}` — ça efface les 121 réglages FF).

---

## 4. Stack & dépendances
- **Base** : Freedom Fighters (`CAFEBEEFF0CACC1A`) + Réoccupation + EPF/EDF + base game (`58D0FB3206B6F859`). REMIXED **dépend de Réoccupation, pas de FF directement** (ordre de compile EPF).
- **Contenu ennemi** : TacticalFlava (`5D550926D43F1409`) + ports autorisés Iron Beard / Conflict:Escalation.
- **Allié** : AMF (CORE `65D2BD1BD7EA7BF6`, FANTASSIN, VÉHICULES01) → Armée française.
- **Missions dynamiques** : DARC (Core/Missions/Chopper + compat FF).
- **Drones/jammers** : RealisticCombatDrones (`65AD60E204191D37`) + JamAiDrones + AIUseDrones + AIUsingStingers + WCS_Armaments.
- **Explosifs** : ACE Core + ACE Explosives.
- **Web/telemetry** : Fleet plugin → GTG-livemap (site `arma.collectifxxl.fr`) via `/ffstate`.
- **Deps à trancher** (bloque des features) : sling load / porte-char, pont REAPER↔FF.

*Sources reverse-engineering : extraits PakInspector dans `C:\Users\benbo\tools\PakInspector\*_extract\`. Mémoires projet Claude = détails techniques par système.*
