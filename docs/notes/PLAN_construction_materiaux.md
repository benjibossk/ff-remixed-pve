# Plan — Construction par matières premières (« façon Foxhole »)

> Rédigé le 2026-09-11. Décision de départ (Benji) : on veut des **matières premières
> transportées**, et pour ne pas alourdir la boucle, on doit pouvoir **les acheter
> directement avec les supplies**.
>
> Analyse technique des deux systèmes de référence : voir ROADMAP.md, Pilier 3.
> Ce document-ci est le **plan d'exécution** : quoi, dans quel ordre, et ce qui reste à trancher.

---

## 1. Le principe, en une phrase

Aujourd'hui on **paie** un bâtiment (un solde de supplies descend, le bâtiment apparaît).
Demain on le **fabrique** : on achète de la matière, on l'amène sur place, on la monte à la pelle.

Ce qui change vraiment pour le joueur, et c'est tout l'intérêt :

| | Aujourd'hui | Avec les matériaux |
|---|---|---|
| Le coût | un nombre qui descend | des caisses à charger et à conduire |
| Le trajet | inexistant | une cible pour l'ennemi |
| Le chantier | instantané | visible, interruptible, défendable |
| Le génie | un droit invisible | un métier : il porte les plans et l'outil |

⚠️ **Le risque à surveiller** : transformer chaque muret en corvée de 20 minutes. Le garde-fou
est à l'étape 1 — l'achat direct en supplies. Sans lui, le système devient punitif.

---

## 1 bis. ⚠️ CORRECTION MAJEURE (2026-09-11) — le jeu de base fait DÉJÀ tout ça

En cherchant comment fonctionnent les sacs de sable qu'on pose et le mortier qu'on assemble
en pièces, on tombe sur une famille de composants **native, entièrement pilotée par la
config** : `SCR_MultiPartDeployableItemComponent` et ses satellites
(`SCR_DeployableVariantContainer`, `SCR_RequiredDeployablePart`, `SCR_PlaceableItemComponent`,
`SCR_DeployableSurfaceValidation`, `SCR_AdditionalDeployablePart`…).

**Ce que ça fait, tel quel :**

| Besoin de notre plan | Champ natif |
|---|---|
| Le « plan » posé au sol | `m_sPreviewObject` — modèle d'aperçu avant pose |
| Les matières exigées | `m_aRequiredElements` : liste de `{prefab, quantité}` |
| **Chercher la matière AUTOUR, pas dans l'inventaire** | `m_fSearchRadius` — **7 m par défaut** |
| Consommer la matière | `m_bDeletePartsOnDeployment` |
| Dire au joueur ce qui manque | `m_sPartName` (chaîne localisée par pièce) |
| Ce qui apparaît au final | `m_sReplacementPrefab` |
| Plusieurs recettes possibles | `m_aVariants` |
| Refus si le terrain ne convient pas | bounding box, `m_fMaxTilt`, classe de validation de surface |

**C'est exactement le modèle Foxhole voulu** : on dépose les caisses de matériaux au sol, on
se met à l'endroit voulu, l'aperçu s'affiche, et l'ouvrage se monte en consommant ce qui est
posé autour. **Aucun script à écrire** — c'est de la configuration de prefab.

⚠️ **La limite, et elle est importante** : ce système fait apparaître un **prefab**. Il ne
connaît ni les zones de construction FF, ni le budget, ni les limites par zone, ni les
compositions multi-objets. Il est donc parfait pour les **fortifications, positions de tir,
mortiers, obstacles** — et inadapté tel quel aux 23 bâtiments FF, qui sont des compositions
gérées par `JWK_BuildAreaControllerComponent`.

**Conséquence sur le plan : on fait DEUX voies, pas une.**

- **Voie A — petits ouvrages (natif, config seule).** Sacs de sable, chevaux de frise,
  positions de tir, mortiers. `SCR_MultiPartDeployableItemComponent` + caisses de matière.
  Rapide à livrer, testable tout de suite, zéro script.
- **Voie B — bâtiments FF (script).** On garde le système FF (zones, budget, fantômes) et on
  y greffe la facture matériaux : avant `SpawnComposition`, vérifier les caisses dans un rayon
  autour du chantier, puis les consommer. La couche « montage à la pelle » (§2) s'applique ici.

La voie A devient donc l'**étape 1 réelle** : elle valide les caisses, l'achat en supplies et
le geste de pose, avant qu'on touche à quoi que ce soit dans FF.

---

## 1 ter. Pourquoi FF ne fait PAS pelleter, et comment le corriger (2026-09-11)

Constat Benji : dans FF on passe en caméra libre et **le bâtiment apparaît d'un coup**,
personne ne donne un seul coup de pelle — alors que le jeu de base, lui, fait pelleter.

**C'est confirmé, et ce sont deux systèmes totalement séparés :**

