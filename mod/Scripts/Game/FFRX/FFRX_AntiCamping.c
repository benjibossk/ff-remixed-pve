// FF - REMIXED - PVE
// Anti-camping, palier 1 : tirer longtemps depuis le meme trou finit par attirer un drone.
//
// ------------------------------------------------------------------------------------
// LE PROBLEME
//
// Une equipe de tireurs d'elite peut engager indefiniment a 400 m sans jamais rien
// risquer : l'IA ne vient pas, et il suffit d'avoir des munitions. Le combat n'est pas
// difficile, il est juste long -- meme constat que pour l'assaut (FFRX_AIAssault), a
// l'autre bout de la distance.
//
// ------------------------------------------------------------------------------------
// CE QU'ON DETECTE, ET POURQUOI PAS AUTRE CHOSE
//
// On compte les TIRS, pas les morts. Compter les morts donnerait un ennemi OMNISCIENT :
// il "saurait" qu'on lui a tue trois hommes alors qu'aucun survivant n'a rien entendu.
// Les coups de feu, eux, sont perceptibles -- le jeu de base a d'ailleurs deja des
// comportements "unknown fire" (SCR_AIObserveUnknownFireBehavior) : une IA remarque un
// tir dont elle ignore l'origine et va regarder.
//
// ⚠️ Et ce n'est PAS le nombre de tirs qui declenche, c'est la DUREE de tir depuis un
// meme endroit. Une embuscade, c'est une rafale puis on decroche : ca ne doit rien
// declencher. Le camping, c'est tirer depuis le meme trou pendant des minutes. D'ou deux
// seuils CUMULES (MIN_SHOTS et MIN_SPAN_S). Un joueur qui tire beaucoup mais se deplace
// n'est jamais inquiete -- c'est exactement le comportement qu'on veut encourager.
//
// L'accumulation se fait sur le LIEU, pas sur le joueur : une signature par cellule de
// terrain, qui expire. Le pardon est donc automatique et lisible -- tu bouges, ton
// ancienne position refroidit toute seule. Rien ne "colle" au joueur.
//
// ------------------------------------------------------------------------------------
// CE QUI REND LA REACTION NATURELLE
//
// 1. ON NE REAGIT QU'A CE QUE L'ENNEMI PEUT PERCEVOIR. Au moment de declencher, on
//    verifie qu'un ennemi se trouve a portee d'oreille. Sans ca, un drone surgirait pour
//    des tirs que personne n'a pu entendre. Le test est fait UNE fois, au declenchement,
//    et pas a chaque balle : une requete spatiale par tir couterait cher pour rien.
// 2. LE DELAI EST LA CHAINE DE COMMANDEMENT. Remarquer, rendre compte, decider, lancer le
//    drone, voler jusque-la. Ce n'est pas une temporisation artificielle : c'est ce qui
//    rend la reaction credible, et ca laisse au joueur le temps de decrocher.
// 3. POSITION APPROXIMATIVE. Le renseignement est floute (SPREAD), d'autant plus que le
//    tir etait loin. Le drone fouille une zone. C'est toute la difference entre "on me
//    traque" et "le jeu triche".
// 4. LE DRONE ARRIVE DE QUELQUE PART. Il apparait a distance, hors de portee visuelle, et
//    vole jusqu'a la zone : on peut le voir venir, l'entendre, le descendre.
//
// ------------------------------------------------------------------------------------
// LE DRONE
//
// `DJIRGD5Dropper` porte `NOVA_Mavic` (mod AIUseDrones) : une fois pose, il cherche seul
// ses cibles dans un rayon et largue une grenade. On n'a donc RIEN a piloter -- on le
// depose et on lui donne sa faction (`SetAIFaction`, methode publique).
//
// ⚠️ Ce drone est ARME, pas un simple observateur. C'est assume et conforme a la demande
// ("un droniste ennemi envoie un drone pour aller les attaquer") : l'avertissement, c'est
// le drone lui-meme -- visible et audible bien avant de pouvoir agir. Le joueur qui
// comprend se deplace ; celui qui l'ignore prend la grenade.
//
// SERVEUR UNIQUEMENT.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

class FFRX_CampTuning
{
	// --- Detection ---
	static const float CELL          = 40;    // taille de cellule (m) : on tolere de bouger un peu
	static const int   MIN_SHOTS     = 7;     // en deca, c'est une embuscade, pas du camping
	static const float MIN_SPAN_S    = 90;    // duree minimale entre le 1er et le dernier tir
	static const float FORGET_S      = 120;   // sans tir pendant ce temps, la cellule refroidit

