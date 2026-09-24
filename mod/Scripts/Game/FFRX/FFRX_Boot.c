// FF - REMIXED - PVE
// Amorcage central de nos systemes serveur -- et RE-amorcage apres un hot-reload.
//
// ------------------------------------------------------------------------------------
// LE PROBLEME QU'ON REGLE ICI
//
// Tous nos systemes serveur demarrent par un `Boot()` appele au demarrage de la partie
// (`SCR_BaseGameMode.OnGameModeStart`, cf. FFRX_Groups.c). Chacun se protege d'un double
// appel par un booleen statique du genre `if (s_bStarted) return;`, et pose ensuite un
// `CallLater` periodique.
//
// Un HOT-RELOAD (Shift+F7) detruit la machine virtuelle : les `CallLater` disparaissent
// et les statiques repassent a false. Mais `OnGameModeStart` ne se rejoue PAS -- la
// partie, elle, n'a pas redemarre. Resultat : apres un Shift+F7, **plus aucun de nos
// systemes ne tourne**, en silence.
//
// C'est un piege couteux en debug : on modifie un systeme, on recompile, on teste, il ne
// se passe rien -- et on cherche le bug dans le code alors que le systeme est simplement
// eteint. (Cas reel : le diagnostic d'accoutumance a l'arme, invisible sur 3 reloads.)
//
// ------------------------------------------------------------------------------------
// LA SOLUTION
//
// Un `GameSystem` sert de second point d'amorcage. Contrairement a un evenement de mode
// de jeu, un GameSystem est RECREE quand la VM se recharge : son `OnUpdate` repart donc
// apres chaque Shift+F7, et il rallume tout ce qui s'est eteint.
//
// Les deux chemins coexistent sans conflit : chaque `Boot()` est idempotent (verifie un
// par un), donc etre appele deux fois au demarrage ne fait rien de plus.
//
// ------------------------------------------------------------------------------------
// LA SEULE EXCEPTION : FFRX_DefaultFob
//
// Son `Boot()` n'est PAS idempotent, et c'est voulu : il remet `s_bDone`/`s_iAttempts` a
// zero puis replanifie la pose du FOB de depart, parce qu'un REDEMARRAGE de mission
// detruit le FOB pose sans reinitialiser les statiques.
//
// Le ré-amorcer apres un simple hot-reload n'aurait aucun sens : la partie tourne, le FOB
// est deja la. D'ou le parametre `atGameStart` -- lui seul reste reserve au vrai
// demarrage. (Son garde reel reste le test "un FOB existe-t-il deja ?" dans TryPlace,
// donc meme une erreur ici ne poserait pas deux FOB ; on evite juste le travail inutile.)
//
// ⚠️ LIMITE CONNUE -- le directeur d'assaut (FFRX_AIAssault) n'est pas dans cette liste.
// Il ne s'amorce pas par un `Boot()` mais quand un groupe s'enregistre a son `EOnInit`.
// Apres un hot-reload, les groupes DEJA presents ne se re-enregistrent pas : l'assaut
// redemarre au fur et a mesure des nouveaux spawns (frequents chez FF), pas instantanement.
// Enumerer les groupes existants demanderait un chemin fragile ; on prefere l'attente.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