| | Jeu de base (sacs de sable) | FF |
|---|---|---|
| Chantier | `SCR_CampaignBuildingLayoutComponent`, valeur accumulée | aucun |
| Progression | `AddBuildingValue()` par coup d'outil, 50 % / 100 % | aucune |
| Apparition | `SpawnComposition()` à 100 % | **immédiate** |

FF n'utilise **rien** du système `SCR_CampaignBuilding*`. Il a le sien :
`JWK_ConstructionManagerComponent.SpawnBuildItem(prefab, pos, angles)` fait apparaître le
prefab final sur-le-champ. Son `m_aGhosts` n'est que l'aperçu **pendant le placement**, pas
un chantier persistant.

**Le point d'accroche est propre et disponible :**
```
// JWK_ConstructionManagerComponent.c:371 — PUBLIQUE, classe non `sealed`
IEntity SpawnBuildItem(ResourceName prefab, vector pos, vector angles)
```
Un `modded class JWK_ConstructionManagerComponent` qui override cette méthode permet de
remplacer « faire apparaître le bâtiment » par « poser un CHANTIER » : un objet qui accumule
la valeur des coups de pelle (mécanisme du jeu de base, ou le nôtre) et n'appelle
`super.SpawnBuildItem()` qu'une fois la construction terminée.

⚠️ **Le piège à vérifier avant de coder.** L'appelant fait, juste après :
```
IEntity entity = SpawnBuildItem(prefab, pos, angles);
JWK.GetPlayerProfile(playerId).TakeMoney_S(...);
PostBuildItemCreated(playerId, itemConfig, entity);
```
Il recevrait donc notre **chantier** là où il attend un **bâtiment fini**. Deux conséquences
à trancher : (a) le chantier compte-t-il dans la limite d'objets de la zone — probablement
oui, et c'est souhaitable ; (b) que fait `PostBuildItemCreated` d'une entité qui n'est pas
encore le bâtiment ? À lire avant de se lancer.

### ✅ VÉRIFIÉ (2026-09-17, 2e passe) — le système de chantier est RÉUTILISABLE TEL QUEL

> **Annule et remplace la correction ci-dessous**, qui concluait à tort que le jeu de base ne
> fait pas pelleter. La méthode `Build()` du composant de gadget est bien commentée, mais elle
> est **inutile** : le lien pelle → chantier passe par l'**action utilisateur**.

**Le prefab de chantier existe déjà** — `Prefabs/Compositions/Misc/FreeRoamBuilding/CompositionLayoutBase.et` :

```
SCR_CampaignBuildingLayoutComponent          progression (m_iToBuildValue / m_fCurrentBuildValue)
SCR_CampaignBuildingBuildUserAction          action "Construire", Duration -5, PerformPerFrame 1
SCR_CampaignBuildingDisassemblyUserAction    démonter
RplComponent                                 répliqué
RigidBody (NoCollision, Static)              boîte d'interaction, ne bloque pas
```

**La chaîne complète, vérifiée ligne à ligne :**

```c
PerformAction()      -> networkComponent.AddBuildingValue(GetBuildingToolValue(user), owner)
GetBuildingToolValue -> gadgetManager.GetHeldGadgetComponent().GetToolConstructionValue()  // = m_iConstructionValue
EvaluateBuildingStatus(v) -> si v >= m_iToBuildValue : SpawnComposition()
SpawnComposition()   -> SCR_EditorLinkComponent.SpawnComposition() puis supprime le chantier
```

**Aucun couplage à Conflict, les quatre points de blocage potentiels sont levés :**

1. `SCR_CampaignBuildingNetworkComponent` vit sur **`DefaultPlayerControllerMP.et`**, pas sur un
   contrôleur Conflict — et `JWK_PlayerController.et` en dérive. Les joueurs FF l'ont donc déjà.
2. `SCR_EditorLinkComponent` **ne dépend pas de l'éditeur** : il spawne une liste de prefabs
   enfants déclarée dans les données du prefab. Son garde `IsEditMode()` ne sert qu'à ne rien
   faire dans le World Editor.
3. `CanBePerformedScript` n'exige **qu'un gadget en main** + `CanUseItem()`. Pas de faction,
   pas de base, pas de mode de jeu.
4. `m_iToBuildValue` est normalement fourni par le gestionnaire de Conflict, mais c'est un
   simple champ : on le règle directement dans notre prefab dérivé.

**Ce que ça change pour le plan.** Plus besoin d'écrire une progression, ni de créer un prefab
de chantier de toutes pièces, ni de gérer la réplication. Il reste, **par bâtiment**, un prefab
dérivé de `CompositionLayoutBase` dont les entrées `SCR_EditorLinkComponent` pointent vers la
composition finale — et l'override de `SpawnBuildItem` qui pose ce chantier au lieu du bâtiment.

