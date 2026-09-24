# Tâche — loadouts nommés (dotations par spécialité)

But : un **admin** crée un loadout **en jeu** (il s'équipe puis `#loadout publish <nom>`), Fleet
l'envoie au site. Plus tard, un joueur le **charge en jeu** (via une caisse/arsenal, à venir).
Le site est le **stockage** + l'UI d'admin (renommer / supprimer / réassigner à une spécialité).

## Modèle de données
Un loadout = `{ name, faction, data }` :
- `name`   : nom libre choisi par l'admin (ex. "Assaut", "Medic jour"). **Clé** (avec faction).
- `faction`: clé de faction du perso au moment de la capture (ex. "US", "USSR"). Peut être "".
- `data`   : **chaîne JSON opaque** = le loadout complet (arbre inventaire). **Ne pas le parser
  ni le modifier** côté site — le stocker tel quel et le renvoyer **verbatim**. C'est Fleet qui
  le produit (capture) et le consomme (application). Il peut faire plusieurs Ko.

Reco stockage : table `loadouts(id, faction, name, data TEXT, updated_at)`, unique `(faction, name)`.

## Endpoints Go à faire (Bearer, comme les autres push Fleet)

### 1. `POST /api/v1/loadouts`  — créer / mettre à jour
Corps envoyé par Fleet :
```json
{ "name": "Assaut", "faction": "US", "data": "{\"clothing\":[...],\"weapons\":[...]}" }
```
- Upsert par `(faction, name)` : si existe -> remplace `data`, sinon insert.
- Réponse 200 (Fleet ne lit pas le corps de réponse, log seulement). `data` peut être volumineux
  (prévois une limite de taille de corps généreuse, ~256 Ko).

### 2. `GET /api/v1/loadouts`  — lister (et fournir le data pour l'application)
- Renvoie **tous** les loadouts (ou filtrable `?faction=US` plus tard) :
```json
{ "loadouts": [
  { "name": "Assaut", "faction": "US", "data": "{...json opaque...}" },
  { "name": "Medic",  "faction": "US", "data": "{...}" }
] }
```
- Fleet appelle ça pour `#loadout list` (n'affiche que les noms) **et** pour `#loadout apply <nom>`
  (retrouve le `data` du bon nom et l'applique). Donc **inclure `data`** dans la réponse.
- ⚠️ Renvoie `data` **exactement** comme reçu (même chaîne). Pas de reformat JSON.

### 3. (admin, plus tard) suppression / renommage
- `DELETE /api/v1/loadouts/:id` ou par `(faction,name)`.
- UI d'admin : liste des loadouts, bouton supprimer/renommer, et **assignation à une "spécialité"**
  (catégorie libre) pour que la future caisse-arsenal en jeu les regroupe par spécialité.
  -> pour ça, tu peux ajouter un champ `specialty` (string) au modèle, alimenté depuis l'UI web
     (Fleet ne l'envoie pas encore ; défaut "").

## Notes
- Auth : `Authorization: Bearer <clé>` (même clé que positions/markers/…).
- `#loadout publish` est réservé admin côté jeu (permission RCON admin). Le POST est donc de
  confiance (serveur -> site).
- Étape suivante côté jeu (2C) : une **caisse arsenal** qui `GET /loadouts`, affiche la liste
  groupée par spécialité, et applique au clic. On te redemandera peut-être un filtre par
  `specialty`/`faction` à ce moment-là.