class FFRX_Boot
{
	//! Allume tous nos systemes serveur.
	//! `atGameStart` = true uniquement depuis OnGameModeStart (cf. FFRX_DefaultFob ci-dessus).
	static void All(bool atGameStart)
	{
		if (!Replication.IsServer())
			return;

		// FF/Reoccupation state emitter to the admin livemap (endpoint /ffstate).
		FFRX_FFStateSender.Boot();
		// One-time FF reference catalogs (endpoint /ffcatalog).
		FFRX_CatalogSender.Boot();
		// Spawn census: counts what actually spawns (groups / soldiers / vehicles) and
		// prints a table every 10 min. Read-only, changes nothing. Command: #census.
		FFRX_SpawnCensus.Boot();
		// Adaptive threat: the enemy fields AT / SAM specialists in the zones where the
		// players actually hurt him (armour kills, helicopter flight time). Command:
		// #menace. Set "Menace adaptative - intensite" to 0 to switch it off.
		FFRX_AdaptiveThreat.Boot();
		// Night perception: the engine models neither darkness nor NV optics for the AI.
		// Enemies without a night optic see worse after 21:00. Setting: "Vue de
		// l'ennemi la nuit" (100 = off).
		FFRX_NightVision.Boot();
		// Suicide vests: a few enemy fighters charge the player and blow up. Different
		// from FFRX_SuicideBombers, which spawns lone CIVILIAN bombers. Setting:
		// "Gilets suicide %" (0 = off).
		FFRX_SuicideVest.Boot();
		// Tracantes : le moteur ne relie pas les balles lumineuses a la perception de l'IA.
		// Un joueur qui tire a la tracante est mieux repere par les ennemis qui le VOIENT.
		// Reglage : "Tracantes - l'ennemi repere mieux %" (0 = off). Commande : #tracante.
		FFRX_TracerWatch.Boot();
		// Chantiers de construction : reprend ceux qui etaient en cours si le serveur a
		// redemarre (notre propre JSON -- le prefab de chantier n'a pas de persistance EPF).
		FFRX_BuildSiteStore.Boot();
		// Named loadouts (dotations) fed by the site -> crate actions (endpoint /loadouts).
		FFRX_LoadoutClient.Boot();
		// Rank insignia: put the AMF patch matching the player's real rank on the kit he
		// spawns with. Clothing CHANGES are handled elsewhere (OnItemAdded hook in
		// FFRX_JammerBatteryDrop -- only one modded inventory manager allowed per addon).
		FFRX_RankPatch.Boot();
		// Evacuation des blesses IA a couvert avant soin (complement de FFRX_HealDiscipline).
		FFRX_CasualtyEvac.Boot();

		// Place the single default starting FOB (fresh campaign only) -- demarrage SEULEMENT.
		if (atGameStart)
			FFRX_DefaultFob.Boot();

		// Marine resupply: periodic supply vessel in the sea near the FOB (Pillar 3).
		FFRX_MarineResupply.Boot();
		// Enemy air resupply: periodic enemy chopper reinforcing/resupplying an enemy point (Pillar 6/7).
		FFRX_EnemyAirResupply.Boot();
		// Asymmetric warfare -- booby-trapped civilian cars in enemy territory (Pillar 4, brick C).
		FFRX_BoobyTrapCars.Boot();
		// VBIED : voiture piegee ROULANTE lancee sur un joueur en territoire ennemi. Le pendant
		// MOBILE (donc lisible, donc neutralisable au tir) de la voiture piegee ci-dessus.
		FFRX_VBIEDSpawner.Boot();
		// IED poses A L'AVANCE sur les points d'interet de la carte : le danger appartient au
		// LIEU et non au joueur, contrairement aux trois briques ci-dessus (Pillar 4).
		FFRX_IEDScatter.Boot();
		// Carcasses de decor piegees : une minorite des epaves de la carte porte une charge, pour
		// que fouiller une epave redevienne un pari (Pillar 4).
		FFRX_TrappedWrecks.Boot();
		// Balise GPS : tic serveur qui decharge les balises ALLUMEES (Pilier renseignement).
		FFRX_Beacons.Boot();
		// PNJ guide : un officier pres du drapeau, briefing d'accueil SUR ACTION.
		FFRX_GuideNPC.Boot();
		// Tours emettrices construites : elles etendent la couverture radio (balises + livemap).
		FFRX_RadioTowers.Boot();
		// Sites radio FF sur Anizay : la carte n'en avait AUCUN, donc tout le systeme de
		// signal etait en "hors de portee" permanent et les depeches Reoccupation inertes.
		FFRX_RadioSites.Boot();
		// Auto-placed enemy minefields on ACE mines (Pillar 4, brick D -- ex-FFMI, now REMIXED-owned).
		FFRX_MinePlacement.Boot();
		// Civilian suicide bombers near player groups in contested territory (Pillar 4, brick B).
		FFRX_SuicideBombers.Boot();
		// Disguised enemy spies (civilian -> soldier on approach) (Pillar 4, brick A).
		FFRX_DisguisedSpies.Boot();
		// Persistent civilian identities (Pillar 4 RPG, phase 1 -- name + memory, robust scanner).
		FFRX_CivRoster.Boot();
		// Le fichier civil part au site toutes les 5 min (endpoint /ffcivs, page Civils).
		FFRX_CivSender.Boot();
		// Civilian counter-espionage: hostile/low-trust civilians report the player (D7/D9).
		FFRX_CounterEspionage.Boot();
		// Accoutumance a l'arme : le serveur compte le temps arme en main et pousse le
		// facteur de stabilite aux clients (cf. FFRX_WeaponFamiliarity.c).
		FFRX_Familiarity.Boot();
	}
}

// ------------------------------------------------------------------------------------
//  Le filet de securite : rallume tout apres un hot-reload
// ------------------------------------------------------------------------------------
class FFRX_BootSystem : GameSystem
{
	//! Une seule tentative par vie de VM. Remis a false par le rechargement lui-meme,
	//! ce qui est exactement le declencheur qu'on veut.
	protected bool m_bDone;

	override static void InitInfo(WorldSystemInfo outInfo)
	{
		outInfo
			.SetLocation(WorldSystemLocation.Server)
			.SetAbstract(false)
			.AddPoint(ESystemPoint.FixedFrame);
	}

	override protected void OnUpdate(ESystemPoint point)
	{
		if (m_bDone)
			return;
		if (point != ESystemPoint.FixedFrame)
			return;

		// On attend que la partie soit reellement en cours. Amorcer pendant le chargement
		// ferait echouer les systemes qui interrogent le monde (index d'entites, factions,
		// territoire) et leur garde `s_bStarted` les empecherait de reessayer.
		if (JWK_GameModeSystem.S_GetState() != SCR_EGameModeState.GAME)
			return;

		m_bDone = true;

		// atGameStart = false : ce chemin sert le hot-reload. Au vrai demarrage,
		// OnGameModeStart est deja passe avant nous.
		FFRX_Boot.All(false);

		Print("[FFRX][Boot] systemes serveur (re)amorces.", LogLevel.NORMAL);
	}
}
