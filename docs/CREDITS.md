# FF - REMIXED - PVE — Crédits

## CRX Enfusion A.I. — ATiM-

**Mod :** [CRX Enfusion A.I.](https://reforger.armaplatform.com/workshop/5F268647F8A1A1F4-CRXEnfusionA.I.)
**Auteur :** ATiM-
**Workshop GUID :** `5F268647F8A1A1F4`

CRX **n'est pas une dépendance** de REMIXED. Nous avons voulu l'ajouter, mais ses 220 scripts
font déborder le budget global d'instructions d'initialisation statique du module de script
« Game » du moteur, ce qui casse la compilation de tout le projet (erreurs en cascade
« Too many instructions per function » dans le jeu de base et dans FF).

Nous avons donc **réécrit nous-mêmes**, contre l'API vanilla, les comportements qui nous
intéressaient. **Aucune ligne de code de CRX n'est copiée** — mais le mérite d'avoir identifié
ces défauts de l'IA revient à ATiM-, et c'est en lisant son travail que nous les avons repérés.

Comportements concernés (dans `Scripts/Game/FFRX/FFRX_AIDifficulty.c`) :

| Comportement | Défaut vanilla corrigé |
|---|---|
| Les équipages tiennent leur tourelle | Un servant de tourelle sans conducteur dont la cible sort de son cône de tir débarque pour enquêter à pied — il abandonne une arme lourde pour son fusil. Le vanilla ne l'interdit que pour les APC. |
| Délai de riposte réglable | Un ennemi surpris attend jusqu'à ~1 s à 300 m avant d'ouvrir le feu. |

Si le budget de scripts le permet un jour (par exemple après un allègement des dépendances),
CRX mérite d'être utilisé directement : il fait bien plus que ce que nous avons repris
(réactions à la densité de feu, combat rapproché, gestion des couverts, mouvement de groupe,
attributs Game Master).

## AIReflexFire — cjcn

**Mod :** [AIReflexFire](https://reforger.armaplatform.com/workshop/6A34762D41CD50CC-AIReflexFire)
**Auteur :** cjcn
**Workshop GUID :** `6A34762D41CD50CC`

Également **pas une dépendance** : ce mod applique des valeurs figées à toute l'IA, alors
que nous voulions des curseurs réglables en jeu et un ciblage des ennemis là où c'est possible.
Le code est le nôtre (`Scripts/Game/FFRX/FFRX_AIReflex.c`), mais **le mérite d'avoir identifié
ces deux défauts revient à cjcn**.

| Comportement | Défaut vanilla corrigé |
|---|---|
| Temps de visée | `m_fBaseStabilizationTime` (0,4 s) et `m_fBaseRejectionTime` (1 s) s'appliquent **avant chaque tir**, en boucle : c'est la principale raison pour laquelle l'IA paraît molle une fois engagée. |
| Réflexe au contact | En déplacement, le comportement de mouvement l'emporte sur celui d'attaque : une IA qui croise un ennemi à bout portant continue de courir au lieu de tirer. |

## WCS_LoadoutEditor — équipe WCS

**Mod :** [WCS_LoadoutEditor](https://reforger.armaplatform.com/workshop/61D57616CAFBB23D)
**Workshop GUID :** `61D57616CAFBB23D`

**Pas une dépendance** : aucun de leur code n'est repris et le mod n'apporte aucun asset à
REMIXED. Mais c'est en lisant leur source qu'on a compris comment injecter proprement de
l'UI dans le menu d'inventaire du jeu de base, après plusieurs tentatives ratées de notre
côté (barre de catégories créée mais invisible). **Le mérite de ces deux constats leur
revient** ; le code est le nôtre (`Scripts/Game/FFRX/FFRX_ArsenalVanillaFilter.c`).

| Ce qu'on leur doit | Détail |
|---|---|
| Construction de l'UI | Ne **jamais** empiler des widgets bruts sous un parent quelconque : sans slot qui les dimensionne, ils s'effondrent à 0×0 — présents, « visibles », invisibles. Eux chargent systématiquement un `.layout` dans un conteneur **nommé** du layout vanilla (`CreateWidgets(layout, parent)`). |
| Filtrage par catégorie | Se fait côté **données**, en surchargeant la récupération des items (`WCS_GetItemsOfCategory`) — ce qu'on faisait déjà, et que leur code confirme comme la bonne voie. |

## Freedom Fighters — JohnnyKerner

Mod de base dont REMIXED est une extension.
Documentation modding : <https://www.johnnykerner.dev/FreedomFighters/modding/>
