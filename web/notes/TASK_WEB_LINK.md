# Tâche — liaison compte web ↔ UID en jeu (code)

But : lier un compte web/Discord à l'identité en jeu (UID) pour débloquer les permissions
« ses propres » (marqueurs, objectifs) + l'affichage nominatif.

## Flux
1. **Web** : l'utilisateur connecté génère un **code de liaison** (ex. 6 chiffres), affiché sur le
   site. Le web stocke `code -> compte (pending)` avec une expiration (ex. 10 min).
2. **Jeu** : le joueur tape en chat **`#link <code>`**.
3. **Fleet** → `POST /api/v1/link` (Bearer, `server_id` via la clé) avec :
   ```json
   { "code":"123456", "uid":"7a69...","name":"Ben" }
   ```
4. **Web** : matche `code` → lie le **compte** à cet **uid** (+ mémorise le `name`). Réponse 200.

## Go à faire
- Endpoint **`POST /api/v1/link`** (Bearer, comme les autres push Fleet).
- Cherche le code pending → si trouvé & non expiré : enregistre `account.reforger_uid = uid`
  (table users, à côté du lien Discord). Supprime le code. Renvoie 200.
- Si code inconnu/expiré : 200 quand même (ou 404) — Fleet ne réagit pas au corps, log seulement.
- (Le web a déjà `admin/users/:id/link` pour un lien manuel admin — ça, c'est le lien **auto par code**.)

## Utilisation du lien (une fois établi)
- **Marqueurs** : `ownerUid` (déjà envoyé par Fleet) == `account.reforger_uid` → le joueur peut
  déplacer/supprimer SES marqueurs depuis le site.
- **Objectifs / grades** : idem, cibler/afficher par UID lié.

## Notes
- `#link` est ouvert à **tous** les joueurs (chat), traité **serveur**.
- Fleet log `[GTGPOS] Lien envoyé : code=… uid=…` quand la commande est tapée.
- Sécurité : le code est à usage unique + expirant (généré côté web).
