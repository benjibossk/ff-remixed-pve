# Tâche — marqueurs bidirectionnels sur la livemap (façon Anarchy)

But : sur la livemap, **poser / déplacer / éditer / supprimer** des marqueurs qui apparaissent
**en jeu**, avec une UI qui **reproduit celle d'Anarchy** (pour ne pas perdre les joueurs).

**Côté jeu = DÉJÀ PRÊT** : Fleet poll `GET /api/v1/commands` et applique `place/move/edit/remove`
(crée/déplace/édite/supprime le marqueur Anarchy + broadcast à tous). L'endpoint navigateur
`POST /api/v1/command` existe (`commands.go`, auth session, `server_id` dans le body).
**Tout le travail restant est frontend (map.html).**

---

## 1. Coordonnées : clic Leaflet → mètres monde (inverse de `armaToLeaflet`, ~L455)
```js
function leafletToArma(latlng){
  if (!currentMapConfig || currentMapConfig.ScaleX === undefined)
    return { x: Math.round(latlng.lng), z: Math.round(latlng.lat) };
  const sX=currentMapConfig.ScaleX??1, sY=currentMapConfig.ScaleY??1;
  const oX=currentMapConfig.OffsetX??0, oY=currentMapConfig.OffsetY??0;
  let p = LCRSCustomSimple.projection.project(latlng);
  let t = LCRSCustomSimple.transformation.transform(p);
  t.x=(t.x-oX)/sX; t.y=(t.y-oY)/sY;
  p = LCRSCustomSimple.transformation.untransform(t);
  const a = LCRSCustomSimple.projection.unproject(p);
  return { x: Math.round(a.lng), z: Math.round(a.lat) };  // lng=X(est), lat=Z(nord)
}
```
Toutes les commandes : `x`=X (est), `y`=**Z monde (nord)**, en **mètres**. (⚠️ y ≠ altitude.)

## 2. Commandes (POST `/api/v1/command`, body JSON, `credentials:'include'`)
```
{ server_id, type, id, x, y, kind, icon, ident, dim, sym, color, rot, size, vis, ch, text }
```
| type | champs | effet |
|---|---|---|
| `place`  | x,y + champs marqueur | crée |
| `move`   | id, x, y | déplace |
| `edit`   | id + tous les champs | édite |
| `remove` | id | supprime |

## 3. UI de création = COPIE d'Anarchy
Double-clic sur une zone vide → dialogue avec :

**a) Type (`kind`)** : `0` = **Civil**, `1` = **Militaire**.

**b) Visibilité / canal (`vis`)** — mêmes libellés + couleurs qu'Anarchy :
| vis | libellé | couleur |
|---|---|---|
| 0 | **Local** (auteur seul) | gris `#AAAAAA` |
| 1 | **Group** | vert `#49C24A` |
| 2 | **Side** (faction) | bleu `#2E6FE6` |
| 3 | **Global** (tous) | rouge `#D83A3A` |

**c) Si Civil** → grille d'**icônes** + **palette de couleurs** :
- Palette (ARGB, alpha FF) — **exactement la palette Anarchy** :
  `magenta 255,0,236` · `orange 242,166,35` · `brun 194,100,20` · `rouge 218,7,7` ·
  `vert 4,113,20` · `bleu 1,50,216` · `bleu clair 4,139,228` · `rose 222,10,180` · `violet 72,21,90`
- Icônes (`icon` = index) : imageset `{E23427CAC80DA8B7}UI/Textures/Icons/icons_mapMarkersUI.imageset`.
  Catégories : **General, Arrow (flèches), Maneuver (manœuvre)**… → réutiliser ces icônes
  (extractibles du .pak) ou un jeu équivalent. `icon` = position dans la catégorie.

