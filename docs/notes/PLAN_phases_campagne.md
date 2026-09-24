# Phases de campagne — état des lieux & proposition

> 🔴 P1 de la roadmap : le « chef d'orchestre » qui débloque ressources / matériel / zones par palier.
> Ce document sert à **décider avant de coder**. Rien n'est implémenté à ce stade.
> Date : 2026-09-08.
>
> **🅿️ CHANTIER EN PAUSE (décision Benji, 2026-09-08).** Motif : FF impose déjà une
> linéarité de fait (il faut des usines pour produire, une base pour la MOB, et
> Réoccupation escalade toute seule) — un système de phases apporterait donc moins que
> d'autres chantiers. À reprendre quand le reste sera stabilisé.
> **Arbitrage acquis : 5 phases** (et non 4). Le tableau §3 est à redécouper en
> conséquence — pas encore fait.

---

## 1. Ce qui existe déjà — et c'est beaucoup

### FF a un moteur de progression complet, et on ne s'en sert pas

`JWK_GameProgressManagerComponent` mesure en permanence l'avancement de la campagne, découpé en **6 mécaniques** (`JWK_EGameProgressMechanic`) :

| Mécanique | Entité suivie | Poids | Requis pour gagner |
|---|---|---|---|
| `TOWN_CAPTURE` | `JWK_TownEntity` | 100 | 100 % |
| `BASE_CAPTURE` | `JWK_MilitaryBaseEntity` | **250** | 100 % |
| `FACTORY_CAPTURE` | `JWK_FactoryEntity` | 100 | 100 % |
| `RADIO_TOWER_CAPTURE` | `JWK_RadioSiteEntity` | 100 | 100 % |
| `RADAR_SITE_CAPTURE` | `JWK_RadarSiteEntity` | 100 | 100 % |
| `PROPAGANDA_POSTERS` | — | 100 | **0 %** (compte dans la barre, pas dans la victoire) |

Chaque contrôleur s'abonne à `GetOnFactionControlChanged()` des sites indexés : la progression est **événementielle**, pas sondée. `GetTotalProgressPercentage()` renvoie la moyenne pondérée 0-100.

**Le point important : ce moteur ne fait que MESURER.** Rien dans FF ne consulte ce pourcentage pour débloquer quoi que ce soit. C'est une jauge, pas une vanne. On l'affiche déjà sur la page Économie ; c'est le seul usage qu'on en fait.

### Ce qui joue déjà un rôle de palier chez nous, sans être un système

- **MOB sur base militaire** : il faut prendre une base pour installer une MOB (hiérarchie de spawn MOB > FOB > Ville).
- **Économie** : usines et stations à capturer pour produire ; le ravitaillement plafonne ce qu'on peut acheter et construire.
- **Réoccupation** : escalade ennemie autonome (death squads, contre-attaques) — la difficulté monte toute seule avec le temps et nos prises.
- **XP Fleet** : l'intro est gatée à 500 XP.
- **Coût** : tout le matériel est déjà limité par les supplies.

C'est ce que tu voulais dire par « c'est déjà un peu fait » — et c'est exact. **Il manque une seule chose : la progression ne débloque rien.** Tout est disponible dès J1, seul le prix freine.

### Ce qu'on sait déjà manipuler

- Ajouter / retirer un bâtiment constructible : prouvé par `FFRX_DepotFR` (ajout) et `FFRX_RemoveCommandPost` (retrait).
- Filtrer un catalogue d'arsenal : prouvé par `InventoryItems_EntityCatalog_FR_DESERT.conf`.
- Servir des dotations différentes selon le contexte : `FFRX_LoadoutTheatre`.
- Piloter le procurement véhicule : `FFRX_Procurement` / `FFRX_ProcurementShop`.

Autrement dit **toutes les vannes existent déjà**. Il ne manque que le robinet qui les commande.

---

## 2. Ce qu'il reste vraiment à faire

Un composant qui :
1. lit l'avancement (le moteur FF, éventuellement enrichi),
2. en déduit une **phase** courante (1..N),
3. **applique** les déblocages de cette phase,
4. l'annonce aux joueurs et le persiste.

### Faisabilité technique — vérifiée

