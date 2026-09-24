# Contre-mesures — ce qu'il reste à faire dans le Workbench

> Le **code est fini et compile** (`FFRX_VehicleSmokeScreen.c` + `FFRX_VehicleSmokeWCS.c`).
> Il ne manque plus qu'à **monter les lanceurs sur les prefabs** — et ça se fait dans le
> Workbench, pas à la main : un `.et` édité au clavier a ses IDs d'entité non bakés et
> provoque un `SYSTEM_FAILURE` au join du dédié (cf. mémoire `hand-edited-et-resave-for-mp`).
> Dernière mise à jour : 2026-09-09.

---

## ⚠️ CHANGEMENT D'APPROCHE — l'override a été SUPPRIMÉ

Le shadow-override de `Mi8MT_unarmed_transport_Patrol.et` (GUID `5BBDA2DACF9CDCA4`) créé
le 2026-09-08 **a été supprimé** (sauvegarde : `tools/reforge-tools/_backup_mi8_override/`).

**Pourquoi** : `DarcChopper` override **déjà ce même GUID**, et son override n'est pas
vide — il déclare 13 composants et slots, dont son propre **`SDRC_ChopperComp`** (force
du rotor, vitesses min/max, remplissage des sièges), le démarrage moteur instantané, les
étiquettes d'entité éditable, la porte arrière et les sièges cargo.

Deux overrides sur un même GUID ne fusionnent pas : **le plus prioritaire remplace
l'autre entièrement**. Donc, selon l'ordre de chargement :
- soit DarcChopper gagne → nos lance-leurres n'existent pas, sans le moindre message ;
- soit REMIXED gagne → **DarcChopper perd tout son système** sur ce prefab, ses hélicos
  n'ayant plus de `SDRC_ChopperComp`.

Notre override étant **vide**, le second cas était une régression silencieuse déjà
embarquée dans le pak publié. D'où la suppression immédiate.

*(Collision trouvée avec `reforge.js guid`, l'outil de lecture de pak — elle était
invisible autrement.)*

---

## 1. Mi-8 ravitailleur → leurres  *(refaire, en HÉRITAGE)*

Rappel de cadrage : **DARC ne spawne aucun Mi-8** (ses 11 prefabs hélico sont AH6M,
Ka-137, UH1H, KA52, MI28, Mi-1, Mi24). Le Mi-8 est le nôtre : `FFRX_EnemyAirResupply.c`.

**À faire dans le Workbench :**
1. Ouvrir `{5BBDA2DACF9CDCA4}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_transport_Patrol.et`
   — c'est-à-dire **la version de DarcChopper**, celle que le jeu charge.
2. Clic droit → **Inherit in addon** (FF - REMIXED - PVE) → l'enregistrer sous
   `Prefabs/Vehicles/Enemy/Mi8MT_Resupply_Flares.et`.
   ⚠️ **Inherit, PAS Override** : on veut un GUID neuf. On hérite ainsi de toute la
   configuration DarcChopper (rotor, sièges, portes) *et* on ajoute nos leurres, sans
   toucher à son prefab.
3. Sur la racine, ajouter **`WCS_Armament_DispenserManagerComponent`**, avec dans son
   `m_FlareInfo` (type `WCS_Armament_DispenserFlareInfo`) un `m_FireModes` contenant un
   `WCS_Armament_DispenserSingleFireMode`, `m_iFireIntervalMS = 150`.
   Laisser `m_aActionEnabledCompartments` vide (notre tir est serveur, il contourne la
   vérification de poste d'équipage).
4. Dans le `SlotManagerComponent` **existant** (il contient déjà `door_rear`, `SeatsRear`
   et les `SupplyStorage`), **ajouter** deux slots `WCS_Armament_DispenserSlotInfo` —
   ne pas remplacer la liste, sinon on perd portes, sièges et stockages :

   | Nom | Offset | Angles |
   |---|---|---|
   | `Flares_Left`  | `-1.3694 2.5721 -2.3226` | `0 -74.5 0`  |
   | `Flares_Right` | `1.3694 2.5721 -2.3226`  | `0 74.5 180` |

   Sur chacun : prefab `{63DA839055CED0D2}Prefabs/Weapons/Countermeasures/Dispenser_Flare_Mi8MT.et`,
   `MergePhysics 1`, `DisablePhysicsInteraction 1`.
   *(Offsets repris du Mi-8 de WCS, même modèle 3D.)*
5. **Force Save All** sur le prefab (bake des IDs — obligatoire pour le dédié).
6. Me donner le **GUID** du nouveau prefab : je remplace `CHOPPER_PREFAB` dans
   `FFRX_EnemyAirResupply.c` (ligne 17). C'est la seule ligne à changer.

Le slot enregistre lui-même le lanceur auprès du gestionnaire
(`WCS_Armament_DispenserSlotInfo.OnAttachedEntity`).

---

## 2. Blindé ennemi → fumigènes  *(essai à juger sur pièce)*

Gabarit : `BTR70_Turret_SmokeExample.et` de WCS.
- Racine : `WCS_Armament_DispenserManagerComponent` avec `m_SmokeInfo`
  (`WCS_Armament_DispenserSmokeInfo`), `m_iFireIntervalMS = 200`.
- Un slot `WCS_Armament_DispenserSlotInfo` `SmokeLauncher`, offset `0 0.3423 -0.1874`,
  prefab `{BE733285C090F150}Prefabs/Weapons/Countermeasures/Dispenser_Smoke_BTR70.et`.

⚠️ **Même leçon que pour le Mi-8** : avant de shadow-overrider un véhicule de mod tiers,
vérifier qui d'autre override déjà ce GUID —
`node C:\Users\benbo\tools\reforge-tools\reforge.js guid <chemin>` liste tous les
déclarants. Préférer l'héritage dès qu'un autre mod est dans la place.

Tant que ce n'est pas fait, **tous les blindés retombent sur la fumée simulée**, qui
fonctionne déjà sans aucun prefab.

---

## 3. Hélicos DARC → plus tard

Les 11 prefabs des missions hélico DARC appartiennent à des mods tiers. Les équiper
demanderait 11 prefabs hérités et autant de branchements dans les configs DARC. À ne
faire que si l'essai Mi-8 convainc.
