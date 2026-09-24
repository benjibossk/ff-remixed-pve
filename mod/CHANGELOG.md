# FF - REMIXED - PVE — Journal des versions

Texte destiné au champ « changelog » du Workshop. Une section par publication,
la plus récente en haut.

---

## 2026-09-10

### Comportement de l'IA au feu
- **L'IA ne soigne plus un camarade à découvert sous le feu.** Un médecin se levait
  au milieu de l'échange pour panser un blessé, offrant deux cibles immobiles.
  La cause était une priorité : dans le jeu de base, soigner un camarade (111) passe
  **au-dessus des réflexes de survie** (mise à couvert, repli, ~110-125). Bohemia avait
  d'ailleurs prévu une garde — `MAX_THREAT_THRESHOLD` — mais ne l'a jamais branchée.
  Sous menace, le soin passe désormais derrière la survie ; il reprend seul au calme.
  Réglable (« Discipline de soin IA », 0 = comportement d'origine).
- **Les blessés sont traînés à couvert avant d'être soignés.** Un camarade valide
  saisit le blessé, s'éloigne de la menace vers un vrai couvert — c'est le moteur qui
  le choisit, pas nous — le repose, et le soin peut alors avoir lieu. Nécessite
  ACE Carrying.

### Menace asymétrique
- **Voiture piégée roulante (VBIED)** : une voiture civile conduite par un ennemi fonce
  sur un joueur et explose au contact. Contrairement à la voiture piégée statique, la
  menace est **visible et neutralisable** — tirer sur le conducteur arrête l'attaque.
  Une menace qu'on ne peut pas contrer serait une taxe, pas un défi.
- **Le kamikaze à pied vient vous chercher.** Il restait immobile là où il apparaissait.
  Il progresse maintenant en zigzag, et **se met à couvert** quand on lui tire dessus
  avant de reprendre — l'IA du jeu s'en charge, on lui laisse simplement la main.
- **Voitures civiles piégées réglables** : proportion de véhicules piégés et plafond de
  pièges actifs pilotables depuis les réglages FF. À 0 %, le système est coupé sans
  retirer le mod. Le piège reste une vraie mine ACE, donc **détectable au détecteur**.

### Grades et galons
- **Nouvelle échelle de grades française à 18 échelons**, de Déserteur à Colonel,
  alignée **1:1 sur les galons AMF disponibles**. Trois grades ajoutés :
  Caporal-Chef de 1ère Classe, Sergent-Chef BM2 et Aspirant. Il n'y a
  volontairement pas de Général : AMF ne fournit pas le galon, donc le grade
  n'existe pas — un grade qui ne se voit pas n'a pas d'intérêt.
- **Le galon se pose tout seul** sur la tenue, au grade réel du soldat, au spawn
  **et à chaque changement de tenue**. Galon discret quand le soldat porte un
  gilet de combat, voyant en t-shirt ou en tenue de cérémonie.
- **Les galons sont retirés de l'arsenal** (67 entrées désactivées) : on ne
  choisit plus son grade dans une caisse. Les autres patchs (patronymique,
  groupe sanguin, insignes de régiment, opérations) restent disponibles.

### Génie et construction
- **Nouvelle dotation « Genie »**, réservée à l'escouade ECHO. Un joueur d'une
  autre escouade qui tente de la prendre reçoit un message explicite au lieu
  d'un refus muet.
- **N'importe quelle pelle devient un outil de construction**. Jusqu'ici seules
  deux pelles FF précises fonctionnaient ; une pelle vanilla, visuellement
  identique, ne servait à rien.
- **Nouvelle commande `#pelle`** (ouverte à tous) : explique pourquoi on ne peut
  pas construire — outil en main, outil reconnu, zone de construction, et
  nombre d'objets réellement constructibles à l'endroit où l'on se trouve.

### Équipement
- **La radio Thalès rentre enfin dans la poche radio** du gilet modulaire AMF.
- **Brouilleur installable sur un véhicule** : on pose son sac sur un véhicule
  équipé du support, et on peut le reprendre. La charge de batterie suit le sac.
- **Correction d'une boucle infinie du sac brouilleur** qui saturait le journal
  du serveur (32 600 lignes en 4 minutes). La cellule de rechange livrée avec le
  sac n'est plus dévorée à l'apparition.

### Accueil et vie serveur
- **La cinématique d'introduction** se déclenche désormais sur le **temps de jeu
  réel** (moins de 30 minutes) et non plus sur l'expérience, qui plafonnait et
  pouvait la rejouer indéfiniment.

### Correctifs
- **Fin d'un spam d'erreurs** dans la console serveur lié au mode de combat de
  l'IA, déclenché à chaque groupe en cours d'apparition ou de disparition.

### Outillage (administration)
- **Recensement des spawns ennemis** : le serveur compte ce qui apparaît
  réellement — types de groupe, soldats, véhicules et leurs proportions — et le
  publie sur la page **Recensement** du site. Commande `#census`.