*Nuance à traiter :* l'action accepte **n'importe quel** gadget en main (le filtre "outil de
construction" se fait en aval, `GetToolConstructionValue()` rendant 0 pour les autres). Un
joueur avec une lampe torche verra donc l'action mais ne progressera pas. À gater proprement.

---

### ⚠️ CORRECTION (2026-09-17) — le jeu de base NE FAIT PAS pelleter non plus  *(ERRONÉE — voir ci-dessus)*

Le tableau ci-dessus est **faux sur une ligne** : « Progression — `AddBuildingValue()` par coup
d'outil ». Vérification faite dans le pak du jeu livré, `SCR_CampaignBuildingGadgetToolComponent.c` :

```c
//	//------------------------------------------------------------------------------------------------
//	// Perform one build step - add a given build value to a composition player is building.
//	protected void Build(notnull SCR_CampaignBuildingLayoutComponent layoutComponent)
//	{
//		layoutComponent.AddBuildingValue(m_iConstructionValue);
//	}
```

**La méthode est commentée dans le jeu livré.** Le `m_iConstructionValue 10` que porte notre
pelle n'est lu par personne : `GetToolConstructionValue()` n'a aucun appelant dans tout le
jeu de base. Bohemia a laissé la mécanique en place mais débranchée.

Ce qui existe bel et bien côté natif, et reste valable : `AddBuildingValue()`,
`EvaluateBuildingStatus()` (bascule à 100 %), `m_iToBuildValue` / `m_fCurrentBuildValue`, le
tout **répliqué** (`Replication.BumpMe()`). La forme est donc bonne à copier.

**Mais le réutiliser tel quel est déconseillé** : le reste de la classe est soudé à l'éditeur
de Conflict — `SCR_EditorLinkComponent`, `SCR_EditableEntityComponent`, `SCR_RefPreviewEntity`,
compositions désignées par ID via un `SCR_CampaignBuildingManagerComponent` et son gestionnaire
d'outlines. L'importer dans FF signifierait traîner toute cette plomberie à côté du système de
construction que FF possède déjà.

**Conséquence :** on écrit notre propre progression, légère, côté FF. On peut en revanche
réutiliser `GetToolConstructionValue()` comme rendement par coup, pour que les différentes
pelles ne se valent pas.

**Séparer les deux reproches.** La caméra de placement et l'apparition instantanée sont deux
choses distinctes. Une caméra de placement est normale (le jeu de base en a une aussi pour
viser la pose). Le vrai défaut, c'est **l'absence de chantier** : sans lui, construire n'est
pas une action, c'est un achat. C'est ça qu'on corrige.

---

## 1 quater. Usines TYPÉES : la carte décide de ce qu'on peut bâtir (idée Benji, 2026-09-11)

> « Plusieurs types d'usine — ferme, cimenterie, scierie. Capturer certains matériaux permet
> de construire certains bâtiments. Le surplus se revend contre des supplies. »

**Pourquoi c'est la meilleure idée du chantier.** Aujourd'hui toutes les usines FF sont
**interchangeables** : ce sont des robinets à supplies, et en capturer une plutôt qu'une autre
ne change rien. Les typer donne une identité à chaque point de la carte et transforme la
question « où attaquer ? » en question **tactique** : on n'attaque plus « une usine », on
attaque *la cimenterie, parce qu'on veut des bunkers*.

Et le surplus revendable ferme la boucle : sans lui, une faction qui tient trois scieries
accumule du bois mort et le système devient une punition.

**Ce que FF a déjà, et qui rend l'idée abordable :**
- Les usines sont des **POI capturables**, avec leur propre stockage
  (`JWK_FactoryResourceStorageComponent`, qui étend le stockage logistique vanilla).
- Une **production périodique** déjà réglable (`ECONOMY_FACTORY_PRODUCTION_FACTOR`,
  `START_FACTORY_STOCK_MIN/MAX`).
- La capture d'usine est déjà une **mécanique de progression** reconnue (`FACTORY_CAPTURE`).

Ce qui manque : **le type**. Une usine ne produit aujourd'hui qu'une chose, indifférenciée.

**Deux voies d'implémentation, et elles ne se valent pas :**

- **(a) Ajouter des types de ressource au moteur.** `EResourceType` (base game) ne contient
  que `SUPPLIES` et `ELECTRICITY` — c'est un enum script, donc extensible par `modded enum`
  (on le fait déjà pour `ECommonItemType`). Intégration profonde : stockages, transport et
  UI logistique existants marcheraient. ⚠️ Mais ça touche à une mécanique centrale du jeu de
  base, avec un risque de rupture à chaque mise à jour, et une UI à vérifier partout.
- **(b) Les matériaux restent des CAISSES physiques** (des items), et l'usine en produit dans
  son stockage. ⚠️ Pas d'intégration à l'UI logistique, mais **compatible直 avec le système
  de déploiement natif** (§1 bis), qui cherche justement des *prefabs* dans un rayon.
  *Recommandation : (b).* On garde le geste Foxhole — charger, conduire, décharger — et on
  évite de modifier une mécanique centrale du moteur.

**Typage proposé (à ajuster selon les usines réellement présentes sur les cartes) :**

| Usine | Produit | Débloque surtout |
|---|---|---|
| **Scierie** | madriers | structures légères, tours, tentes, planchers |
| **Cimenterie** | ciment | ouvrages en dur, dépôts, garages, bunkers |
| **Sablière / carrière** | sacs de sable | fortifications, positions de tir |
| **Aciérie / fonderie** | plaques d'acier | bunkers, hangars, portes blindées |
| **Ferme** | ravitaillement / vivres | ⚠️ ne sert à aucune construction — voir ci-dessous |

⚠️ **Le cas de la ferme est un vrai problème de conception.** Elle ne produit rien qui serve
à bâtir. Trois issues : en faire la source des **supplies** (donc du carburant économique
général), lui donner un usage propre (soin, moral, ravitaillement des civils — lie le Pilier 4),
ou la laisser hors du système. À trancher, sinon on aura une usine que personne ne capture.

**La revente du surplus** se branche naturellement sur le dépôt : convertir des caisses en
supplies au taux inverse de l'achat, avec une **marge défavorable** (par ex. rachat à 60-70 %).
Sans cette marge, acheter-revendre en boucle devient une machine à supplies.

⚠️ **Dépendance de conception** : ce système n'a d'intérêt que si les cartes ont réellement
plusieurs types d'usines identifiables. **À vérifier sur Everon ET sur Anizay** avant de
s'engager : si Anizay n'a que deux usines génériques, le typage n'y produira aucune décision
intéressante.

---

## 1 quinquies. ⚠️ V1 MINIMALE — la version à livrer en premier (2026-09-11)

> Doute de Benji : « c'est peut-être un peu compliqué au début ? »
> Doute légitime. Le risque n'est pas l'ambition, c'est de livrer un système **à moitié fait** :
> des joueurs qui trimballent des caisses sans comprendre pourquoi, pour un bâtiment qui
> apparaissait très bien tout seul avant.

**Règle retenue : UNE SEULE MATIÈRE pour la V1.**

Une caisse générique, « **matériaux de construction** ». Pas de bois / ciment / acier / sable,
pas d'usines typées, pas de plans. La boucle complète tient alors en quatre gestes :

> **acheter au dépôt (supplies) → charger → conduire → décharger → pelleter**

**Pourquoi ça suffit à faire le boulot.** Ce qui rend la construction intéressante, ce n'est
pas la comptabilité à quatre ressources, c'est que **le coût devienne un trajet** et que le
chantier soit **visible et attaquable**. Une seule matière donne déjà tout ça. Les quatre
matières n'ajoutent que de la *variété de décision* — c'est du raffinement, pas du fondement.

**Ce que la V1 apporte, mesurable :**
- la logistique a enfin une raison d'exister au-delà du ravitaillement ;
- le génie a un métier (l'outil, le chantier, le temps) ;
- une base en construction devient un objectif pour les deux camps.

**Complexité, pour qui ?** À distinguer, parce que les deux doutes ne se traitent pas pareil :
- **Pour le joueur** : elle vient du nombre de matières et d'étapes. Une matière = une étape.
  La V1 est donc *simple à jouer*, même si elle change beaucoup le jeu.
- **Pour nous** : elle vient de la greffe sur le pipeline de construction FF (`SpawnBuildItem`,
  cf. §1 ter). Ce coût-là est le même avec une matière ou avec quatre — autant le payer une
  fois, sur une V1 sobre.

**Périmètre de la V1 : UN SEUL BÂTIMENT d'abord** (le Bunker, 120 supplies, petit et rapide à
tester), avant de basculer les 23. Si le geste n'est pas agréable sur un bunker, il ne le sera
pas sur un dépôt à 1500.

**Ordre d'ajout ensuite, chaque étape étant optionnelle :**
1. V1 : une matière, un bâtiment, chantier à la pelle.
2. Étendre aux 23 bâtiments (même code, table de coûts dérivée §4).
3. Les 4 matières (le système de caisses ne change pas, seul le contenu de la facture change).
4. Les usines typées (§1 quater) — n'a de sens qu'une fois les 4 matières en place.
5. Les plans du génie.

⚠️ **Garde-fou à prévoir dès la V1** : un réglage admin « construction par matériaux »
(0 = comportement FF actuel). Si le système ne plaît pas en jeu, on le coupe sans republier.

---

## 1 sexies. Dé-risquage technique — résultat (2026-09-11)

Lecture du chemin réel de construction, `JWK_BuildAreaControllerComponent.c:340-358` :

```c
IEntity entity = JWK_ConstructionManagerComponent.GetInstance().SpawnBuildItem(prefab, pos, angles);
if (!entity) return null;
if (moneyCost > 0)    profile.TakeMoney_S(...);
if (suppliesCost > 0) m_LogisticsStorage.TakeResources(SUPPLIES, suppliesCost);   // <-- LE COUT EST ICI
RegisterBuildItem_S(entity);                                                       // <-- limite de zone
JWK_ConstructionManagerComponent.GetInstance().PostBuildItemCreated(...);          // <-- simple event
```

**Verdict : aucun blocage.** Trois enseignements :

1. **`PostBuildItemCreated` est inoffensif** — il ne fait qu'émettre `NotifyPlayerBuilt_S`.
   Lui passer un chantier au lieu d'un bâtiment ne casse rien. C'était mon inquiétude
   principale, elle est levée.
2. **Le coût en supplies est prélevé par l'APPELANT, pas par `SpawnBuildItem`.** Donc
   overrider `SpawnBuildItem` seul laisse les supplies être débités quand même. Pour que les
   matériaux **remplacent** le coût (décision §6.1), il faut en plus soit mettre
   `m_iSuppliesCost` à 0 dans notre `BuildItems.conf`, soit overrider l'appelant.
   *La voie config est la plus simple et la plus réversible.*
3. **`RegisterBuildItem_S(entity)` inscrit l'entité dans `m_aBuildItems`** (la limite de zone).
   Notre chantier y sera donc compté — c'est **souhaitable** (un chantier occupe une place).
   ⚠️ Mais il faudra gérer proprement la bascule chantier → bâtiment : désinscrire puis
   réinscrire, sinon la zone compte deux objets pour un.

**Sur l'aspect visuel du chantier.** Le « fantôme » de FF est **client-side** : il n'existe
pas de prefab de fantôme séparé, c'est le prefab du bâtiment lui-même, spawné en local avec
la physique désactivée. Il n'est donc pas réutilisable comme chantier serveur.

⚠️ **Il nous manque donc UN asset, et c'est le seul vrai prérequis** : un prefab « chantier »
portant un `ActionsManagerComponent` avec notre action « Construire ». Sans lui, aucun moyen
d'attacher une action de pelle à une entité spawnée à l'exécution — les actions viennent du
prefab, on ne peut pas les ajouter par script.
*Piste sans art nouveau :* réutiliser le prefab du bâtiment, enfoncé dans le sol, et le faire
monter au fil de la progression. Lisible immédiatement, aucun modèle à créer.

---

## 2. Les trois couches (livrables indépendants)

Chacune apporte quelque chose seule. On peut s'arrêter après n'importe laquelle.

### Couche 1 — Matières premières achetables et transportables
- 4 ou 5 ressources physiques (voir §4), sous forme de **caisses transportables**.
- **Achat direct en supplies** au dépôt : c'est la simplification demandée. Pas de chaîne
  de production à gérer, pas de minage — on convertit du supplies en matière, point.
- Transport libre : à la main, en véhicule, en hélico.

### Couche 2 — Montage progressif à la pelle
- Mécanisme **natif du jeu de base**, celui des sacs de sable et barbelés :
  `SCR_CampaignBuildingBuildUserAction` → `AddBuildingValue()` →
  `EvaluateBuildingStatus()` (50 % = le chantier prend forme, 100 % = `SpawnComposition()`).
- La vitesse vient de l'**outil** (`m_iConstructionValue`). Notre `ETool_ALICE` porte déjà 10.
  Un outil de génie à 25-30 rend le levier « le génie construit plus vite » gratuit.

### Couche 3 — Plans du génie
- Un item **« plan »** débloque un jeu de constructions pour celui qui le porte.
- Intérêt de conception : un droit qu'on **porte**, qu'on peut perdre, voler, ou confier —
  au lieu d'une permission invisible attachée à une escouade.

---

## 3. Les 23 bâtiments FF existants (relevés dans `Configs/Construction/BuildItems.conf`)

Coût actuel en supplies, et zones autorisées (`m_iBuildAreaTypes`, `-` = partout).

| Bâtiment | Supplies | Zones | Famille proposée |
|---|---:|---|---|
| FiringPosition | 50 | 11 | **Léger** |
| Bunker | 120 | 11 | **Fortification** |
| StorageSmall | 200 | 11 | Léger |
| ArmoryFOB | 200 | - | Léger |
| EquipmentStoreFOB | 200 | - | Léger |
| Helipad | 200 | - | **Lourd** |
| FieldFuelStorage | 200 | 3 | Léger |
| ArmoryMOB | 250 | - | Léger |
| EquipmentStoreMOB | 250 | - | Léger |
| FieldGarageSmall | 250 | 11 | Lourd |
| GuardTowerFOB | 250 | 10 | Fortification |
| LivingArea | 300 | 3 | Léger |
| MedicalTent | 300 | 3 | Léger |
| MortarFortified | 300 | 3 | Fortification |
| StorageMedium | 400 | 11 | Léger |
| FieldGarageMedium | 400 | 11 | Lourd |
| GuardTowerMOB | 400 | - | Fortification |
| CommandPost | 500 | 3 | Lourd |
| HelipadRepair | 600 | - | Lourd |
| StorageLarge | 1000 | - | Lourd |
| VehicleDepot | 1500 | 9 | Lourd |
| VehicleRamp | 0 | 3 | Léger |
| FuelPoint | 0 | 3 | Léger |

**Total : 7 870 supplies.** Deux items sont **gratuits** (VehicleRamp, FuelPoint) — à
décider s'ils le restent (probablement oui : ce sont des commodités, pas des ouvrages).

⚠️ Rappel : 4 items FF sont déjà **désactivés** chez nous (`FFRX_RemoveCommandPost` et les
`m_bEnabled 0` de notre `BuildItems.conf`), et on ajoute déjà `FFRX_DepotFR` et
`FFRX_ArsenalBox`. La liste effective en jeu n'est donc pas exactement celle-ci.

### Bâtiments candidats à ajouter (GUIDs déjà relevés, cf. ROADMAP Pilier 3)
Tour de contrôle `{9CEC8BB63429FF28}` · Hangar `{CEB1F71D46592F3C}` · Bunkers SPS
`{6214C73708EA0E2D}` / `{BA8463780B060A68}` · Casernes `{4D62E2F38A755750}` · Garage
`{80A5B37A1472B084}` · Radar `{DED4DB7D08E6E0BE}` · Pylônes d'antenne (lie le chantier radio).

⚠️ **Coût caché déjà identifié** : chaque item de construction exige un **prefab fantôme**
(`m_aGhosts`) — l'aperçu translucide qu'on place avant de bâtir. Il n'existe pas pour ces
bâtiments, il faudra le créer. C'est le vrai travail de ces ajouts, pas la config.

---

## 4. Les matières premières proposées

Quatre suffisent. Au-delà, on gère un inventaire au lieu de jouer.

| Matière | Sert à | Famille |
|---|---|---|
| **Sacs de sable** | fortifications, positions de tir | Fortification |
| **Madriers** | structures légères, tentes, tours | Léger |
| **Ciment** | ouvrages en dur, dépôts, garages | Lourd |
| **Plaques d'acier** | bunkers, portes, hangars | Lourd / Fortification |

### Recette : dérivée du coût, pas écrite à la main
Écrire 23 recettes à la main, c'est 23 occasions de se tromper et un enfer à rééquilibrer.
On dérive la facture du **coût en supplies existant**, déjà équilibré par FF :

```
unites_totales = round(cout_supplies / PRIX_UNITE)      // PRIX_UNITE ~ 50
repartition selon la famille :
  Leger         →  70 % madriers, 30 % sacs de sable
  Fortification →  60 % sacs de sable, 40 % acier
  Lourd         →  50 % ciment, 30 % acier, 20 % madriers
```

Exemples : FiringPosition (50) = 1 unité. Bunker (120) ≈ 2 sacs + 1 acier.
CommandPost (500) = 10 unités → 5 ciment, 3 acier, 2 madriers. VehicleDepot (1500) = 30 unités.

Un seul curseur, `PRIX_UNITE`, règle la lourdeur de **tout** le système.

---

## 5. Ordre de travail proposé

1. **Les caisses de matière + l'achat en supplies.** Livrable autonome et testable seul :
   on achète, on charge, on transporte. Même sans construction derrière, ça se vérifie.
2. **Brancher la facture matériaux sur la construction existante.** Le menu FF ne montre
   plus « 500 supplies » mais « 5 ciment, 3 acier, 2 madriers », et vérifie la présence
   sur le chantier.
3. **Le montage à la pelle.** Bascule du bâtiment instantané vers le chantier progressif.
4. **Les plans du génie.** Gating par item porté.
5. **Nouveaux bâtiments** (tour de contrôle, hangar, bunkers SPS…), une fois le socle en place
   — et une fois réglée la question des prefabs fantômes.

---

## 6. ✅ DÉCISIONS PRISES (Benji, 2026-09-18) — 5/5

| | Question | Réponse |
|---|---|---|
| **D1** | Jusqu'où va la première version ? | **Chantier + matériaux** |
| **D2** | Quel geste pour construire ? | **Pelle sur chantier** |
| **D3** | Matériaux à la place des supplies, ou en plus ? | **Selon la taille** |
| **D4** | Qui a le droit de construire ? | **Génie pose, tous aident** |
| **D5** | Combien de temps pour monter un ouvrage ? | **Proportionnel au bâtiment** |

**Ce que ça change par rapport au plan écrit plus haut :**

- **D1 élargit la V1 minimale du §1 quinquies.** Celle-ci proposait *chantier seul*, matériaux
  plus tard. On fait les deux d'un coup. Le §1 quinquies reste valable sur un point qu'il faut
  garder : **un seul type de matière** (« matériaux de construction ») et **un seul bâtiment**
  pour commencer. Élargir le périmètre des couches ne veut pas dire élargir celui du test.
- **D3 tranche différemment de la recommandation du §6.1** (« les matériaux remplacent »).
  *Selon la taille* = les petits ouvrages restent payés en supplies seuls, les gros exigent des
  matériaux transportés. C'est mieux que la règle uniforme : poser un sac de sable ne doit pas
  demander un convoi, bâtir un dépôt si. Reste à fixer **le seuil** — une valeur en supplies,
  au-dessus de laquelle la facture passe en matériaux.
- **D4 s'appuie sur une brique livrée le jour même** : le drapeau `genie` par escouade
  (`ffrx-groups.json` + `FFRX_GroupsManager.IsGenie`, cf. `FFRX_RoleBonus.c`). *Génie pose* =
  seul un sapeur peut **ouvrir** le chantier ; *tous aident* = n'importe qui peut pelleter
  dessus. Le bonus de rendement du génie s'applique en plus, sans rien à écrire.
- **D5 se règle par `m_iToBuildValue`**, posé à la création du chantier en fonction du coût du
  bâtiment. Un seul coefficient règle la lourdeur de tout le système, comme `PRIX_UNITE`.

---

## 6 bis. ✅ ARCHITECTURE SIMPLIFIÉE (vérifiée dans les sources, 2026-09-18)

> **Le plan prévoyait un prefab de chantier PAR BÂTIMENT (23 prefabs). Ce n'est pas nécessaire :
> un seul suffit.**

Le §1 ter proposait de dériver `CompositionLayoutBase` et de faire pointer son
`SCR_EditorLinkComponent` vers la composition finale. C'est faisable, mais `m_aEntries` est une
donnée **de prefab** (`[Attribute()] ref array<ref SCR_EditorLinkEntry>` sur la *ComponentClass*,
lue via `GetComponentData`) : elle ne se change pas à l'exécution. Une entrée = un prefab. D'où
les 23.

**On s'en passe en surchargeant `SpawnComposition()` nous-mêmes.** C'est une méthode publique de
`SCR_CampaignBuildingLayoutComponent`, appelée par `EvaluateBuildingStatus` quand la valeur
atteint `m_iToBuildValue`. Un `modded class` peut donc :
- porter le prefab cible, le joueur et l'angle dans des champs à nous, posés au moment où le
  chantier est créé ;
- régler `m_iToBuildValue` (champ `protected`, donc accessible depuis la classe moddée) — c'est
  **D5** ;
- faire apparaître le bâtiment FF par le chemin normal, puis se supprimer.

**Bilan : UN seul prefab de chantier générique**, sans `SCR_EditorLinkComponent`.

**Les quatre points de blocage du §1 ter, revérifiés indépendamment ce jour :**

1. `SCR_CampaignBuildingBuildUserAction.PerformAction` appelle bien
   `AddBuildingValue(GetBuildingToolValue(user), owner)`, et `GetBuildingToolValue` lit bien
   `GetToolConstructionValue()`. **La « correction » du 2026-09-17 qui disait le contraire est
   donc bien erronée** — elle avait regardé `Build()` dans le composant de gadget (effectivement
   commentée) en concluant qu'il n'y avait aucun appelant, alors que l'appelant est l'action
   utilisateur. Confirmé une 3ᵉ fois en écrivant le bonus du génie, qui exploite cette chaîne.
2. `SCR_EditorLinkComponent` : plus dans le chemin, question sans objet.
3. `SCR_CampaignBuildingNetworkComponent` vit sur `DefaultPlayerControllerMP.et` dont
   `JWK_PlayerController.et` dérive → les joueurs FF l'ont déjà.
4. `CanBePerformedScript` n'exige qu'un gadget en main. **Nuance à traiter** : n'importe quel
   gadget affiche l'action (une lampe torche montre « Construire » mais ne fait rien progresser,
   `GetToolConstructionValue()` rendant 0). À gater proprement, sinon le joueur croit à un bug.

