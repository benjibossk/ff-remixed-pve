# Récupération Anizay (depuis FF-Anizay-Ternary) — checklist

> Cible : monde `{0E15D706E35C9455}Worlds/FFANIZAY.ent` (dans FF - REMIXED - PVE). Dép SOFT vers la carte Anizay (pas de dép dure). Auteur des layers : **TernaryOperator** (autorisé, à créditer).
> ⚠️ Le seul truc buggé de Ternary = le **faction manager** (référence FR/RHS/FFAA non chargés). On NE reprend PAS son `default.layer` tel quel → on met un faction manager propre **FIA + MEI + CIV**.

## Méthode Workbench (recommandée)
1. **Garder ENABLED dans le Workbench** : le mod **Anizay** (carte) + le mod **FF-Anizay-Ternary** (source des layers/prefabs) — juste activés, PAS en dépendance de REMIXED.
2. Ouvrir le monde de Ternary, **copier les entités de chaque layer** → coller dans `FFANIZAY.ent`, **sauver le layer dans REMIXED**. (Les prefabs se résolvent par GUID tant que Anizay+Ternary sont activés.)
3. Pour l'**indépendance totale** (ne plus dépendre de Ternary) : copier aussi les **42 prefabs custom** dans REMIXED en **préservant les GUIDs** (sinon les layers cassent). Sinon, garder Ternary activé comme source d'assets.
4. **Faction manager** : NE PAS reprendre `default.layer` de Ternary → poser un `JWK_FactionManager` propre (voir bloc que je prépare : FIA/MEI/CIV).

## LAYERS à reprendre (56)
### Bases / militaire (4)
- [ ] MilitaryBases/fob_hilltop · fob_nauzad · fob_obeh · fob_thirty_palms
### Villes / settlements (~34)
- [ ] Towns : Sufian, akhund, anizay, barekzi, deraz, dola, domi, gorqan, herati, janur, khas, landay, makan_aljamal, maku, mian, murabat, musakhan, naudeh, naw, qalandar, tebbi, zaranj, zarifheyl
- [ ] Settlements : baluchan, hameed, riqay, salam
### Usines (12)
- [ ] Factories : akund_oilfield, alis_oil, asadis_oil, barezki_oil, british_mueseum_dig, hidden_arms_factory, hotel_construction, pharma_lab, sufian_oil, suspicious_meat_plant, warlord_villa
### Points d'intérêt
- [ ] Airports/landay_airfield · riqay_airfield
- [ ] FuelStations/fuel_stations
- [ ] Shops/hotel_one · hotel_two
- [ ] checkpoints · radar · radios · remote_sites
### Système / divers
- [ ] cameraPoints · generics · manual_busstops · manual_phones
- [ ] ⛔ default.layer (faction manager) → **NE PAS reprendre**, refaire propre

## PREFABS CUSTOM à reprendre (42) — préserver les GUIDs
### Bâtiments arabes / désert (~35) — `Prefabs/Models/houses/`
- [ ] tem_arab1/3/4/6/7/8/9/11/16/19/33/43/47
- [ ] tem_House_C_1, tem_house_C_5, tem_house_c_4/11/12
- [ ] tem_House_K_1/5/7/8
- [ ] tem_House_L_1/3/4/6/7_EP1
- [ ] tem_Mosque_small_1/2
- [ ] tem_building1/2/4/5_empty
- [ ] tem_villa1
### Props (3) — `Prefabs/Models/props/` + Services
- [ ] tem_helipad1 · tem_heli_decal · PhoneWall_01_grey
### Structures modifiées
- [ ] Modded/Structures/Military/Houses/Barrracks_01/Barracks_01_military_white
- [ ] Structures/Industrial/.../FuelTank_02_Pump_green · _grey
### FF wiring (à REFAIRE propre, pas copier tel quel)
- [ ] World/JWK_World_Anizay.et → notre `JWK_World` REMIXED (map offset/size, persistence `FFRX_Anizay`, road network)
- [ ] GameMode/FreedomFighters_Anizay.et → notre game mode REMIXED

*Tout extrait dispo : `C:\Users\benbo\tools\PakInspector\ffanizay_extract\`.*
