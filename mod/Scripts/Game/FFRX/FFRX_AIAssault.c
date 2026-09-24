// FF - REMIXED - PVE
// Assaut : quand un groupe ennemi accroche un joueur a distance, une PARTIE du groupe
// va le chercher au lieu de rester a se faire tirer comme au pigeon.
//
// ------------------------------------------------------------------------------------
// LE PROBLEME
//
// Un groupe vanilla engage a 200 m se met en position et echange des coups de feu. Le
// joueur, lui, a une lunette et du temps : il les descend un par un sans jamais etre
// menace. Le combat n'est pas difficile, il est juste long.
//
// On ne veut PAS rendre l'IA plus precise (c'est le levier de FFRX_AIDifficulty, et
// pousse trop loin ca donne des snipers injouables). On veut la rendre PRESSANTE :
// obliger le joueur a bouger, a se replier, a gerer une menace qui se rapproche.
//
// ------------------------------------------------------------------------------------
// POURQUOI L'IA VANILLA N'AVANCE PAS DEJA
//
// Elle essaie, mais deux choses l'en empechent :
//
// 1. PRIORITES. `SCR_AIAttackBehavior` sort a 70 (cible non selectionnee) ou 90
//    (selectionnee). Le comportement de deplacement `SCR_AIMoveAndInvestigateBehavior`
//    sort a 64. Tant qu'il y a une cible connue, TIRER gagne toujours contre AVANCER.
//    L'IA se deplace un peu (les "combat moves" de l'attaque) mais ne franchit jamais
//    la distance.
//
// 2. GARNISONS. `SCR_AIGroupUtilityComponent.IsPositionAllowed` interdit d'attaquer une
//    position hors du rayon d'un `SCR_DefendWaypoint`. Les garnisons FF sont clouees a
//    leur rayon par conception. (Note : `m_fMaxAutonomousDistance` vaut 9999 par defaut,
//    donc ce n'est PAS le blocage pour les patrouilles libres -- verifie.)
//
// On regle les deux d'un coup en poussant nous-memes un comportement de deplacement
// avec une priorite CHOISIE, directement sur l'agent. Ca court-circuite le processeur
// de clusters, donc le verrou de waypoint de defense ne s'applique pas : une garnison
// sortira de son batiment pour venir nous chercher.
//
// ------------------------------------------------------------------------------------
// LE CHOIX DE LA PRIORITE -- c'est le coeur du reglage
//
// On sort a 95 (voir ASSAULT_PRIORITY) :
//   - au-dessus de l'attaque normale (70 / 90) -> l'assaillant avance vraiment ;
//   - EN DESSOUS de 110-125 : repli sous le feu, soin critique, sortie d'un vehicule
//     en flammes, jet de grenade. Un assaillant garde donc ses reflexes de survie ;
//   - tres en dessous des priorites "danger immediat" (1000+), notamment
//     PRIORITY_BEHAVIOR_ATTACK_HIGH_PRIORITY (1120), qui se declenche quand la cible
//     MET EN DANGER l'IA. Autrement dit : des que le joueur les prend reellement a
//     partie de pres, l'assaut s'efface et ils se battent. C'est exactement ce qu'on
//     veut -- pas des zombies qui courent en ligne droite sans tirer.
//
// ------------------------------------------------------------------------------------
// L'ASSAUT EST BORNE (et c'est delibere)
//
// On ne laisse pas un groupe en assaut permanent : il courrait sans combattre. Un
// groupe alterne BOND (ASSAULT_DURATION_S, il avance) puis PAUSE (ASSAULT_COOLDOWN_S,
// il reprend le comportement normal et se bat). Ca produit un mouvement par bonds
// successifs, qui se trouve etre aussi la facon reelle de progresser sous le feu.
//
// Et on ARRETE d'assaillir sous ASSAULT_STOP_DIST : au contact, l'IA vanilla se
// debrouille mieux que nous. Notre travail est de fermer la distance, pas de refaire
// le combat rapproche.
//
// ------------------------------------------------------------------------------------
// PORTEE : ENNEMIS SEULEMENT
//
// `FFRX_IsEnemyAI()` (role de faction, cf. FFRX_AIDifficulty) -- pas de chemin de
// prefab, donc ca couvre USSR sur Everon, MEI au desert et toute faction ennemie
// ajoutee plus tard. Nos IA alliees gardent le comportement vanilla : un allie qui
// charge tout seul est un allie mort.
//
// SERVEUR UNIQUEMENT : l'IA n'existe que sur l'autorite.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie
// desynchronise sur l'UTF-8).

