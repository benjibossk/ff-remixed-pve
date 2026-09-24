# Message a TacticalFlava — poids du module de scripts

> Redige le 2026-09-18. Version francaise ci-dessous, version anglaise en fin de fichier.
> Mesures faites sur le pak Workshop `TacticalFlava_5D550926D43F1409` extrait localement.

---

## Version francaise

Salut,

Je developpe un mod de mission (FF - REMIXED - PVE, un mode PVE sur base Freedom Fighters)
et on utilise TacticalFlava sur notre serveur — le contenu est excellent, c'est vraiment du
beau boulot, en particulier les armes et les tenues.

Je te contacte pour un sujet technique qui n'est pas un bug, mais qui nous gene a l'usage :
**le volume de scripts du mod**, et le fait qu'on ne puisse pas n'en prendre qu'une partie.

### Les chiffres

J'ai extrait le pak Workshop et compte les declarations de classes :

| | Fichiers `.c` | Classes |
|---|---:|---:|
| `Scripts/` (TacticalFlava) | 182 | 321 |
| `RHS_Scripts/` | 448 | 801 |
| **Total** | **630** | **1 122** |

Soit **71 % des classes sous `RHS_Scripts/`** : radios, missiles guides, APS, artillerie et
contre-batterie, systeme de call-in, menu radial, carte GPS, visees 2D. Les plus gros
fichiers sont `TF_Handler_CounterBattery.c` (83 Ko), `RHS_RadioComponent.c` (68 Ko),
`RHS_APSV2Component.c` (56 Ko), `RHS_GuidedWeaponStationComponent.c` (46 Ko).

A titre de comparaison, notre mod complet — qui porte tout un mode de jeu — fait 254 classes.
TacticalFlava en represente **4,4 fois plus**.

### Pourquoi c'est un probleme pour nous

Arma Reforger a un **plafond de compilation des scripts** a l'echelle du module `Game`.
Quand on s'en approche, le symptome n'est pas explicite : on obtient
`Can't compile "Game" script module!`, souvent accompagne d'erreurs incoherentes sur des
fichiers **sans rapport**, y compris des fichiers du jeu de base, ou des
`Too many instructions per function`. C'est tres couteux a diagnostiquer, parce que rien
ne pointe vers la vraie cause.

Notre serveur fait tourner une quarantaine de mods, dont plusieurs a scripts. Chaque
dependance compte, et TacticalFlava est de loin la plus lourde de notre liste.

Le point important : **tout se compile, meme ce qu'on n'utilise pas.** Concretement, on a
ajoute TacticalFlava en dependance pour **un seul objet** — le SOFLAM, qu'on rhabille en
jumelles francaises Thales OB-72 pour garder le guidage laser. On paie donc 1 122 classes,
dont l'artillerie, la contre-batterie, les APS et tout le systeme radio, pour une paire de
jumelles. Beaucoup d'autres serveurs sont probablement dans le meme cas avec un vehicule ou
une arme.

### Ce qu'on propose

**Decouper les SCRIPTS en modules, pas seulement les assets.**

On a deja decoupe TacticalFlava par assets en local, pour nos tests : Core / Weapons /
Clothes / TIGR / HMMWV / M-ATV / BM-21 / Turrets / 2B9Vasilek. Ca marche tres bien pour le
poids en Mo et le temps de telechargement. **Mais ca ne soulage pas le budget de scripts**,
parce que le code reste groupe.

L'idee serait donc de separer aussi les sous-systemes lourds, pour qu'un consommateur ne
dependre que de ce qu'il emploie :