	// --- Plausibilite ---
	static const float EARSHOT       = 400;   // un ennemi plus loin que ca n'a rien pu entendre

	// --- Reaction ---
	static const float REPORT_DELAY_S = 20;   // chaine de commandement : remarquer -> rendre compte -> decider
	static const float SPREAD         = 60;   // flou du renseignement (m)
	static const float SPAWN_DIST     = 500;  // le drone apparait hors de vue et vole jusque-la
	static const float SPAWN_HEIGHT   = 60;

	// --- Garde-fous ---
	static const float COOLDOWN_S    = 300;   // par cellule : pas de vagues successives
	static const int   MAX_ACTIVE    = 2;     // plafond global simultane

	// --- Escalade (paliers 2 et 3) ---
	static const float DISPATCH_RANGE = 1500;  // au-dela, le groupe arriverait bien trop tard

	static const ResourceName DRONE_PREFAB = "{55C5BEE65087F7CD}Prefabs/Drones/DJIBase/DJIRGD5Dropper.et";
}

//------------------------------------------------------------------------------------------------
//! Signature de tir accumulee sur UNE cellule de terrain.
class FFRX_CampCell
{
	string m_sKey;
	vector m_vPos;        // position du dernier tir (plus utile que le centre de cellule)
	float  m_fFirst_s;
	float  m_fLast_s;
	int    m_iShots;
	float  m_fMuteUntil_s;  // delai de recuperation apres une reaction
	int    m_iLevel;        // palier d'escalade deja atteint sur cette position (0 = aucun)

	void FFRX_CampCell(string key, vector pos, float now_s)
	{
		m_sKey    = key;
		m_vPos    = pos;
		m_fFirst_s = now_s;
		m_fLast_s  = now_s;
		m_iShots   = 1;
	}
}

//------------------------------------------------------------------------------------------------
class FFRX_CampWatch
{
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref map<string, ref FFRX_CampCell> s_mCells;

	protected static map<string, ref FFRX_CampCell> Cells()
	{
		if (!s_mCells)
			s_mCells = new map<string, ref FFRX_CampCell>();

		return s_mCells;
	}
	protected static int s_iActive;

	//------------------------------------------------------------------------------------------------
	//! Appele depuis l'unique handler de tir (cf. FFRX_VehicleSmokeScreen).
	static void OnPlayerShot(int playerId, IEntity shooter, BaseWeaponComponent weapon)
	{
		if (!Replication.IsServer())
			return;
		if (!shooter)
			return;
		if (FFRX_Enabled() <= 0)
			return;

		World world = GetGame().GetWorld();
		if (!world)
			return;

		float now_s = world.GetWorldTime() / 1000.0;
		vector pos  = shooter.GetOrigin();
		string key  = FFRX_CellKey(pos);

		FFRX_CampCell cell;
		if (!Cells().Find(key, cell) || !cell)
		{
			Cells().Set(key, new FFRX_CampCell(key, pos, now_s));
			FFRX_Prune(now_s);
			return;
		}

		// Trou de silence : la position est consideree comme abandonnee puis reoccupee.
		// On repart de zero plutot que de cumuler deux embuscades distantes d'une heure.
		if (now_s - cell.m_fLast_s > FFRX_CampTuning.FORGET_S)
		{
			cell.m_fFirst_s = now_s;
			cell.m_iShots   = 0;
		}

		cell.m_vPos   = pos;
		cell.m_fLast_s = now_s;
		cell.m_iShots  = cell.m_iShots + 1;

		if (now_s < cell.m_fMuteUntil_s)
			return;

		// LES DEUX seuils, pas l'un ou l'autre.
		if (cell.m_iShots < FFRX_CampTuning.MIN_SHOTS)
			return;
		if (cell.m_fLast_s - cell.m_fFirst_s < FFRX_CampTuning.MIN_SPAN_S)
			return;

		FFRX_TryRespond(cell, playerId, now_s);
	}