⚠️ **Le seul prérequis restant est l'asset** : le prefab de chantier générique, dérivé de
`{...}Prefabs/Compositions/Misc/FreeRoamBuilding/CompositionLayoutBase.et` (qui porte déjà
`SCR_CampaignBuildingLayoutComponent`, l'action « Construire » en `Duration -5 / PerformPerFrame 1`,
l'action de démontage, un `RplComponent` et un `RigidBody NoCollision`). À enregistrer dans le
Workbench avant de pouvoir tester.

---

## 6 quater. ✅ LIVRÉ ET VU EN JEU (2026-09-19) — le chantier fonctionne

`FFRX_BuildSite.c` + `Prefabs/FFRX/Build/FFRX_BuildSite.et` (`{6FFEC0DEB1115171}`).
**Vérifié en jeu** : bunker posé, monté à la pelle en 58 s par un sapeur, visible pendant
tout le montage. Log : `Chantier ouvert : Bunker.et (120 supplies) -- 240 points de pelle`
puis `[Genie] Ben construit au rendement du genie : 10 -> 25 points par coup`.

**Générique dès le départ** : aucun code ne nomme un bâtiment, les 23 passent par le même
chemin.

**Cadence** : l'action verse la valeur de l'outil toutes les 5 s (pelle 10, sapeur 25).
2 points par supply, puis compression en racine au-delà de 400 points.

| Bâtiment | Supplies | Points | Seul | Sapeur |
|---|---:|---:|---|---|
| FiringPosition | 50 | 100 | 50 s | 20 s |
| Bunker | 120 | 240 | 2 min | 50 s |
| CommandPost | 500 | 890 | 7,4 min | 3 min |
| StorageLarge | 1000 | 1200 | 10 min | 4 min |
| VehicleDepot | 1500 | 1420 | 12 min | 4,7 min |

**Démontage** : action en boucle, retire le double par coup → moitié du temps de montage.
Remboursement intégral (rien n'a été consommé ; l'abattement aura du sens avec D1/D3).

**Persistance** : `$profile:FFRX_buildsites.json`, écriture groupée 10 s, reprise à
l'amorçage. Le prefab du jeu de base n'a pas d'EPF, il fallait l'écrire.

### Quatre pièges rencontrés, tous non évidents
1. **L'action « Construire » ne s'affichait pas** : `CanBeShownScript` exige
   `HasBuildingPreview()`, qui n'est vrai que si `SpawnPreview()` (chemin Conflict) a tourné.
   On répond « oui » sur nos chantiers. Le test porte sur le **nom de prefab**, pas sur un
   champ script : ceux-ci ne sont pas répliqués, or c'est le CLIENT qui affiche l'action.
2. **Le bâtiment ne pouvait pas monter** : la racine du fantôme est un `StaticModelEntity`,
   qui ne réplique pas ses changements de transformation. Il montait côté serveur et restait
   immobile chez le client. → on **repose** le visuel par paliers de 25 % au lieu de le déplacer.
3. **Le fantôme de FF n'est pas un décor** : il hérite du vrai bâtiment et porte un
   `EPF_PersistenceComponent`. Sans `PauseTracking()`, EPF rendait au redémarrage un bâtiment
   à moitié enterré, en doublon du chantier. (Piège repéré par Benji.)
4. **Première échelle fausse d'un facteur 20** : 0,1 point par supply → bunker monté en
   2 coups. Et un plafond plat à 900 écrasait les 4 plus gros au même temps.

### ⬜ Reste à faire
- **Tester la reprise après redémarrage** (poser, 2-3 coups, relancer le dédié).
- **Contrôle négatif** : un joueur hors génie ne doit pas pouvoir poser.
- 8 des 23 bâtiments n'ont **aucun `m_aGhosts`** → repli sur le prefab réel, à voir en jeu.
- Le visuel étant un vrai bâtiment, il peut être **utilisable avant la fin**. Arbitrage Benji :
  pas bloquant, on augmentera la profondeur d'enfouissement si ça se voit.
- Cosmétique : les pourcentages de l'action sont faux côté client (`m_iToBuildValue` n'est
  pas répliqué). Le décompte serveur, lui, est juste.