- **TF-Core** : uniquement la plomberie partagee (helpers, composants de base).
- **TF-GuidedWeapons** : missiles guides, APS, stations d'arme, camera TV.
- **TF-Artillery** : artillerie automatique, contre-batterie, call-in.
- **TF-Radio** : `RHS_RadioComponent` et ce qui en depend.
- **TF-UI** : menu radial, carte GPS, visees 2D (136 fichiers d'UI a eux seuls).
- Les packs de vehicules / armes / tenues ne dependant que du minimum necessaire.

Deux pistes complementaires, si un decoupage complet est trop lourd :

1. **Ne pas livrer les scripts d'editeur dans le module runtime** — il y a des
   `WorkbenchGame/WorldEditor` et `Game/Editor` dans le pak, qui n'ont pas besoin d'etre
   compiles sur un serveur dedie.
2. **Regrouper les petites classes.** Beaucoup de fichiers ne declarent qu'une ou deux
   classes tres courtes ; a 1 122 classes, la consolidation peut rapporter.

### Ce que je ne pretends pas

Pour etre honnete sur la methode : je mesure depuis le pak extrait, je ne vois ni votre
arborescence source ni vos contraintes, et il y a surement de bonnes raisons a
l'organisation actuelle. Je ne dis pas non plus que TacticalFlava depasse le plafond a lui
seul — c'est la somme des mods qui compte, et chacun a sa part. Je signale simplement que
c'est le plus gros contributeur de notre liste, et que le decoupage nous debloquerait.

Si ca vous interesse, je peux fournir le detail du comptage par dossier, et on peut tester
un decoupage de votre cote sur notre serveur (une quarantaine de mods, dedie sous charge
reelle) avant que vous ne publiiez quoi que ce soit.

Merci pour le mod, et pour le temps de lecture.

---

## English version

Hi,

I develop a mission mod (FF - REMIXED - PVE, a PVE mode built on Freedom Fighters) and we
run TacticalFlava on our server. The content is excellent — really nice work, especially
the weapons and the clothing.

I'm reaching out about a technical point. It isn't a bug, but it does get in our way:
**the sheer volume of scripts in the mod**, and the fact that consumers can't take only
part of it.

### The numbers

I extracted the Workshop pak and counted class declarations:

| | `.c` files | Classes |
|---|---:|---:|
| `Scripts/` (TacticalFlava) | 182 | 321 |
| `RHS_Scripts/` | 448 | 801 |
| **Total** | **630** | **1,122** |

So **71% of the classes sit under `RHS_Scripts/`**: radios, guided missiles, APS, artillery
and counter-battery, the call-in system, radial menu, GPS map, 2D sights. The largest files
are `TF_Handler_CounterBattery.c` (83 KB), `RHS_RadioComponent.c` (68 KB),
`RHS_APSV2Component.c` (56 KB), `RHS_GuidedWeaponStationComponent.c` (46 KB).

For comparison, our entire mod — which implements a whole game mode — is 254 classes.
TacticalFlava is **4.4x that**.

### Why this is a problem for us

Arma Reforger has a **script compilation ceiling** at the `Game` module level. As you
approach it, the symptom is not explicit: you get `Can't compile "Game" script module!`,
often with incoherent errors on **unrelated** files — including base-game files — or
`Too many instructions per function`. It is expensive to diagnose, because nothing points
at the real cause.

Our server runs about forty mods, several of them script-heavy. Every dependency counts,
and TacticalFlava is by far the heaviest on our list.

The key point: **everything compiles, including what we never use.** We added TacticalFlava
as a dependency for **one single item** — the SOFLAM, which we reskin as a French Thales
OB-72 to keep the laser designation. So we pay 1,122 classes, including artillery,
counter-battery, APS and the whole radio system, for a pair of binoculars. Plenty of other
servers are probably in the same position over one vehicle or one weapon.

### What we'd suggest

**Split the SCRIPTS into modules, not just the assets.**

We already split TacticalFlava by assets locally for our own testing: Core / Weapons /
Clothes / TIGR / HMMWV / M-ATV / BM-21 / Turrets / 2B9Vasilek. That works well for download
size. **But it does not relieve the script budget**, because the code stays bundled.

The idea would be to separate the heavy subsystems too, so a consumer only depends on what
it actually uses:

- **TF-Core**: shared plumbing only (helpers, base components).
- **TF-GuidedWeapons**: guided missiles, APS, weapon stations, TV camera.
- **TF-Artillery**: automatic artillery, counter-battery, call-in.
- **TF-Radio**: `RHS_RadioComponent` and its dependents.
- **TF-UI**: radial menu, GPS map, 2D sights (136 UI files on their own).
- Vehicle / weapon / clothing packs depending only on the minimum they need.

Two lighter alternatives if a full split is too much work:

1. **Don't ship editor scripts in the runtime module** — there are `WorkbenchGame/WorldEditor`
   and `Game/Editor` scripts in the pak that don't need compiling on a dedicated server.
2. **Merge small classes.** Many files declare only one or two very short classes; at 1,122
   classes, consolidation adds up.

### What I'm not claiming

To be straight about method: I measured from the extracted pak, I can't see your source
layout or your constraints, and there are surely good reasons for the current organisation.
I'm also not claiming TacticalFlava breaks the ceiling on its own — it's the sum of all mods
that matters, and everyone contributes. I'm only saying it's the largest single contributor
on our list, and that a split would unblock us.

If it's of interest, I can share the per-folder breakdown, and we can test a split build on
our server (about forty mods, dedicated, under real load) before you publish anything.

Thanks for the mod, and for reading this far.