	//------------------------------------------------------------------------------------------------
	protected static void FFRX_TryRespond(notnull FFRX_CampCell cell, int playerId, float now_s)
	{
		if (s_iActive >= FFRX_CampTuning.MAX_ACTIVE)
			return;

		// ⚠️ GARDE-FOU DEFENSEUR. Tirer beaucoup depuis un point qu'on TIENT, ce n'est pas
		// du camping, c'est le defendre -- et le systeme se retournerait contre le joueur
		// au pire moment, pendant qu'il encaisse une contre-attaque. On ne declenche donc
		// jamais en territoire ami.
		if (FFRX_OnFriendlyGround(cell.m_vPos))
			return;

		// Plausibilite : quelqu'un a-t-il pu ENTENDRE ? Teste ici seulement, une requete
		// spatiale par balle serait ruineuse.
		if (!FFRX_EnemyWithinEarshot(cell.m_vPos))
			return;

		// On arme la reaction et on met la cellule en sourdine tout de suite : sans ca,
		// les tirs des 20 prochaines secondes redeclencheraient en boucle.
		cell.m_fMuteUntil_s = now_s + FFRX_CampTuning.COOLDOWN_S;
		cell.m_iShots = 0;
		cell.m_fFirst_s = now_s;

		s_iActive = s_iActive + 1;

		// Renseignement FLOUTE : le drone fouille une zone, il ne fond pas sur une croix.
		vector target = cell.m_vPos;
		target[0] = target[0] + Math.RandomFloat(-FFRX_CampTuning.SPREAD, FFRX_CampTuning.SPREAD);
		target[2] = target[2] + Math.RandomFloat(-FFRX_CampTuning.SPREAD, FFRX_CampTuning.SPREAD);

		// ESCALADE : la reponse monte d'un cran a chaque fois que la MEME position est
		// re-signalee. Le but est d'apprendre au joueur a bouger, pas de le tuer : le
		// premier palier avertit, les suivants pesent de plus en plus lourd.
		cell.m_iLevel = cell.m_iLevel + 1;
		int level = cell.m_iLevel;

		Print(string.Format("[FFRX][AntiCamp] position tenue detectee (pid=%1) -> palier %2 dans %3 s.",
			playerId, level, FFRX_CampTuning.REPORT_DELAY_S), LogLevel.NORMAL);

		GetGame().GetCallqueue().CallLater(FFRX_Respond,
			FFRX_CampTuning.REPORT_DELAY_S * 1000, false, target, level);
	}

	//------------------------------------------------------------------------------------------------
	//! Etat lisible de la cellule du joueur, et declenchement force si `force`.
	//! Appele par la commande #camp (cf. bas de fichier).
	static string FFRX_Report(int playerId, vector pos, bool force)
	{
		if (FFRX_Enabled() <= 0)
			return "Anti-camping DESACTIVE (curseur 'Anti-camping (drone)' a 0).";

		World world = GetGame().GetWorld();
		if (!world)
			return "Monde indisponible.";

		float now_s = world.GetWorldTime() / 1000.0;

		// Les deux verrous INVISIBLES, ceux qui font qu'on peut tirer 20 fois sans rien
		// declencher : on les evalue toujours, meme en mode force, pour que le rapport dise
		// la verite sur la situation.
		bool ami   = FFRX_OnFriendlyGround(pos);
		bool oreille = FFRX_EnemyWithinEarshot(pos);

		string txt = "[Anti-camping] ";

		FFRX_CampCell cell = Cells().Get(FFRX_CellKey(pos));
		if (!cell)
		{
			txt = txt + "aucun tir enregistre sur cette cellule.";
		}
		else
		{
			int span = (int)(cell.m_fLast_s - cell.m_fFirst_s);
			txt = txt + cell.m_iShots.ToString() + "/" + FFRX_CampTuning.MIN_SHOTS.ToString() + " tirs, "
				+ span.ToString() + "/" + ((int)FFRX_CampTuning.MIN_SPAN_S).ToString() + " s, palier atteint "
				+ cell.m_iLevel.ToString();

			if (cell.m_fMuteUntil_s > now_s)
				txt = txt + ", EN SOURDINE " + ((int)(cell.m_fMuteUntil_s - now_s)).ToString() + " s";
		}

		txt = txt + " | terrain ami : " + ami.ToString() + " (si oui, rien ne part jamais)";
		txt = txt + " | ennemi a portee d'oreille (" + ((int)FFRX_CampTuning.EARSHOT).ToString() + " m) : " + oreille.ToString();
		txt = txt + " | reactions en cours " + s_iActive.ToString() + "/" + FFRX_CampTuning.MAX_ACTIVE.ToString();

		if (!force)
			return txt + " -- '#camp go' pour declencher ici.";

		// Declenchement force : on saute les SEUILS, mais on garde tout le reste du chemin
		// (palier, flou du renseignement, delai de compte rendu) pour tester ce qui est
		// reellement joue, et pas un raccourci.
		if (!cell)
		{
			cell = new FFRX_CampCell(FFRX_CellKey(pos), pos, now_s);
			Cells().Set(cell.m_sKey, cell);
		}

		cell.m_iLevel = cell.m_iLevel + 1;
		int level = cell.m_iLevel;

		vector target = pos;
		target[0] = target[0] + Math.RandomFloat(-FFRX_CampTuning.SPREAD, FFRX_CampTuning.SPREAD);
		target[2] = target[2] + Math.RandomFloat(-FFRX_CampTuning.SPREAD, FFRX_CampTuning.SPREAD);

		s_iActive = s_iActive + 1;
		GetGame().GetCallqueue().CallLater(FFRX_Respond,
			FFRX_CampTuning.REPORT_DELAY_S * 1000, false, target, level);

		Print(string.Format("[FFRX][AntiCamp] declenchement FORCE (#camp go, pid=%1) -> palier %2 dans %3 s.",
			playerId, level, FFRX_CampTuning.REPORT_DELAY_S), LogLevel.NORMAL);

		return txt + " || FORCE : palier " + level.ToString() + " dans "
			+ ((int)FFRX_CampTuning.REPORT_DELAY_S).ToString() + " s.";
	}

