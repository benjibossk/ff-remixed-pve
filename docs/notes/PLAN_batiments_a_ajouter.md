# Bâtiments à ajouter au catalogue de construction FF

> Relevé le 2026-09-19. Complète la liste de la ROADMAP (Pilier 3), qu'il remplace.
> Source : catalogue Game Master (`PrefabsEditable/Auto/Structures/…`) + arborescence
> `Prefabs/Structures/…` du jeu de base.

---

## ⚠️ Le verrou qui bloquait cette liste vient de sauter

La ROADMAP disait : *« Coût caché : chaque item exige un prefab fantôme (`m_aGhosts`) à
créer, absent pour ces bâtiments. »*

**Ce n'est plus vrai.** Depuis le chantier (`FFRX_BuildSite.c`, 2026-09-19), quand un item
n'a pas de `m_aGhosts` on **retombe sur le prefab réel** comme apparence de chantier. Il a
fallu l'écrire de toute façon : 8 des 23 bâtiments FF existants n'ont déjà aucun fantôme.

Conséquence : **ajouter un bâtiment ne demande plus qu'une entrée de configuration.**
Pas d'asset à créer, pas d'import Workbench.

---

## 1. Toujours prendre le prefab NON éditable

Le catalogue Game Master sert à **découvrir** ce qui existe, pas à être référencé.

| | |
|---|---|
| ❌ `PrefabsEditable/Auto/Structures/…/E_Xxx.et` | déplaçable en Game Master, porte un `SCR_EditableEntityComponent`, plus lourd |
| ✅ `Prefabs/Structures/…/Xxx.et` | ce qu'on veut poser |

C'est exactement la substitution faite sur les cabines téléphoniques d'Anizay le
2026-09-18 (57 instances). L'éditable **hérite** du réel et n'ajoute que le composant
d'édition — la permutation est donc sûre, mais dans ce sens-là seulement.

---

## 2. Liste de la ROADMAP — les 7 GUIDs sont vérifiés et valides

| Bâtiment | GUID | Chemin |
|---|---|---|
| Tour de contrôle | `{9CEC8BB63429FF28}` | `Structures/Airport/ControlTower_01/ControlTower_01.et` |
| Hangar | `{CEB1F71D46592F3C}` | `Structures/Airport/Hangar_01/Hangar_01.et` |
| Bunker SPS | `{6214C73708EA0E2D}` | `Structures/Military/Fortifications/Bunker_SPS/Bunker_SPS.et` |
| Bunker SPS 2M | `{BA8463780B060A68}` | `…/Bunker_SPS/Bunker_SPS2M.et` |
| Caserne (camo) | `{4D62E2F38A755750}` | `Structures/Military/Houses/Barracks_01/Barracks_01_military_camo.et` |
| Garage | `{80A5B37A1472B084}` | `Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et` |
| Radar d'approche | `{DED4DB7D08E6E0BE}` | `Structures/Military/Radar/ApproachRadar_RPL5_01/…` |

---

## 3. Nouveaux candidats, par usage

### A. Défense et postes de garde — le volet le plus absent du catalogue actuel
FF n'a que `FiringPosition`, `Bunker` et deux tours de garde. C'est le trou le plus net.

| Bâtiment | GUID | Chemin |
|---|---|---|
| Guérite | `{3F91DEEC9C78E473}` | `Military/Houses/GuardHouse_01/GuardHouse_01.et` |
| Tour de garde | `{FBCE1861B987EA68}` | `Military/Houses/GuardTower_01/GuardTower_01.et` |
| Poste de guet | *à relever* | `Military/Houses/GuardBox_01/GuardBox_01.et` |
| Tour de garde USSR | *à relever* | `Military/Houses/GuardTower_USSR_01/…` |

### B. Tranchées — modulaires, et le vrai « creusement » qu'on cherchait
13 pièces assemblables (`TrenchOld_01`) : droites 6 m, virages 30°, jonctions, extrémités,
en trois finitions (nue / bois / métal) plus des variantes forêt. **GUIDs à relever** : ce
sont des éléments de décor de carte, rien ne les référence dans l'extrait.

C'est aussi la réponse à la « tranchée ACE » : elle n'existe pas (l'action ACE n'est qu'une
animation, cf. `FFRX_RoleBonus.c`). Des tranchées constructibles par pièces donneraient le
même résultat avec notre propre système.