- `GetTotalProgressPercentage()` est **public** → lisible sans bidouille.
- Il n'y a **aucun ScriptInvoker** sur le changement de progression. On récupère l'événement en `modded class JWK_GameProgressManagerComponent` avec un `override` de `RefreshTotalProgress_S()` (`protected`, donc surchargeable) qui appelle `super` puis émet notre propre signal. Pas de sondage.
- `SetMechanicProgress_S()` est **public** : on peut alimenter nos propres mécaniques.
- On peut ajouter des mécaniques via `modded enum JWK_EGameProgressMechanic` + un contrôleur maison, en surchargeant `Configs/GameProgress.conf`.
  ✅ **Vérifié : Réoccupation ne touche pas à la progression** (aucune référence dans son pak 6.0.0). Le fichier n'a donc qu'un seul propriétaire possible — nous. Pas de collision à craindre, contrairement à `GameSettings.conf`.

### Le vrai risque : gater par le pourcentage global est trop grossier

La barre FF est une moyenne pondérée. À 30 % on ne sait pas **ce qui** a été pris : trois villes sans aucune usine, ou une base et deux usines. Deux situations qui n'appellent pas du tout les mêmes déblocages.

**Recommandation : gater sur des conditions LISIBLES, pas sur un pourcentage.**
« Vous tenez 1 base militaire et 2 usines » se comprend, se raconte, et se vise. « Vous êtes à 34 % » ne se joue pas. Les mécaniques FF sont déjà comptées séparément (`GetMechanicProgress`), donc c'est aussi simple à lire que le total.

---

## 3. Proposition — 4 phases

Structure suggérée, à ajuster librement (les seuils sont des points de départ, pas des convictions) :

| Phase | Nom | Condition d'entrée | Ce que ça ouvre |
|---|---|---|---|
| 1 | **Clandestinité** | départ | Armes légères, véhicules civils, FOB, dépôt. L'état actuel du jeu à J1, mais amputé du haut de gamme. |
| 2 | **Insurrection** | 1 usine **et** 2 villes | Armement d'infanterie complet, véhicules militaires légers, premières constructions défensives. |
| 3 | **Force organisée** | 1 base militaire | MOB, blindés, mortiers, ateliers, hangar. C'est le vrai saut de puissance. |
| 4 | **Offensive** | 2 bases **et** 50 % du territoire | Blindé lourd, aérien, bâtiments en dur (tour de contrôle, bunkers). |

**Une phase ne redescend jamais.** Perdre une base réactive la pression de Réoccupation, mais ne reprend pas le matériel déjà acquis : rétrograder punirait deux fois et rendrait la logistique illisible (des véhicules achetés deviendraient soudain interdits).

### Ce que ça change concrètement pour les joueurs

Aujourd'hui, prendre une base militaire donne un point de spawn et des supplies. Demain, ça **ouvre un palier** annoncé à tout le serveur. La capture devient un objectif collectif lisible, et l'ordre des opérations se met à compter — c'est exactement le manque que pointait la roadmap.

---

## 4. Découpage proposé

1. **`FFRX_CampaignPhase`** — lecture de la progression, calcul de la phase, événement de changement, persistance, annonce serveur + commande admin `#phase`. *Aucun déblocage.* Testable seul : on voit la phase monter en jouant.
2. **Table de déblocage** — quelle phase ouvre quoi. C'est du **contenu**, pas du code : à remplir avec toi.
3. **Application** — branchement sur les vannes existantes (catalogues, construction, procurement).
4. **Affichage** — phase visible en jeu et sur la page Économie (l'historique est déjà là pour tracer la courbe).

L'étape 1 est indépendante et sans risque. Les étapes 2-3 demandent tes arbitrages.

---

## 5. Ce que j'attends de toi

**La table de déblocage** (étape 2). C'est un choix de game design, pas une question technique : dire quel matériel appartient à quelle phase, c'est décider du rythme de ta campagne. Je peux proposer un premier jet à partir des catalogues existants, mais l'arbitrage final t'appartient.

Deux questions annexes :
- **Combien de phases ?** 4 est un compromis ; 3 est plus lisible, 5 plus progressif.
- **Les phases sont-elles par théâtre ?** Une campagne sur Anizay repart-elle en phase 1, ou hérite-t-elle d'Everon ? (Mon avis : elle repart à zéro — sinon le second théâtre n'a plus de courbe de progression.)