	//------------------------------------------------------------------------------------------------
	//! Aiguillage des paliers, appele apres le delai de compte rendu.
	protected static void FFRX_Respond(vector target, int level)
	{
		s_iActive = s_iActive - 1;
		if (s_iActive < 0)
			s_iActive = 0;

		// Palier 2 : une equipe deja presente vient voir. Palier 3 et au-dela : on
		// cherche d'abord un groupe MONTE (il arrive vite et pese lourd), a defaut
		// n'importe lequel. Si le monde n'a personne a envoyer, on retombe sur le drone
		// plutot que de faire apparaitre une escouade -- un spawn de complaisance est
		// exactement ce qui donne l'impression que le jeu triche.
		if (level >= 2)
		{
			bool wantVehicle = level >= 3;

			SCR_AIGroup g = FFRX_AssaultDirector.FFRX_FindEnemyGroupNear(
				target, FFRX_CampTuning.DISPATCH_RANGE, wantVehicle);

			if (!g && wantVehicle)
				g = FFRX_AssaultDirector.FFRX_FindEnemyGroupNear(target, FFRX_CampTuning.DISPATCH_RANGE, false);

			if (g)
			{
				int sent = FFRX_AssaultDirector.FFRX_DispatchTo(g, target);
				if (sent > 0)
				{
					Print(string.Format("[FFRX][AntiCamp] palier %1 : %2 homme(s) envoye(s) enqueter.",
						level, sent), LogLevel.NORMAL);
					return;
				}
			}

			Print(string.Format("[FFRX][AntiCamp] palier %1 : aucun groupe disponible -> repli sur le drone.",
				level), LogLevel.NORMAL);
		}

		FFRX_LaunchDrone(target);
	}