**d) Si Militaire (APP-6)** → 3 sélecteurs :
- **Identité (`ident`)** : Inconnu `0` · **Allié** (BLUFOR) `1` · **Ennemi** (OPFOR) `2` · **Neutre** (INDFOR) `3`
- **Dimension (`dim`)** : Aucune `0` · **Sol** (LAND) `1` · Installation `2` · Install. air `3` · **Air** `4` · **Équipement** `5`
- **Symbole (`sym`, bitflags)** : Infanterie `1` · Motorisé `2` · Blindé `4` · Antichar `8` · Mortier `16` ·
  Artillerie `32` · Aile fixe `64` · Hélico `128` · Recon `256` · Ravitaillement `512` · Médical `2048` …
  (valeurs = `EMilitarySymbolIcon`, `1<<n`. On peut combiner par OR, mais commencer par un seul suffit.)

**e) Taille (`size`)** : pourcentage, **défaut 200**. **Texte (`text`)** : libellé libre.
Champs non utilisés → `0` ; `ch` (canal group/faction) → `-1` (le serveur le règle).

## 4. Déplacer / éditer / supprimer
- **Déplacer** : rendre les marqueurs Leaflet `draggable`, sur `dragend` → `POST {type:'move', id, x, y}`.
- **Éditer** : clic → rouvrir le dialogue prérempli → `POST {type:'edit', id, ...}`.
- **Supprimer** : bouton corbeille dans le dialogue → `POST {type:'remove', id}`.

## 5. Permissions (côté web — le jeu applique tout ce qu'on lui envoie)
Chaque marqueur poussé par le jeu a un champ **`owner`** (id du poseur ; `-1` = posé serveur/web).
- **Admin** (session owner/admin du serveur) : peut **déplacer / éditer / supprimer N'IMPORTE QUEL** marqueur, et en poser.
- **Joueur** (utilisateur web lié à son perso en jeu) : peut **déplacer/supprimer SES propres** marqueurs
  (`marker.owner == son id en jeu`). ⚠️ **Dépendance** : il faut lier le compte web ↔ l'identité en
  jeu (même problématique que les grades : Discord/UID ↔ playerId/owner). Tant que ce lien n'existe
  pas, limiter l'écriture aux **admins** ; le « joueur bouge ses points » se fait déjà **en jeu** via
  Anarchy. À activer côté web quand le mapping compte↔joueur sera en place.

## 5bis. Auteur + date (affichage + PERSISTANCE)
Chaque marqueur poussé par le jeu porte **`author`** (nom du poseur ; "Web" si posé depuis le
site) et **`created`** (Unix secondes = date de pose). → afficher dans le tooltip/popup :
« posé par `author` le `new Date(created*1000)` ».

⚠️ **PERSISTER côté web pour garder la vraie date d'origine.** Fleet re-stampe `created` à la 1re
fois qu'il voit le marqueur **par session** → après un redémarrage serveur, les anciens marqueurs
reprennent la date du redémarrage. Pour figer la **date/heure de pose d'origine** :

1. **Stockage persistant** `(server_id, marker_id) → { created, author }` (table SQL, ex.
   `marker_meta`, OU fichier JSON à côté — au choix ; markers sont en mémoire aujourd'hui donc
   il faut une petite table dédiée). Clé = `server_id + marker_id` (l'id Anarchy est stable et
   persiste entre sessions).
2. **À chaque `POST /api/v1/markers`** (handler `markers.go`) : pour chaque marqueur reçu,
   - si `(server_id, marker_id)` **inconnu** → enregistrer `{created, author}` reçus (1re pose),
   - si **déjà connu** → **écraser** `created`/`author` du marqueur par les valeurs **stockées**
     (on ignore le re-stamp de Fleet), de sorte que la date affichée reste celle d'origine.
3. Servir aux frontends les marqueurs avec ces `created`/`author` d'origine.
4. (option) purge : si un `marker_id` n'apparaît plus dans N pushs, on peut supprimer sa ligne
   `marker_meta` (ou la garder pour un historique).

→ Résultat : la date et l'heure de pose sont **conservées** même après redémarrage du serveur de jeu.

## 6. Notes
- Placement/édition conseillés **en mode LIVE**.
- Latence : l'ordre part en file → Fleet le tire au prochain tick (~5 s) → effet en jeu, puis
  retour sur la livemap au push `/markers` suivant. (Optionnel : affichage optimiste immédiat.)
- UI riche déjà prototypée dans `D:\Serveur\marker-web` (formulaire complet) — bonne base.