//------------------------------------------------------------------------------------------------
class FFRX_AssaultTuning
{
	//! Priorite du comportement d'assaut. Voir le long commentaire ci-dessus : cette
	//! valeur est le reglage le plus sensible du fichier.
	static const float ASSAULT_PRIORITY = 95;

	static const int   TICK_MS             = 4000;  // cadence de decision du directeur
	static const float ASSAULT_DURATION_S  = 22;    // duree d'un bond
	static const float ASSAULT_COOLDOWN_S  = 26;    // pause entre deux bonds (ils se battent)
	static const float ASSAULT_STOP_DIST   = 45;    // au contact, on rend la main a l'IA vanilla
	static const float ASSAULT_MAX_DIST    = 900;   // au-dela, la cible n'est pas credible
	static const float ARRIVE_RADIUS       = 12;    // rayon d'arrivee du deplacement
	static const float SPREAD              = 14;    // dispersion laterale entre assaillants

	//! Part du groupe envoyee a l'assaut, en % (reglage FF ; 0 = systeme desactive).
	static float AssaultFraction()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return 0.5;
		return c.m_fFFRX_AssaultPct / 100.0;
	}

	//! En deca de cette distance on n'assaille pas (reglage FF).
	static float MinDistance()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return 80;
		return c.m_fFFRX_AssaultMinDist;
	}
}

//------------------------------------------------------------------------------------------------
//! Etat d'assaut d'UN groupe. On garde l'etat au niveau du GROUPE, pas de l'agent : une
//! vague d'assaut est une decision collective, et ca evite une table par soldat.
class FFRX_AssaultEntry
{
	SCR_AIGroup m_Group;
	float m_fNextPush_s;      // prochaine decision autorisee (temps monde, secondes)

	void FFRX_AssaultEntry(SCR_AIGroup g)
	{
		m_Group = g;
	}
}

//------------------------------------------------------------------------------------------------
class FFRX_AssaultDirector
{
	protected static ref array<ref FFRX_AssaultEntry> s_aEntries;
	protected static bool s_bStarted;

