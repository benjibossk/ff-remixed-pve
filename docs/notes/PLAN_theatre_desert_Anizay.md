# PLAN — Théâtre désert Anizay (REMIXED)

> Objectif : un **scénario REMIXED-Anizay** (carte désert) où l'ennemi = insurgés **MEI** (look Taliban, déjà désert) et l'allié = **Armée française camo désert (AMF DA / Daguet)**, en réutilisant TOUT notre code REMIXED. Décidé avec Benji (2026-08-26).

## Décisions
- **Structure = scénario séparé** (comme FF fait Everon vs Anizay), PAS de bascule biome dans un seul scénario. Colle à la rotation trimestrielle de campagnes (D1).
- **Ennemi = MEI vanilla FF** (si ses characters conviennent) **+ réinjecter toutes nos modifs ennemies** : opérateurs drone, Javelin (AT), Igla (AA), soldat jammer — comme pour l'USSR.
- **Allié = FIA en camo désert AMF DA** (au lieu de CCE woodland).

## Mods fournis (installés)
- `Anizay_6266DCA9193C705E` — carte désert « GameMaster Anizay » v1.0.12.
- `FF-Anizay-Ternary_68DB0E515B508C67` — **scénario FF complet sur Anizay** v0.0.46 (mission `Anizay2.conf` + monde `FF_Anizay.ent` + toutes les couches villes/FOB/usines + `JWK_World_Anizay`). **= notre base/template.** Deps : Anizay + FF + EPF/EDF + base.
- `MiddleEastInsurgents_64CEC8E005828E5D` v1.3.0 + `MEI_FIX_6A1B31C0AD884B6E` v1.0.1 — contenu insurgés (à évaluer : enrichit-il le MEI de base ?).

## Ce que FF fournit déjà (template)
- **`Configs/Factions/FF_MEI.conf`** : faction MEI (insurgés désert), `m_aForces` = liste de `Character_MEI_*` (INDFOR/MEI), + groupes :
  - `m_rDefaultGroupPrefab {AD9B7874DFFF2A9D}Prefabs/Groups/MEI/Group_MEI_Base.et`
  - `m_aPatrolGroups` : `Group_MEI_SentryTeam.et`
  - `m_aSpecialistGroups` : `Group_MEI_SapperTeam.et`
- Le `JWK_FactionManager` **vanilla** contient déjà MEC/MEI/TKA (désert) → sur Anizay l'ennemi est déjà insurgé.
- Loadout : `Configs/Factions/Utils/Loadouts/Loadout_MEI.conf`.
- *(Système biome FF dispo — `JWK_EWorldBiome`/`m_iBiome` — mais FF-Anizay ne l'utilise PAS ; on ne s'en sert pas non plus. cf. mémoire `ff-biome-camo-system`.)*