### C. Obstacles de terrain
| Bâtiment | Intérêt |
|---|---|
| `Dragontooth_01` V1/V2/V3, `Dragonsteeth_01_old` | anti-véhicule |
| `DirtCover_01_long` v1/v2/v3, `DirtCover_01_short` | merlons |
| `DirtPile_01_large` / `_small` | remblais |

### D. Camp et logistique
| Bâtiment | Intérêt |
|---|---|
| `ShelterStorageUS_01` / `ShelterStorageUSSR_01` | abris de stockage |
| `TentUSMedical_01`, `TentUSSRMedical_01` | poste médical avancé |
| `HelipadImprovised_US_01` / `_USSR_01` | hélipad léger, moins cher que celui de FF |
| `AirfieldMatting_US_01_long` / `_short` | piste sommaire |
| `CanvasCover_*` (Large / Medium / Small) | couvert, camouflage de matériel |
| 37 filets de camouflage (`Military/CamoNets`) | dissimulation de véhicules |

### E. Radio — lie le chantier « radio / signal » du Pilier 2
| Bâtiment | GUID | Chemin |
|---|---|---|
| Pylône émetteur | `{DBCDEC45DB834E8A}` | `Infrastructure/Towers/TransmitterTower_01.et` |
| Pylône moyen / petit | *à relever* | `…/TransmitterTower_01_medium` / `_small` |
| Antenne VOR | *à relever* | `Infrastructure/Towers/AntennaVOR_01.et` |

Ces pylônes sont la brique matérielle du « capturer/poser des antennes pour étendre la
couverture livemap » déjà écrit au Pilier 2.

### F. Bâtiments en dur — GUIDs à relever
Aucune version Game Master, donc rien ne les référence : `Barracks_E_02`, `Barracks_E_03`,
`BarracksWooden_01`, `ControlTowerMilitary_E_01` / `_E_02`, `GarageMilitary_E_01`
(`{F16324CB8CB07C00}` ✅), `FuelStorage_E_01`, `Cafeteria_E_01`, `ClubMilitary_01`.

---

## 4. Ce qu'il faut écrire par bâtiment

Une entrée dans notre `Configs/Construction/BuildItems.conf`, sur le modèle de
`FFRX_ArsenalBox` qui marche déjà :

```
JWK_BuildItemConfig "{<GUID d'instance NEUF>}" {
 m_sName        "FFRX_GuardTower"
 m_sTitle       "Tour de garde"
 m_sDescription "..."
 m_iDisplayOrder 520
 m_tPreview     "{...}UI/Textures/AssetImages/..."   // facultatif
 m_iCost         0
 m_iSuppliesCost 250                                  // pilote AUSSI le temps de chantier
 m_aPrefabs { "{<GUID du prefab>}Prefabs/Structures/..." }
 // m_aGhosts : plus necessaire, repli automatique sur m_aPrefabs
 m_iCategory     <categorie FF>
 m_iBuildAreaTypes 15
}
```

⚠️ `m_iSuppliesCost` sert deux fois : le prix **et** la durée de montage (2 points par
supply, compressé au-delà de 400). Le fixer, c'est régler les deux d'un coup.

⚠️ Notre `BuildItems.conf` est un **override** : ne jamais y redéclarer l'héritage, et
reprendre les GUIDs d'instance existants pour les items qu'on modifie (cf. mémoires
`enfusion-config-override-root-class` et `enfusion-override-instance-guid`).

---

## 5. Ordre proposé

1. **Défense (A)** — le trou le plus criant, 4 items, GUIDs presque tous connus.
2. **Obstacles (C)** — très peu coûteux, forte valeur défensive.
3. **Tranchées (B)** — le plus gros gain de jeu, mais 13 GUIDs à relever et une question
   d'assemblage (pièces libres, ou gabarits pré-assemblés ?).
4. **Radio (E)** — à synchroniser avec le chantier radio du Pilier 2.
5. **Camp (D)** puis **dur (F)**.

## 6. À trancher

- **Faut-il tout ouvrir d'emblée ?** Le Pilier 1 note que FF n'a aucune progression :
  tout est disponible dès J1, seul le coût limite. Ajouter 30 bâtiments accentue ça.
  Les réserver à des phases de campagne reste possible plus tard.
- **Zones** (`m_iBuildAreaTypes`) : quels bâtiments en FOB, lesquels réservés à la MOB ?
  `PLAYER_CAMP` n'est utilisé par **aucun** item FF aujourd'hui — il y a une place libre.
- **Les 13 pièces de tranchée** feraient 13 entrées de menu. Proposer plutôt 3-4 gabarits
  pré-assemblés ?