	//------------------------------------------------------------------------------------------------
	protected static void FFRX_LaunchDrone(vector target)
	{

		string enemyKey = FFRX_EnemyFactionKey();
		if (enemyKey == "")
		{
			Print("[FFRX][AntiCamp] pas de faction ennemie -> drone annule.", LogLevel.WARNING);
			return;
		}

		// Depart hors de portee visuelle, direction aleatoire : le drone ARRIVE, il
		// n'apparait pas au-dessus de la tete du joueur.
		float ang = Math.RandomFloat(0, Math.PI2);
		vector origin = target;
		origin[0] = origin[0] + Math.Cos(ang) * FFRX_CampTuning.SPAWN_DIST;
		origin[2] = origin[2] + Math.Sin(ang) * FFRX_CampTuning.SPAWN_DIST;
		origin[1] = origin[1] + FFRX_CampTuning.SPAWN_HEIGHT;

		IEntity drone = SDRC_SpawnHelper.SpawnItem(origin, FFRX_CampTuning.DRONE_PREFAB, 0, -1, false);
		if (!drone)
		{
			Print("[FFRX][AntiCamp] spawn du drone ECHOUE (prefab absent ? mod AIUseDrones desactive ?).", LogLevel.WARNING);
			return;
		}

		NOVA_Mavic mavic = NOVA_Mavic.Cast(drone.FindComponent(NOVA_Mavic));
		if (mavic)
			mavic.SetAIFaction(enemyKey);
		else
			Print("[FFRX][AntiCamp] drone sans composant NOVA_Mavic : il ne chassera pas.", LogLevel.WARNING);

		Print(string.Format("[FFRX][AntiCamp] drone ennemi '%1' lance vers %2.", enemyKey, target.ToString()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Un ennemi assez proche pour avoir entendu le coup de feu ?
	protected static bool FFRX_EnemyWithinEarshot(vector pos)
	{
		array<EntityID> ids = JWK_IndexSystem.Get().GetAll(SCR_ChimeraCharacter);
		if (!ids)
			return false;

		float maxSq = FFRX_CampTuning.EARSHOT * FFRX_CampTuning.EARSHOT;

		foreach (EntityID id : ids)
		{
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			if (!ent)
				continue;
			if (vector.DistanceSq(ent.GetOrigin(), pos) > maxSq)
				continue;

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (combat && combat.FFRX_IsEnemyAI())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Le sol sous cette position est-il tenu par notre camp ?
	//! Territoire inconnu -> on repond NON : en cas de doute, mieux vaut declencher une
	//! reaction de trop que laisser un campeur tranquille par exces de prudence.
	protected static bool FFRX_OnFriendlyGround(vector pos)
	{
		JWK_TerritoryControlSystem tc = JWK.GetTerritoryControl();
		if (!tc)
			return false;

		JWK_TerritoryControlNodeComponent node = tc.GetNodeAt(pos);
		if (!node)
			return false;

		JWK_EFactionRole role = node.GetFactionRole();
		return role == JWK_EFactionRole.PLAYER || role == JWK_EFactionRole.SUPPORTING;
	}

	//------------------------------------------------------------------------------------------------
	protected static string FFRX_EnemyFactionKey()
	{
		JWK_FactionManager fm = JWK.GetFactions();
		if (!fm)
			return "";
		JWK_Faction f = fm.GetJWKFactionByRole(JWK_EFactionRole.ENEMY);
		if (!f)
			return "";
		return f.GetKey();
	}

	//------------------------------------------------------------------------------------------------
	//! Cellule de terrain. On arrondit pour tolerer les petits deplacements : rester dans
	//! son trou en se decalant de trois metres, c'est toujours du camping.
	protected static string FFRX_CellKey(vector pos)
	{
		int cx = Math.Floor(pos[0] / FFRX_CampTuning.CELL);
		int cz = Math.Floor(pos[2] / FFRX_CampTuning.CELL);
		return cx.ToString() + ":" + cz.ToString();
	}

	//! Purge des cellules froides : sans ca la table grossit sans fin sur un serveur
	//! qui tourne des jours.
	protected static void FFRX_Prune(float now_s)
	{
		array<string> dead = {};
		foreach (string k, FFRX_CampCell c : Cells())
		{
			if (!c)
				continue;
			if (now_s - c.m_fLast_s > FFRX_CampTuning.FORGET_S && now_s > c.m_fMuteUntil_s)
				dead.Insert(k);
		}
		foreach (string k : dead)
			Cells().Remove(k);
	}

	//------------------------------------------------------------------------------------------------
	//! Curseur FF : 0 = mecanique desactivee, sans avoir a retirer le mod.
	protected static float FFRX_Enabled()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return 1;
		return c.m_fFFRX_AntiCamp;
	}
}

// ======================================================================================
//  #camp  --  RENDRE L'ANTI-CAMPING OBSERVABLE ET DECLENCHABLE
// ======================================================================================
//
// POURQUOI CETTE COMMANDE EXISTE. Le systeme a ete livre le 2026-09-10 et, au 19/09, il
// n'avait JAMAIS tourne en jeu : zero occurrence de "[FFRX][AntiCamp]" dans les logs du
// dedie. Ce n'est pas surprenant -- le declenchement demande de reunir, sur la MEME
// cellule de 40 m : 7 tirs, 90 s d'ecart entre le premier et le dernier, et un ennemi a
// moins de 400 m. Reunir tout ca volontairement en test est long et incertain, et quand
// rien ne part on ne sait pas LEQUEL des criteres a manque.
//
// C'est exactement le piege qui nous a coute plusieurs sessions sur la touche J : un
// systeme muet est indiscernable d'un systeme casse. On rend donc l'etat lisible.
//
//   #camp        -> etat de MA cellule : tirs comptes, duree, sourdine, palier atteint,
//                   et surtout les deux verrous invisibles (terrain ami / ennemi a portee)
//   #camp go     -> declenche la reaction ICI, en sautant les seuils (le reste du chemin
//                   est identique : delai de compte rendu, flou, escalade)
//
// `go` monte d'un palier a chaque appel, comme un vrai signalement repete : trois appels
// permettent donc de voir successivement le drone, l'equipe detournee, puis le groupe
// monte -- sans camper une demi-heure.
class FFRX_CampCommand : ScrServerCommand
{
	override string GetKeyword() { return "camp"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		bool force = (argv && argv.Count() > 1 && argv[1] == "go");
		return ScrServerCmdResult(FFRX_CampWatch.FFRX_Report(playerId, ent.GetOrigin(), force),
			EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement (#camp).", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
