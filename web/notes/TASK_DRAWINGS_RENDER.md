# Tâche — rendre les dessins (crayon Anarchy) sur la carte

Les dessins arrivent déjà côté serveur : Fleet POST `/api/v1/drawings`, stockés, servis par
`GET /api/v1/public/drawings/:server_id`. **Vérifié live** — exemple réel :
```json
[ { "id":6, "owner":1, "color":-12224, "w":0, "vis":2, "ch":0, "fill":0,
    "author":"Ben", "pts":[2769,3998, 2768,3995, 2766,3992, 2765,3989] } ]
```
**Il ne manque QUE le rendu frontend** (map.html) — aucune couche dessins n'existe aujourd'hui.

## Format
- `pts` = **paires x,z à plat** en **mètres monde** (même espace que les marqueurs) → utiliser
  `armaToLeaflet([x, z])` sur chaque paire pour obtenir les latlng.
- `color` = **ARGB** (int signé, même convention que marqueurs/escouades) → convertir avec
  `fltArgbToCss(color)` (ou `fltRgbSolid` si tu veux ignorer l'alpha, comme pour les escouades).
- `w` = index d'épaisseur (0,1,2… → mappe sur un `weight` px, ex. `weight: 2 + w*2`).
- `fill` = 0/1 : si 1, fermer en polygone rempli (`L.polygon`), sinon `L.polyline`.
- `vis`/`ch` = visibilité/canal (peut être ignoré pour l'affichage, ou filtrer plus tard).

## À faire (map.html) — calqué sur les escouades/marqueurs
1. Créer une couche : `let drawingsLayer = L.layerGroup().addTo(map);`
2. Fonction `setDrawings(list)` :
   ```js
   function setDrawings(list){
     drawingsLayer.clearLayers();
     (Array.isArray(list) ? list : []).forEach(d => {
       const pts = d.pts || [];
       const latlngs = [];
       for (let i = 0; i + 1 < pts.length; i += 2) latlngs.push(armaToLeaflet([pts[i], pts[i+1]]));
       if (latlngs.length < 2 && !d.fill) return;           // point isolé -> ignore
       const css = fltArgbToCss(d.color);
       const w = 2 + (d.w|0) * 2;
       if (d.fill) L.polygon(latlngs, { color: css, weight: w, fillColor: css, fillOpacity: 0.25 }).addTo(drawingsLayer);
       else        L.polyline(latlngs, { color: css, weight: w }).addTo(drawingsLayer);
     });
   }
   ```
3. Fetch en **LIVE** (dans la même boucle que `setSquads`) :
   ```js
   try { setDrawings(await fetchData(`public/drawings/${serverId}`)); } catch (_) { setDrawings([]); }
   ```
4. En relecture / stop live : `setDrawings([])` (comme les escouades — pas d'historique dessins).

## Notes
- Coords = **mètres monde**, jamais relatif [0,1]. `armaToLeaflet` existe déjà.
- Les dessins bidirectionnels web→jeu (créer un dessin depuis le web) ne sont PAS demandés ici :
  juste **afficher** ceux faits en jeu. (La création reste une étape ultérieure.)