---

## 6 ter. Décisions déjà tranchées plus haut — conservées pour mémoire

1. **⚠️ Les matériaux REMPLACENT-ILS le coût en supplies, ou s'y ajoutent-ils ?** *(→ tranché par D3 : selon la taille.)*
   C'est la question structurante. Cumuler les deux ferait payer deux fois et rendrait le
   génie injouable. *Recommandation : les matériaux remplacent. Les supplies servent à
   ACHETER la matière, donc le coût est déjà payé à ce moment-là.*
2. **Où achète-t-on ?** Au dépôt FOB uniquement (logistique forte, trajets longs), ou
   partout où il y a du stockage (souple, mais le transport perd son sens) ?
3. **Les matériaux sont-ils persistants ?** Une caisse laissée sur le terrain survit-elle
   au redémarrage ? Lie la persistance EPF et le risque de gonflement des sauvegardes.
4. **Que devient le montage à la pelle pour les gros ouvrages ?** 30 unités de dépôt à la
   pelle, c'est long. Prévoir un véhicule du génie qui accélère (levier `m_iConstructionValue`),
   ou plafonner le temps de montage.
5. **Les plans : un par bâtiment, ou par famille ?** Un par bâtiment donne une progression
   fine mais 23 items à gérer. *Recommandation : par famille (Léger / Fortification / Lourd).*

---

## 7. Ce qui est déjà acquis techniquement

- Le **montage progressif** est natif : aucun code moteur à écrire.
- Notre **ETool porte déjà** `m_iConstructionValue 10`.
- Le **modèle matière première** est prouvé par Kodiis (`BuildPrices.c` : rondins + clous,
  `map<prefab, quantité>` d'items réels, remboursement à 50 % au démontage).
- Le mécanisme d'**ajout** d'un item de construction est prouvé (`FFRX_DepotFR`), celui du
  **retrait** aussi (`FFRX_RemoveCommandPost`).
