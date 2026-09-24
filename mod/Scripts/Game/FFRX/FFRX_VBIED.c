// FF - REMIXED - PVE
// Guerre asymetrique -- VBIED : voiture piegee ROULANTE qui fonce sur un joueur.
//
// A ne pas confondre avec FFRX_BoobyTrapCars : celui-la pose une mine sous une epave
// abandonnee, le joueur SUBIT sans rien voir venir. Ici la menace est MOBILE et donc
// LISIBLE : on voit une voiture arriver, on peut l'identifier, tirer dessus, se mettre
// a couvert. C'est ce qui rend la mecanique jouable plutot que punitive.
//
// COMMENT. On spawne un vehicule CIVIL (il se fond dans le trafic) avec un conducteur IA
// ennemi dedans, puis on lui repousse un ordre de deplacement vers le joueur a chaque
// tick -- la cible bouge, un ordre unique serait perime en quelques secondes. A
// DETONATE_DIST, explosion et nettoyage.
//
// POURQUOI REPOUSSER L'ORDRE : SCR_AIMoveAndInvestigateBehavior vise une POSITION figee.
// Meme mecanique que FFRX_AIAssault, avec ici une priorite plus haute : le conducteur ne
// doit pas s'arreter pour se battre, il n'a qu'une seule chose a faire.
//
// Le vehicule reste destructible : lui tirer dessus tue le conducteur et stoppe l'attaque.
// C'est voulu -- une menace qu'on ne peut pas neutraliser n'est pas un defi, c'est une taxe.
//
// Serveur uniquement. Chaines ASCII. Commande de test : #vbied.

class FFRX_VBIEDTuning
{
	//! Priorite du deplacement. Au-dessus de l'assaut (95) : un kamikaze ne se met pas
	//! a couvert pour echanger des coups de feu, il fonce.
	static const float DRIVE_PRIORITY = 130;

	static const int   TICK_MS         = 1000;   // re-visee de la cible
	static const float DETONATE_DIST   = 8.0;    // rayon de declenchement
	static const float ARRIVE_RADIUS   = 5.0;
	static const float SPAWN_MIN       = 150.0;  // distance de depart
	static const float SPAWN_MAX       = 320.0;
	static const float GIVEUP_DIST     = 900.0;  // trop loin -> on abandonne et on nettoie
	static const int   LIFETIME_MS     = 180000; // securite : jamais de VBIED eternel

	//! Grosse charge : une voiture piegee doit faire bien plus qu'une veste.
	static const ResourceName EXPLOSION = "{564D57EA34A75775}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Medium.et";
}

// ---------------------------------------------------------------------------
class FFRX_VBIED
{
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<ref FFRX_VBIED> s_aActive;

	protected static array<ref FFRX_VBIED> Active()
	{
		if (!s_aActive)
			s_aActive = new array<ref FFRX_VBIED>();

		return s_aActive;
	}

	protected IEntity m_Vehicle;
	protected IEntity m_Driver;
	protected int     m_iTargetPlayerId;
	protected bool    m_bDone;