## Le point d'injection de nos spécialistes = les `Group_MEI_*.et`
Même mécanisme que `Group_USSR_*` : **override même-GUID en append** de `m_aUnitPrefabSlots` pour ajouter nos spécialistes. Groupes MEI à couvrir : `Group_MEI_Base`, `Group_MEI_SentryTeam`, `Group_MEI_SapperTeam` (+ lister les autres via l'extract).

## Étapes

### Phase 0 — Vérifs — ✅ FAIT (2026-08-26)
1. **Source ennemie = le mod `MiddleEastInsurgents` (64CEC8E005828E5D) — CONFIRMÉ par GUID.** `FF_MEI.conf` référence `Character_MEI_Base` en `{92EF7C750D05F9C9}` = **un prefab de CE mod** (`Prefabs/Characters/Factions/IND/MEI/Character_MEI_Base.et`), PAS le base-game (grep base-game = rien). FF fait une **référence souple par GUID** (pas de dép dure) → la faction MEI de FF **est** le mod, il suffit de le charger. **Décision Benji : on prend le mod.** MEI_FIX (6A1B31C0…) = juste un fix d'enum éditeur (négligeable).
2. **Cibles d'override (32 `Group_MEI_*.et` dans le mod)** — les groupes spawnés à couvrir : `Group_MEI_Base`, `Group_MEI_FireTeam`, `Group_MEI_LightFireTeam`, `Group_MEI_RifleSquad`, `Group_MEI_SentryTeam`, `Group_MEI_MachineGunTeam`, `Group_MEI_ReconTeam`, `Group_MEI_Team_AT`, `Group_MEI_Team_LAT`, `Group_MEI_SapperTeam`, `Group_MEI_Defenders`, `Group_MEI_PlatoonHQ`, `Group_MEI_SharpshooterTeam`, `Group_MEI_SniperTeam`, `Group_MEI_MedicalSection`, `Group_MEI_AmmoTeam`, `Group_MEI_Team_GL`, `Group_MEI_Team_Suppress` (+ variantes `_NotSpawned` = templates, à voir si utiles). Même méthode que `Group_USSR_*` : override même-GUID `+ {}` de `m_aUnitPrefabSlots`.
3. **Spécialistes désert** : reste à décider — réutiliser nos prefabs actuels (Igla/Javelin/drone/jammer, look woodland-USSR, visuellement décalé chez des insurgés) OU créer des **variantes désert/MEI** (mieux). Reco : variantes MEI.

### Phase 1 — Scénario REMIXED-Anizay (BENJI, Workbench) — ⚠️ RECRÉER, pas dépendre de Ternary
> Le scénario `FF-Anizay-Ternary` **bugge** → on ne dépend PAS de lui. On **récupère ses layers POI** (autorisé par l'auteur TernaryOperator) et on **recâble le FF proprement** en REMIXED.
- **Garder (POI, le travail de placement)** : `Worlds/FF_Anizay_Layers/` — villes (~25), FOB/bases (fob_hilltop/nauzad/obeh/thirty_palms), usines (11 : oilfields, pharma_lab, hidden_arms_factory, warlord_villa…), aéroports (landay/riqay), checkpoints, radars, radios, stations essence, settlements, remote_sites, shops. *(Tout extrait dans `tools/PakInspector/ffanizay_extract/`.)*
- **Recréer/vérifier (le câblage qui bugge)** : `default.layer` (JWK_FactionManager), `JWK_World_Anizay.et` (map offset/size, persistence, road network), game mode + mission.
- **🐛 BUG DIAGNOSTIQUÉ (2026-08-26)** : ouvrir le scénario Ternary crache une **VME en boucle** sur chaque ville — `JWK_FactionManager NULL ... GetPlayerFactionKey` via `FFRO_FIATownReinforcement.OnPostInit` (compat Réoccupation). **Cause racine** (fin du log) : le `JWK_FactionManager` de Ternary référence des factions **NON chargées** — `'FR'`, `'RHS_USAF'`, `'RHS_AFRF'`, `'RHS_ION'`, `'FFAA'` *(« is not a valid SCR_Faction »)* → le faction manager n'initialise pas → tout ce qui appelle `GetRole/GetPlayerFactionKey` déréf null. **Les villes/layers sont bons** ; c'est UNIQUEMENT le faction manager mal configuré. **FIX en recréant** : `JWK_FactionManager` propre listant SEULEMENT nos factions valides — **FIA** (joueur) + **MEI** (ennemi) + **CIV** (+ ce que Réoccupation attend), pas FR/RHS/FFAA. Modèle = notre faction manager REMIXED-Everon (remplacer USSR par MEI).
- Créer un nouvel addon (ex. `FF - REMIXED - Anizay`) dépendant de : Anizay (carte) + FF + MiddleEastInsurgents + AMF + REMIXED-core. Game mode REMIXED (comme notre FreedomFighters_Base REMIXED).
- Vérifier : SCR_MapEntity, SCR_AIWorld/navmesh, PerceptionManager, LoadoutManager présents (cf. skill FF §15.1). Persistence key `FFRX_Anizay`. Générer le road network.
- Crédit auteur : **TernaryOperator** (port de la carte / layers) — à créditer.

### Phase 2 — Allié désert (CODE, moi + Benji import)
- Étendre le générateur `gen_fia_amf_overrides.py` avec un **profil camo DA (désert)** (uniformes/casque/gilet AMF DA au lieu de CCE) → produire un 2ᵉ set d'overrides `Character_FIA_*` OU des prefabs FIA désert dédiés au scénario Anizay.
- ⚠️ conflit même-GUID : woodland vs désert ne peuvent pas coexister sur le MÊME `Character_FIA_*`. → le set désert vit dans l'**addon/scénario Anizay** (chargé à la place du woodland pour cette carte), pas dans REMIXED-Everon.

### Phase 3 — Ennemi MEI + nos modifs (CODE, moi)
- Générer les **overrides `Group_MEI_*`** (append) ajoutant : opérateur drone (NOVA), Javelin AT, Igla AA, soldat jammer — variantes désert des specialists.
- Réutiliser nos systèmes : jammer (`SAL_DroneJammerComponent`), AA (`WCS_AI_AntiHeliEngagementComponent`), drones (DroneAI).

### Phase 4 — Brancher nos systèmes REMIXED
- La plupart de notre code est faction-agnostique (spawn, intel, civils, éco, écran mort, XP…) → tourne tel quel. Vérifier les endroits qui référencent USSR/FIA en dur (reskins, difficulté IA par force, véhicules ennemis TF) → adapter pour MEI/désert.

## Reste ⚪ / risques
- Nos reskins/difficulté/véhicules ennemis sont câblés **USSR** → sur Anizay c'est MEI : certains systèmes (véhicules TF, difficulté par force USSR) ne s'appliqueront pas → à re-cibler MEI.
- Décider : un seul addon REMIXED multi-cartes (avec sets désert conditionnels par scénario) ou un **sous-addon REMIXED-Anizay** dépendant de REMIXED-core. Reco : sous-addon désert (évite les collisions de GUID woodland/désert).
