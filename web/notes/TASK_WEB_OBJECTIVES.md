# Tâche — assigner un objectif (tâche J) depuis la livemap

Côté jeu = **DÉJÀ PRÊT** : Fleet reçoit l'ordre `objective` (via le canal `commands`) et crée une
tâche standard (`SCR_TaskSystem`) → elle **apparaît dans le menu J** du/des joueur(s) visé(s).
Il ne reste que l'**UI web** pour envoyer l'ordre.

## Commande (POST `/api/v1/command`, session, `credentials:'include'`)
```json
{ "server_id":"...", "type":"objective",
  "objtarget":"player",           // "player" | "squad" | "faction"
  "uid":"<uid>",                  // si player
  "id":12,                        // si squad (= id d'escouade, cf. /squads)
  "faction":"US",                 // si faction (clé)
  "objname":"Tenir le pont",      // titre
  "objdesc":"Défendre jusqu'à 18h",// description
  "x":4520, "y":8110 }            // position monde (mètres ; x=est, y=Z nord ; 0,0 = sans lieu)
```

## UI proposée (map.html)
Deux entrées possibles :
1. **Clic droit sur un joueur / une escouade** (dans le panneau UNITÉS ou sur la carte) →
   menu « Assigner un objectif » → petit formulaire (titre, description, + option « choisir le lieu »).
   - joueur : `objtarget:'player'`, `uid` = `p.playerGuid`.
   - escouade : `objtarget:'squad'`, `id` = `squad.id`.
2. **Bouton « Objectif faction »** (barre d'outils) → `objtarget:'faction'`, `faction` = faction affichée.

Pour la **position** : soit optionnelle (0,0 = pas de lieu), soit « cliquer sur la carte » →
`leafletToArma(e.latlng)` (cf. `TASK_WEB_MARKERS.md` §1) → `x`, `y`.

## Notes
- `x`=X (est), `y`=**Z monde (nord)**, en **mètres** (comme les marqueurs).
- Réservé aux **admins** (auth session owner/admin), comme les autres commandes.
- Latence : l'ordre part en file → Fleet le tire au prochain tick (~5 s) → tâche visible en jeu.
- (Plus tard) sens jeu → objectif : un bouton sur la carte in-game (côté mod), pas cette tâche.
- (Optionnel) suivi de l'état des objectifs sur le web : nécessiterait que Fleet pousse la liste
  des tâches + leur état — pas fait pour l'instant (cette tâche = création/assignation seulement).