	//------------------------------------------------------------------------------------------------
	//! Nombre de VBIED en cours. Sert de plafond au declenchement automatique.
	//! Fiable : Cleanup() se retire de Active(), y compris sur Abort et sur explosion.
	static int ActiveCount()
	{
		return Active().Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Lance un VBIED sur un joueur. \return false si rien n'a pu etre spawne.
	static bool Launch(int targetPlayerId)
	{
		if (!Replication.IsServer())
			return false;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;

		IEntity target = pm.GetPlayerControlledEntity(targetPlayerId);
		if (!target)
			return false;

		vector spawnPos;
		if (!FFRX_FindSpawnPos(target.GetOrigin(), spawnPos))
		{
			Print("[FFRX][VBIED] Aucune position de depart utilisable (route/terrain).", LogLevel.WARNING);
			return false;
		}

		FFRX_VBIED v = new FFRX_VBIED();
		if (!v.Spawn(spawnPos, targetPlayerId))
			return false;

		Active().Insert(v);
		GetGame().GetCallqueue().CallLater(v.Tick, FFRX_VBIEDTuning.TICK_MS, true);
		GetGame().GetCallqueue().CallLater(v.Abort, FFRX_VBIEDTuning.LIFETIME_MS, false);
		Print(string.Format("[FFRX][VBIED] Lance sur le joueur %1 depuis %2.", targetPlayerId, spawnPos), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Position de depart : sur le sol, a bonne distance, dans une direction aleatoire.
	protected static bool FFRX_FindSpawnPos(vector around, out vector outPos)
	{
		float ang = Math.RandomFloat(0, Math.PI2);
		float dist = Math.RandomFloat(FFRX_VBIEDTuning.SPAWN_MIN, FFRX_VBIEDTuning.SPAWN_MAX);

		vector p = around;
		p[0] = p[0] + Math.Cos(ang) * dist;
		p[2] = p[2] + Math.Sin(ang) * dist;

		// Terrain praticable : sans ca le vehicule peut apparaitre dans un rocher ou en mer.
		return SCR_WorldTools.FindEmptyTerrainPosition(outPos, p, 60);
	}

	//------------------------------------------------------------------------------------------------
	protected bool Spawn(vector pos, int targetPlayerId)
	{
		m_iTargetPlayerId = targetPlayerId;

		ResourceName vehPrefab = FFRX_PickCivVehicle();
		Resource res = Resource.Load(vehPrefab);
		if (!res || !res.IsValid())
			return false;

		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.WORLD;
		sp.Transform[3] = pos;

		m_Vehicle = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		if (!m_Vehicle)
			return false;

		// Conducteur ENNEMI : c'est lui qui porte l'intention. On reutilise le spawn FF,
		// qui cree aussi le groupe IA -- sans groupe, aucun comportement ne tourne.
		m_Driver = JWK_SpawnUtils.SpawnCharacter(pos, FFRX_DriverPrefab(), ResourceName.Empty, JWK_EFactionRole.ENEMY);
		if (!m_Driver)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
			m_Vehicle = null;
			return false;
		}

		// Au volant.
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(m_Driver.FindComponent(SCR_CompartmentAccessComponent));
		if (!access || !access.MoveInVehicle(m_Vehicle, ECompartmentType.PILOT))
		{
			Print("[FFRX][VBIED] Le conducteur n'a pas pu monter au volant.", LogLevel.WARNING);
			Cleanup();
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (m_bDone)
			return;

		// Vehicule ou conducteur detruit -> l'attaque est neutralisee, on nettoie.
		if (!m_Vehicle || !m_Driver)
		{
			Abort();
			return;
		}

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		IEntity target = pm.GetPlayerControlledEntity(m_iTargetPlayerId);
		if (!target)
		{
			Abort();   // cible deconnectee ou morte
			return;
		}

		float dist = vector.Distance(m_Vehicle.GetOrigin(), target.GetOrigin());

		if (dist <= FFRX_VBIEDTuning.DETONATE_DIST)
		{
			Detonate();
			return;
		}

		if (dist > FFRX_VBIEDTuning.GIVEUP_DIST)
		{
			Abort();
			return;
		}

		FFRX_DriveTo(target.GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! Repousse l'ordre de deplacement vers la position COURANTE de la cible.
	protected void FFRX_DriveTo(vector goal)
	{
		AIControlComponent ctrl = AIControlComponent.Cast(m_Driver.FindComponent(AIControlComponent));
		if (!ctrl)
			return;

		AIAgent agent = ctrl.GetControlAIAgent();
		if (!agent)
			return;

		IEntity ent = agent.GetControlledEntity();
		if (!ent)
			return;

		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(ent.FindComponent(SCR_AIUtilityComponent));
		if (!utility)
			return;

		// On annule l'ordre precedent, sinon les comportements s'empilent et le conducteur
		// poursuit une position perimee (meme piege que dans FFRX_AIAssault).
		utility.SetStateAllActionsOfType(SCR_AIMoveAndInvestigateBehavior, EAIActionState.FAILED);

		SCR_AIMoveAndInvestigateBehavior move = new SCR_AIMoveAndInvestigateBehavior(
			utility,
			null,
			goal,
			FFRX_VBIEDTuning.DRIVE_PRIORITY,
			SCR_AIActionBase.PRIORITY_LEVEL_NORMAL,
			FFRX_VBIEDTuning.ARRIVE_RADIUS,
			true,
			// Voiture civile = vehicule NON BLINDE. L'enum n'a pas de "UnitType_Vehicle"
			// generique : Unarmored / Medium / Heavy / Aircraft.
			EAIUnitType.UnitType_VehicleUnarmored,
			FFRX_VBIEDTuning.TICK_MS / 1000.0);

		utility.AddAction(move);
	}

	//------------------------------------------------------------------------------------------------
	protected void Detonate()
	{
		if (m_bDone)
			return;
		m_bDone = true;

		vector pos = m_Vehicle.GetOrigin();

		Resource res = Resource.Load(FFRX_VBIEDTuning.EXPLOSION);
		if (res && res.IsValid())
		{
			EntitySpawnParams sp = new EntitySpawnParams();
			sp.TransformMode = ETransformMode.WORLD;
			sp.Transform[3] = pos;
			GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		}

		Print(string.Format("[FFRX][VBIED] Detonation a %1.", pos), LogLevel.NORMAL);
		Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	//! Fin sans explosion : cible perdue, trop loin, ou delai de securite atteint.
	protected void Abort()
	{
		if (m_bDone)
			return;
		m_bDone = true;
		Cleanup();
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup()
	{
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(Abort);

		if (m_Driver)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Driver);
			m_Driver = null;
		}
		if (m_Vehicle)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(m_Vehicle);
			m_Vehicle = null;
		}

		int idx = Active().Find(this);
		if (idx >= 0)
			Active().Remove(idx);
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicule civil : la voiture doit passer pour du trafic ordinaire jusqu'au dernier moment.
	protected static ResourceName FFRX_PickCivVehicle()
	{
		array<ResourceName> pool = {
			"{1F0AC6BC0B7A63E6}Prefabs/Vehicles/Wheeled/S105/S105_Civilian.et",
			"{7B2C9E0E35DB1D3E}Prefabs/Vehicles/Wheeled/S1203/S1203_Civilian.et"
		};
		return pool[Math.RandomInt(0, pool.Count())];
	}

	//------------------------------------------------------------------------------------------------
	//! Conducteur : un civil suffit -- il ne doit pas tirer, juste conduire. Le role ENEMY
	//! est impose au spawn, c'est lui qui rend l'unite hostile.
	protected static ResourceName FFRX_DriverPrefab()
	{
		return "{22E43956740A6794}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Randomized.et";
	}
}

// ---------------------------------------------------------------------------
//  DECLENCHEMENT AUTOMATIQUE
// ---------------------------------------------------------------------------
// Jusqu'au 2026-09-20, tout ce qui precede n'etait appele QUE par la commande admin
// #vbied : la mecanique etait entierement ecrite, et ne se produisait jamais en jeu.
// C'est ce gestionnaire qui en fait une vraie menace.
//
// LA REGLE : toutes les 4 minutes, chaque joueur tire sa chance. S'il est en territoire
// ennemi ou sur la ligne de front, une voiture peut partir sur lui.
//
// POURQUOI SEULEMENT EN TERRITOIRE ENNEMI (meme garde-fou que les kamikazes et les
// voitures piegees) : une VBIED qui surgit dans une zone tenue par la resistance
// n'aurait aucun sens -- ni logistique, ni narratif. La menace doit recompenser la
// prudence quand on s'avance, pas punir le fait d'exister.
//
// POURQUOI UN PLAFOND A 1 PAR DEFAUT : deux voitures qui convergent sur le meme joueur
// ne sont plus une menace lisible mais une embuscade impossible a lire -- or c'est
// precisement la LISIBILITE qui distingue cette mecanique de la voiture piegee posee.
//
// Les deux reglages sont dans le menu admin FF ("Voiture beliere (VBIED) %" et
// "Voiture beliere : max"), donc ajustables sans recompiler.
//
// Serveur uniquement. Amorce depuis FFRX_Boot.c.
// ---------------------------------------------------------------------------

class FFRX_VBIEDSpawner
{
	protected static ref FFRX_VBIEDSpawner s_Instance;

	//! Cadence du tirage. Volontairement lente : c'est un evenement, pas un bruit de fond.
	protected static const int AUTO_TICK_MS = 240000;   // 4 min

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;

		s_Instance = new FFRX_VBIEDSpawner();
		GetGame().GetCallqueue().CallLater(s_Instance.AutoTick, AUTO_TICK_MS, true);
		Print("[FFRX][VBIED] Declenchement automatique demarre.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Chance par joueur et par tick, en 0..1. Reglage FF "Voiture beliere (VBIED) %".
	protected static float Chance()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return 0;
		return c.m_fFFRX_VbiedPct / 100;
	}

	//------------------------------------------------------------------------------------------------
	protected static int MaxActive()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return 0;
		return Math.Round(c.m_fFFRX_VbiedMax);
	}

	//------------------------------------------------------------------------------------------------
	protected void AutoTick()
	{
		int cap = MaxActive();
		if (cap <= 0)
			return; // reglage a 0 = mecanique desactivee

		float chance = Chance();
		if (chance <= 0)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);
		if (ids.IsEmpty())
			return;

		// Ordre aleatoire : sans ca, avec un plafond a 1, ce serait toujours le meme
		// joueur (le premier de la liste) qui se ferait viser.
		for (int i = ids.Count() - 1; i > 0; i--)
		{
			int j = Math.RandomInt(0, i + 1);
			int tmp = ids[i];
			ids[i] = ids[j];
			ids[j] = tmp;
		}

		foreach (int pid : ids)
		{
			if (FFRX_VBIED.ActiveCount() >= cap)
				return;

			if (Math.RandomFloat01() >= chance)
				continue;

			IEntity pe = pm.GetPlayerControlledEntity(pid);
			if (!pe)
				continue;

			if (!InEnemyTerritory(pe.GetOrigin()))
				continue;

			FFRX_VBIED.Launch(pid);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Meme definition que FFRX_SuicideBomber / FFRX_BoobyTrapCars : territoire ennemi ou
	//! noeud frontiere. Duplique plutot que partage, comme les trois autres briques -- les
	//! factoriser est un chantier a part (cf. LOTS_EN_COURS).
	protected bool InEnemyTerritory(vector pos)
	{
		JWK_TerritoryControlSystem tc = JWK.GetTerritoryControl();
		if (!tc)
			return false;

		JWK_TerritoryControlNodeComponent node = tc.GetNodeAt(pos);
		if (!node)
			return false;

		return node.GetFactionRole() == JWK_EFactionRole.ENEMY || node.IsBorder();
	}
}

// ---------------------------------------------------------------------------
//  #vbied -- lance une voiture piegee roulante sur celui qui tape la commande.
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_VBIEDCommand : ScrServerCommand
{
	override string GetKeyword() { return "vbied"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		if (FFRX_VBIED.Launch(playerId))
			return ScrServerCmdResult("Voiture piegee lancee sur toi. Elle arrive.", EServerCmdResultType.OK);
		return ScrServerCmdResult("Echec du lancement (pas de position de depart, ou conducteur impossible a asseoir).", EServerCmdResultType.ERR);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return ScrServerCmdResult("Commande joueur uniquement (#vbied en jeu).", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