	//------------------------------------------------------------------------------------------------
	//! Appele par le `modded class SCR_AIGroup` a l'initialisation de chaque groupe.
	static void Register(SCR_AIGroup group)
	{
		if (!group)
			return;
		if (!Replication.IsServer())
			return;

		if (!s_aEntries)
			s_aEntries = {};

		s_aEntries.Insert(new FFRX_AssaultEntry(group));

		if (!s_bStarted)
		{
			s_bStarted = true;
			GetGame().GetCallqueue().CallLater(Tick, FFRX_AssaultTuning.TICK_MS, true);
			Print("[FFRX][Assaut] directeur demarre.", LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		if (!s_aEntries)
			return;

		// Un pourcentage a 0 desactive completement le systeme, sans avoir a retirer le
		// mod : le directeur continue de tourner mais ne pousse plus rien.
		float fraction = FFRX_AssaultTuning.AssaultFraction();

		World world = GetGame().GetWorld();
		if (!world)
			return;

		float now_s = world.GetWorldTime() / 1000.0;

		// Parcours a l'envers : on purge les groupes morts au passage, et supprimer en
		// remontant ne decale pas les indices restants.
		for (int i = s_aEntries.Count() - 1; i >= 0; i--)
		{
			FFRX_AssaultEntry e = s_aEntries[i];
			if (!e || !e.m_Group)
			{
				s_aEntries.Remove(i);
				continue;
			}

			if (fraction <= 0)
				continue;

			if (now_s < e.m_fNextPush_s)
				continue;

			if (FFRX_TryAssault(e.m_Group, fraction))
			{
				// Bond lance : on laisse courir, puis on rend la main pour qu'ils se battent.
				e.m_fNextPush_s = now_s + FFRX_AssaultTuning.ASSAULT_DURATION_S
					+ FFRX_AssaultTuning.ASSAULT_COOLDOWN_S;
			}
			else
			{
				// Rien a faire pour ce groupe : on re-teste au prochain tick.
				e.m_fNextPush_s = now_s;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Tente une vague d'assaut sur `group`. Retourne true si au moins un agent a ete envoye.
	protected static bool FFRX_TryAssault(notnull SCR_AIGroup group, float fraction)
	{
		// ?? GARDE DE MATURITE -- ne pas retirer.
		//
		// On enregistre les groupes des `EOnInit`, c'est-a-dire AVANT que leur composant
		// utilitaire soit pret. Or `SCR_AICombatComponent.GetCombatMode()` va lire
		// `m_eCombatModeActual` SUR CE COMPOSANT : appele trop tot, il jette une exception
		// VM "NULL pointer to instance" a chaque tick, pour chaque groupe jeune.
		//
		// (Vu en vrai : 34 exceptions en une session, avec une pile trompeuse qui pointe
		// SCR_AICombatComponent.c:855 pour TOUTES les frames de notre propre code.)
		//
		// Un groupe sans composant utilitaire ne peut de toute facon pas assaillir : on
		// sort, et on le reprendra au tick suivant une fois initialise.
		if (!group.GetGroupUtilityComponent())
			return false;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		if (agents.IsEmpty())
			return false;

		// --- Le groupe est-il ennemi ? On teste le premier agent valide : un groupe est
		// homogene en faction, inutile de les interroger tous a chaque tick.
		if (!FFRX_GroupIsEnemy(agents))
			return false;

		// --- Y a-t-il un JOUEUR accroche, et ou ?
		vector targetPos;
		float dist;
		if (!FFRX_FindPlayerContact(agents, targetPos, dist))
			return false;

		// Trop pres : l'IA vanilla gere mieux le contact que nous.
		if (dist < FFRX_AssaultTuning.ASSAULT_STOP_DIST)
			return false;

		// Sous le seuil regle par l'admin : le combat a distance reste voulu.
		if (dist < FFRX_AssaultTuning.MinDistance())
			return false;

		// Trop loin : a 1 km la "position du joueur" est deja perimee quand ils arrivent.
		if (dist > FFRX_AssaultTuning.ASSAULT_MAX_DIST)
			return false;

		// --- Qui part ? On ne retient que les agents a pied : un equipage de vehicule
		// qui debarque pour charger a pied est un contresens.
		array<AIAgent> onFoot = {};
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			if (SCR_AICompartmentHandling.IsInCompartment(a))
				continue;
			onFoot.Insert(a);
		}

		int total = onFoot.Count();
		if (total < 2)
			return false;   // un groupe d'un seul homme n'a personne pour couvrir

		int count = Math.Round(total * fraction);
		if (count < 1)
			count = 1;
		// TOUJOURS garder au moins un homme en appui : c'est ce qui distingue un assaut
		// d'une charge suicidaire, et ca garantit que le joueur reste sous le feu pendant
		// que les autres avancent.
		if (count > total - 1)
			count = total - 1;

		// Fumigene AVANT de sortir a decouvert. Avancer sans se masquer, c'est une charge,
		// pas un assaut -- et c'est precisement le reproche fait au combat a distance.
		FFRX_PopSmoke(group);

		int sent = 0;
		for (int i = 0; i < count; i++)
		{
			if (FFRX_PushAssault(onFoot[i], targetPos, i))
				sent++;
		}

		return sent > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Lecture SURE du mode de combat.
	//!
	//! SCR_AICombatComponent.GetCombatMode() (base-game, l.855) fait :
	//!     return myGroup.GetGroupUtilityComponent().GetCombatModeActual();
	//! sans verifier que GetGroupUtilityComponent() a repondu. Quand un groupe est en cours
	//! de spawn / de streaming, ou qu'il vient de perdre son dernier membre, ce composant
	//! est NULL et l'appel jette :
	//!     VM Exception -- NULL pointer to instance. Variable 'm_eCombatModeActual'
	//! Notre Tick interroge tous les groupes a chaque passage, donc on tombait dessus en
	//! boucle et ca noyait la console. On refait donc le chemin nous-memes, en verifiant
	//! chaque maillon. C'est un defaut du jeu de base : on ne peut que l'eviter.
	//!
	//! \return false si l'etat n'est pas lisible (l'appelant doit alors s'abstenir).
	static bool FFRX_SafeCombatMode(AIAgent agent, out EAIGroupCombatMode outMode)
	{
		outMode = EAIGroupCombatMode.FIRE_AT_WILL;
		if (!agent)
			return false;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group)
			return false;

		SCR_AIGroupUtilityComponent util = group.GetGroupUtilityComponent();
		if (!util)
			return false;

		outMode = util.GetCombatModeActual();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Fait poser un ecran de fumee au groupe, entre lui et l'ennemi, avant le bond.
	//!
	// ------------------------------------------------------------------------------------
	// POURQUOI L'IA NE FUMIGENE JAMAIS EN COMBAT (constat verifie dans le jeu de base)
	//
	// Le moteur SAIT poser un ecran de fumee : `SCR_AIActivitySmokeCoverFeature`. Mais il
	// n'est branche qu'a DEUX choses :
	//   - `SCR_AIHealActivity` (masquer un blesse pendant qu'on le soigne) ;
	//   - `SCR_DeploySmokeCoverWaypoint` (un waypoint pose a la main par le concepteur).
	// Il n'est PAS branche a `SCR_AIAttackClusterActivity`. Autrement dit : un groupe en
	// pleine fusillade ne fumigene jamais. Ce n'est pas un reglage trop lent, c'est une
	// absence de cablage.
	//
	// On appelle donc `Execute()` nous-memes -- la methode est publique, et c'est
	// exactement celle qu'utilise le noeud `SCR_AIDeploySmokeCover`.
	//
	// ------------------------------------------------------------------------------------
	// LES PARAMETRES, ET POURQUOI CEUX-LA
	//
	// `PROTECT_POS | PROTECT_FROM_CLUSTERS` avec la position DU GROUPE : la fumee est
	// calculee a partir des clusters de cibles connus, donc placee ENTRE le groupe et
	// l'ennemi. Sans `PROTECT_FROM_CLUSTERS` elle serait dispersee au hasard autour du
	// point ; sans `PROTECT_POS` elle tomberait pile sur le point, ce qui aveuglerait le
	// groupe au lieu de le couvrir.
	//
	// On ne borne pas le nombre de lanceurs nous-memes : `Execute` plafonne deja a la
	// MOITIE des hommes valides ("other half must cover/fight"), ce qui est la meme
	// philosophie que notre part d'assaut. Inutile de superposer deux limites.
	//
	// ?? Si les ennemis n'ont pas de fumigene en dotation, `Execute` rend false et il ne
	// se passe rien -- c'est une question de CONFIG, pas de code. D'ou le log ci-dessous.
	protected static void FFRX_PopSmoke(notnull SCR_AIGroup group)
	{
		SCR_AIGroupUtilityComponent gu = group.GetGroupUtilityComponent();
		if (!gu)
			return;

		array<AIAgent> none = {};   // parametres `notnull` : passer des tableaux vides, pas null

		SCR_AIActivitySmokeCoverFeature smoke = new SCR_AIActivitySmokeCoverFeature();
		bool thrown = smoke.Execute(
			gu,
			group.GetCenterOfMass(),
			SCR_AIActivitySmokeCoverFeatureProperties.PROTECT_POS
				| SCR_AIActivitySmokeCoverFeatureProperties.PROTECT_FROM_CLUSTERS,
			none,
			none,
			3);

		// Une seule ligne par resultat distinct : elle repond a la question "est-ce que nos
		// ennemis ont seulement un fumigene ?" sans noyer le log.
		if (thrown)
		{
			if (!s_bSmokeOkLogged)
			{
				s_bSmokeOkLogged = true;
				Print("[FFRX][Assaut] ecran de fumee pose avant le bond.", LogLevel.NORMAL);
			}
		}
		else if (!s_bSmokeKoLogged)
		{
			s_bSmokeKoLogged = true;
			Print("[FFRX][Assaut] pas de fumigene pose : aucun ennemi n'en porte, ou aucun lanceur eligible (verifier la DOTATION ennemie).", LogLevel.WARNING);
		}
	}

	protected static bool s_bSmokeOkLogged;
	protected static bool s_bSmokeKoLogged;

	//------------------------------------------------------------------------------------------------
	protected static bool FFRX_GroupIsEnemy(notnull array<AIAgent> agents)
	{
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (!combat)
				continue;

			// Un ordre de cessez-le-feu doit rester un ordre : sans ce test, une embuscade
			// en HOLD_FIRE se leverait toute seule pour charger.
			// Etat illisible (groupe en cours de spawn/despawn) -> on s'abstient plutot que
			// de risquer un assaut sur une base fausse.
			EAIGroupCombatMode mode;
			if (!FFRX_SafeCombatMode(a, mode))
				return false;

			if (mode == EAIGroupCombatMode.HOLD_FIRE)
				return false;

			return combat.FFRX_IsEnemyAI();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Cherche la cible JOUEUR la plus proche connue du groupe.
	//! On interroge les cibles connues des agents plutot que la liste des joueurs du
	//! serveur : sinon le groupe chargerait un joueur qu'il n'a jamais vu, ce qui est de
	//! la triche et se remarque tout de suite en jeu.
	protected static bool FFRX_FindPlayerContact(notnull array<AIAgent> agents, out vector outPos, out float outDist)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;

		bool found = false;
		float best = float.MAX;

		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (!combat)
				continue;

			BaseTarget t = combat.GetCurrentTarget();
			if (!t)
				t = combat.GetLastSeenEnemy();
			if (!t)
				continue;

			IEntity targetEnt = t.GetTargetEntity();
			if (!targetEnt)
				continue;

			// Un joueur, pas une IA alliee : on ne veut pas que deux camps d'IA se
			// chargent mutuellement en boucle a l'autre bout de la carte.
			if (pm.GetPlayerIdFromControlledEntity(targetEnt) <= 0)
				continue;

			float d = vector.Distance(ent.GetOrigin(), targetEnt.GetOrigin());
			if (d < best)
			{
				best = d;
				outPos = targetEnt.GetOrigin();
				found = true;
			}
		}

		outDist = best;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Envoie UN agent a l'assaut de `targetPos`.
	protected static bool FFRX_PushAssault(notnull AIAgent agent, vector targetPos, int index)
	{
		IEntity ent = agent.GetControlledEntity();
		if (!ent)
			return false;

		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(ent.FindComponent(SCR_AIUtilityComponent));
		if (!utility)
			return false;

		// Dispersion laterale : sans elle les assaillants convergent sur le meme point et
		// forment une file indienne -- une cible unique, et un rendu ridicule.
		vector dir = targetPos - ent.GetOrigin();
		dir[1] = 0;
		vector side = "0 0 0";
		if (dir.LengthSq() > 1)
		{
			dir.Normalize();
			// Perpendiculaire horizontale, alternee de part et d'autre de l'axe.
			side = Vector(-dir[2], 0, dir[0]);
			// Couloirs alternes : -1, +1, -2, +2, ...
			// Pas d'operateur '%' en Enforce ("Unknown operator") -> modulo a la main
			// par division entiere.
			int half = index / 2;
			int parity = index - (half * 2);
			float lane = (parity * 2 - 1) * (half + 1);
			side = side * (lane * FFRX_AssaultTuning.SPREAD);
		}

		vector goal = targetPos + side;

		// On annule notre bond precedent avant d'en pousser un nouveau, sinon les
		// comportements s'empilent et l'agent poursuit une position perimee.
		utility.SetStateAllActionsOfType(SCR_AIMoveAndInvestigateBehavior, EAIActionState.FAILED);

		SCR_AIMoveAndInvestigateBehavior move = new SCR_AIMoveAndInvestigateBehavior(
			utility,
			null,                                   // pas rattache a une activite de groupe
			goal,
			FFRX_AssaultTuning.ASSAULT_PRIORITY,
			SCR_AIActionBase.PRIORITY_LEVEL_NORMAL,
			FFRX_AssaultTuning.ARRIVE_RADIUS,
			true,                                   // dangereux -> reste en alerte, pas de surprise a l'arrivee
			EAIUnitType.UnitType_Infantry,
			FFRX_AssaultTuning.ASSAULT_DURATION_S);

		utility.AddAction(move);
		return true;
	}

	// ------------------------------------------------------------------------------------
	//  API pour l'anti-camping : envoyer un groupe DEJA PRESENT enqueter sur une position
	// ------------------------------------------------------------------------------------
	//
	// POURQUOI PAS UN SPAWN : faire apparaitre une escouade pour punir un campeur, c'est
	// exactement ce qui donne l'impression que le jeu triche. Le monde a deja des groupes
	// ennemis ; en detourner un est plus credible (il vient de quelque part, il met du
	// temps, il peut etre intercepte en chemin) et ne coute aucune entite supplementaire.
	//
	// Ce registre existe deja pour l'assaut : on lui ajoute juste une recherche.

	//! Groupe ennemi le plus proche de `pos`, dans `maxDist`. `needVehicle` restreint aux
	//! groupes montes (palier 3). Rend null si le monde n'a rien a envoyer -- cas normal,
	//! et bien meilleur qu'un spawn de complaisance.
	static SCR_AIGroup FFRX_FindEnemyGroupNear(vector pos, float maxDist, bool needVehicle)
	{
		if (!s_aEntries)
			return null;

		SCR_AIGroup best;
		float bestSq = maxDist * maxDist;

		foreach (FFRX_AssaultEntry e : s_aEntries)
		{
			if (!e || !e.m_Group)
				continue;
			if (!e.m_Group.GetGroupUtilityComponent())
				continue;   // meme garde de maturite que le tick (cf. FFRX_TryAssault)

			array<AIAgent> agents = {};
			e.m_Group.GetAgents(agents);
			if (agents.IsEmpty())
				continue;
			if (!FFRX_GroupIsEnemy(agents))
				continue;

			if (needVehicle && !FFRX_GroupIsMounted(agents))
				continue;

			float d = vector.DistanceSq(e.m_Group.GetCenterOfMass(), pos);
			if (d >= bestSq)
				continue;

			bestSq = d;
			best = e.m_Group;
		}

		return best;
	}

	//! Au moins un homme embarque -> le groupe dispose d'un vehicule.
	protected static bool FFRX_GroupIsMounted(notnull array<AIAgent> agents)
	{
		foreach (AIAgent a : agents)
		{
			if (a && SCR_AICompartmentHandling.IsInCompartment(a))
				return true;
		}
		return false;
	}

	//! Envoie tout le groupe enqueter sur `pos`. Rend le nombre d'hommes envoyes.
	//!
	//! On reutilise le meme comportement que l'assaut, mais SANS la part d'appui : ici il
	//! n'y a pas d'echange de tirs en cours a soutenir, le groupe se deplace pour aller
	//! voir. La priorite reste celle de l'assaut, donc les reflexes de survie priment
	//! toujours et le groupe se battra normalement s'il tombe sur quelqu'un en route.
	static int FFRX_DispatchTo(notnull SCR_AIGroup group, vector pos)
	{
		array<AIAgent> agents = {};
		group.GetAgents(agents);

		int sent = 0;
		int lane = 0;
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			if (SCR_AICompartmentHandling.IsInCompartment(a))
				continue;   // les embarques suivent leur vehicule, on ne les debarque pas

			if (FFRX_PushAssault(a, pos, lane))
			{
				sent++;
				lane++;
			}
		}

		return sent;
	}
}

//------------------------------------------------------------------------------------------------
//! Enregistrement des groupes. Pas de registre a maintenir a la main : chaque groupe
//! s'annonce a sa creation, et le directeur purge les references mortes a chaque tick.
//
// ATTRIBUT DE CLASSE REDECLARE -- NE PAS RETIRER.
// Un `modded class` qui omet l'attribut de l'original perd sa deserialisation. Le
// compilateur ne le dit PAS : il rend des dizaines de "Too many instructions per
// function" et "Incompatible parameter" sur des fichiers du JEU DE BASE et de FF
// (JWK_ConvoyAIDeployer, JWK_ShopContext...), dont aucun ne cite ce fichier, puis
// "Can't compile Game script module!". Panne du dedie le 2026-09-15.
[EntityEditorProps(category: "GameScripted/AI")]
modded class SCR_AIGroup
{
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		FFRX_AssaultDirector.Register(this);

		// Recensement des spawns. C'est ICI et pas dans FFRX_SpawnCensus.c parce que
		// Enforce n'admet qu'UN `modded class SCR_AIGroup` par addon -- en declarer un
		// second ailleurs ferait echouer la compilation.
		FFRX_SpawnCensus.NoteGroupFromEOnInit(this, owner);

		// Menace adaptative : meme raison d'etre ici. Si la zone a souffert des blindes
		// ou des helicos des joueurs, le groupe peut recevoir un servant AT ou sol-air.
		FFRX_AdaptiveThreat.Get().MaybeReinforce(this, owner);

		// Niveau de l'escouade (bleus / ordinaire / aguerrie / elite) : tire ici, une
		// fois par groupe, et lu ensuite par chacun de ses membres.
		FFRX_SquadTier.AssignTier(this);
	}
}
