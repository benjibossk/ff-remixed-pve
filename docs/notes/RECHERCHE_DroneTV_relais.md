# Compte-rendu — Mod **DroneTV** & faisabilité d'un flux drone longue-portée vers le camp

> Analyse demandée par Benji (2026-08-24). Question : *un général de l'état-major, depuis la TV du camp, peut-il voir le retour vidéo d'un drone qui est sur le combat ? Quelle distance ? Peut-on faire un relais (le drone stream le plus loin possible, puis c'est re-streamé) ?*

---

## 1. Le mod analysé
- **Nom** : **DroneTV** — GUID `69FF3BAB5CC20685` — version **1.0.11**.
- **Dépendances** : base game + **RealisticCombatDrones** `65AD60E204191D37` (le MÊME mod drone que REMIXED utilise déjà) + **ThermalPostProcessAssets** `69623A037A721B91` (mode caméra thermique). → **Aucun conflit** avec notre stack, pas de grosse dépendance en plus.
- Source extraite : `tools/PakInspector/dronetv_extract/Scripts/Game/DroneTV/` (classes `SAL_*`).

## 2. Comment ça marche
Système d'**interception de flux de drone**, façon **scanner radio** :
1. Un drone **émet sur une fréquence** (`SAL_DroneFrequencyComponent`).
2. Un **récepteur (« catcher »)** — soit **portable** (`SAL_HandheldScreenComponent`), soit une **antenne fixe** — intercepte le flux s'il est **accordé sur la bonne fréquence ET la bonne distance** (tuning fréquence + range, `SAL_TuningActions`/`SAL_RetuneActions`, triangulation via `SAL_TriangulateAction`).
3. Une **TV** (`SAL_TV_ScreenComponent`) branchée sur l'antenne **affiche le flux**.
4. Il faut être **à ≤ 25 m de la TV** pour la regarder (`m_fRenderDistance = 25`).

## 3. Les PORTÉES (réponse directe)
Mesurées **depuis le récepteur / l'antenne** (donc depuis le camp) jusqu'au drone (`SAL_DroneCatcherComponent`) :

| Distance drone ↔ antenne | Résultat |
|---|---|
| **≤ 300 m** | interception récepteur seul (`m_fScanRadius = 300`) |
| **≤ 800 m** | interception **boostée par antenne** (`m_fBoostedScanRadius = 800`) ; le récepteur doit rester à **≤ 50 m** de l'antenne (`m_fAntennaLinkRange = 50`) |
| **> ~1500 m** | **impossible** — plafond dur du **streaming réseau** (voir §4) : « un drone non streamé au client ne peut pas être rendu et s'affiche comme un appareil cassé » (commentaire du mod) |

Le design **pénalise volontairement la distance** : à portée max il faut un accord **4× plus fin** pour garder l'image (`m_fRangeLockEdge = 0.25`). Le dev l'écrit noir sur blanc : ça **force l'opérateur à se rapprocher physiquement du drone et de son pilote**. C'est un outil de **proximité** (guerre électronique / contre-drone), pas un lien QG→front.

## 4. Pourquoi un vrai relais VIDÉO longue-portée n'est pas praticable
Point technique décisif (`SAL_TV_ScreenComponent`, lignes 60-70) : **le flux TV n'est PAS une vidéo transmissible**. C'est un **rendu 3D live, refait localement sur la machine de chaque spectateur** — la TV attache une caméra au drone et **rend la scène réelle** (`RenderTargetWidget.SetWorld(...)`, même mécanisme que les lunettes PIP des optiques). « Purement visuel côté client, ne fait rien sur un serveur headless ».

Conséquence : pour que la TV du camp affiche un drone au front, le **client du camp** doit avoir **streamé** (a) le drone ET (b) toute la scène que la caméra voit (terrain + **unités ennemies** en dessous). Or le moteur Enfusion **ne réplique aux clients que les entités proches** : `networkViewDistance` (500-5000 m, défaut ~1500 m chez nous) plafonne la distance de réplication, « la priorité de transmission est inversement proportionnelle à la distance » (BI/Enfusion). → Les ennemis au front **ne sont pas répliqués** au client du camp.

**Donc même en « forçant » le relais :**
- Augmenter `networkViewDistance` = brute force, coûte CPU + bande passante pour TOUT, monte avec le nombre de joueurs. Non ciblé, non tenable pour un grand théâtre.
- Forcer la réplication du **drone seul** vers le camp → la TV rendrait du **terrain vide sans les ennemis** (les contacts, eux, ne sont pas streamés) → aucune valeur tactique.
- « Le drone stream loin puis on re-stream » : Enfusion **n'a pas** d'encodage/transport de pixels vidéo. Le « flux » = un rendu de scène ; on ne peut pas capturer une image et la ré-émettre à bas coût. Il n'existe pas d'API pour ça.
- Une **chaîne de relais** (antennes réparties) ne change rien : chaque TV doit **rendre** le drone → elle a quand même besoin que le drone + la scène soient streamés chez le spectateur.

**Verdict flux vidéo** : un relais vidéo QG↔front n'est **pas réaliste** dans Enfusion. La limite n'est pas le rayon du mod, c'est le **moteur** (rendu live = entité + scène doivent être présents chez le spectateur).

## 5. Ce qui EST faisable (alternatives à proposer)
Le drone longue-portée peut alimenter le QG **non pas en VIDÉO, mais en TÉLÉMÉTRIE / tactique** — et ça, on est déjà bien placés pour le faire :

- **A. Flux tactique sur la carte / livemap (recommandé)** — le drone au front pousse **sa position + les contacts repérés** (blips) vers la carte admin / le site (on a déjà `NOVA` intel côté drones + le canal `/ffstate` → GTG-livemap). Le général « suit » le drone et voit **ce qu'il repère** en marqueurs temps réel, sans contrainte de streaming. Pas d'image, mais l'info tactique qui compte. **Faisable avec nos briques existantes.**
- **B. DroneTV en proximité (tel quel)** — une TV au camp qui capte les drones **à ≤ 800 m** : défense/contre-drone du camp, ambiance QG, espionner les drones qui passent. Bon usage du mod sans le détourner.
- **C. Écran « déporté » près du front** — un opérateur avec récepteur portable **près du drone** voit la vraie vidéo ; il relaie **verbalement / par marqueurs** au QG. (C'est le modèle voulu par le mod.)
- **D. (lourd, déconseillé)** monter `networkViewDistance`/`streamingRadius` sur le secteur — coût perf global, à réserver à une petite carte / faible effectif.

## 6. Reco
Pour l'idée « le QG suit un drone au combat » : partir sur **A (télémétrie carte/livemap)**, pas sur un flux vidéo. Garder **DroneTV** pour l'usage **proximité camp** (B), où il est excellent et compatible (même dep drone que nous). Un vrai flux vidéo longue-portée = mur moteur, pas un réglage.

---

### Sources
- [Arma Reforger — Configure View Distance (XGamingServer)](https://xgamingserver.com/docs/arma-reforger/view-distance)
- [Arma Reforger Network Tweaking — Replication Layer & Bandwidth (mygamehost)](https://mygamehost.net/wiki/configuring-the-network-replication-layer-and-combating-latency-in-arma-reforger)
- [Server Performance Tuning — streamingRadius (Loafhosts)](https://loafhosts.com/guides/arma-reforger-server-performance-tuning)
- Code du mod : `dronetv_extract/Scripts/Game/DroneTV/SAL_DroneCatcherComponent.c`, `SAL_TV_ScreenComponent.c`, `SAL_CatcherMenuUI.c`.

---

## Contre-épreuve 2026-09-21 — KNS-dev : la conclusion tient

Benji a remonté **KNS-dev** (`9F02A85C31BD6E47`, *Kuka Neighbouring System*), qui annonce des
« flux bodycam en direct entre coéquipiers ». Assez pour rouvrir la question posée ici.
Mod extrait et lu — **2 fichiers, 370 lignes**. Verdict : rien ne change.

**Le mécanisme**, dans `KUKA_BodycamComponent.SpawnCameraAndOverlay()` :

```c
m_pCameraEntity = CameraBase.Cast(GetGame().SpawnEntityPrefab(...));
owner.AddChild(m_pCameraEntity, boneIdx, EAddChildFlags.AUTO_TRANSFORM);
camManager.SetCamera(m_pCameraEntity);
```

Il fait apparaître une caméra accrochée à un **os** du coéquipier, puis **déplace la caméra
locale du spectateur dessus**. Aucun encodage, aucun flux, aucun relais : c'est le client du
spectateur qui **rend la scène** depuis la tête de l'autre. Exactement le même schéma que
DroneTV, donc exactement le même mur.

**Pourquoi ça ne porte pas loin.** Un client ne peut rendre que ce que le serveur lui
**streame**. Observer quelqu'un à plusieurs kilomètres reviendrait à rendre une zone non
chargée → décor vide ou incohérent. Le mod **n'a aucun contrôle de distance** — uniquement un
filtre de faction (`UserAction_ViewBodycam.c:64`). Il ne résout pas le problème : il ne le
rencontre pas, tant qu'on reste entre voisins. Son nom le dit.

**Conclusion inchangée : pas de retransmission vidéo longue portée vers le QG.** La réponse
reste la **télémétrie et les marqueurs** — c'est précisément ce que fait la balise GPS livrée
le même jour (`FFRX_Beacon.c`), avec en prime une qualité de signal qui décroît avec la
distance aux relais tenus.

**Ce que KNS vaut quand même** : voir ce que voit un coéquipier **proche** (un chef de groupe
qui bascule sur la bodycam de son éclaireur à 200 m). Très léger (596 Ko, 0 dépendance), mais
4 téléchargements, v0.0.13 publiée le jour même, et il embarque des scripts — donc il pèse sur
le plafond de compilation. À ne pas mettre en dépendance sans test isolé.
