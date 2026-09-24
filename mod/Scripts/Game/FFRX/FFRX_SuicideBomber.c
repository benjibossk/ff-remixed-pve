// FF - REMIXED - PVE
// Asymmetric warfare -- BRICK B: civilian suicide bombers.
//
// A "civilian" (a normal CIV character, blends into the population) walks/stands near a player and
// blows up. Forces players to be wary of civilians (D9). SYSTEM-based (no custom prefab needed):
// we spawn a plain CIV prefab and manage detonation externally -> when a resistance player gets
// within DETONATE_DIST, spawn a medium TNT explosion at the bomber and delete it. Reuses the proven
// explosion pattern (Explosion_Tnt_Medium).
//
// Server-only. Booted from FFRX_Groups.c -> OnGameModeStart. Test command: #spawnbomber.
// v1 = proximity detonation (bomber is stationary where spawned). Making it walk toward the player
// (AI move order) is a later enhancement.

class FFRX_SuicideBombers
{
	protected static ref FFRX_SuicideBombers s_Instance;

	protected static const int   FFRX_SPAWN_MS       = 150000;  // auto-spawn cadence
	protected static const float FFRX_DETONATE_DIST  = 4.0;     // boom trigger
	protected static const int   FFRX_TICK_MS        = 1000;    // proximity check
	protected static const int   FFRX_MAX_ACTIVE     = 3;       // cap simultaneous bombers
	protected static const float FFRX_SPAWN_MIN      = 120.0;   // spawn this far from the target player
	protected static const float FFRX_SPAWN_MAX      = 260.0;

	// --- Charge vers le joueur (voir FFRX_Charge pour le raisonnement) ---
	protected static const float FFRX_CHARGE_PRIORITY = 100;   // > attaque (70/90), < reflexes de survie (110+)
	protected static const float FFRX_CHARGE_ZIGZAG   = 6.0;   // amplitude laterale, en metres
	protected static const float FFRX_CHARGE_MAX_DIST = 400.0; // au-dela, il n'a plus de cible credible
	protected static const ResourceName FFRX_EXPLOSION = "{564D57EA34A75775}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Medium.et";

	// Plain CIV prefabs (Randomized = visual variety). Blend into the population.
	protected ref array<ResourceName> m_aBomberPrefabs = {
		"{22E43956740A6794}Prefabs/Characters/Factions/CIV/GenericCivilians/Character_CIV_Randomized.et",
		"{B3AB6D12D247DDB5}Prefabs/Characters/Factions/CIV/Businessman/Character_CIV_Businessman_Randomized.et",
		"{D97EAE64721478F3}Prefabs/Characters/Factions/CIV/ConstructionWorker/Character_CIV_ConstructionWorker_Randomized.et",
		"{5882AE7A4543AFB4}Prefabs/Characters/Factions/CIV/Dockworker/Character_CIV_Dockworker_Randomized.et"
	};

	protected ref array<IEntity> m_aBombers = {};

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;
		s_Instance = new FFRX_SuicideBombers();
		GetGame().GetCallqueue().CallLater(s_Instance.FFRX_Tick, FFRX_TICK_MS, true);
		GetGame().GetCallqueue().CallLater(s_Instance.FFRX_AutoSpawn, FFRX_SPAWN_MS, true);
		Print("[FFRX][Bomber] Systeme kamikazes civils demarre.", LogLevel.NORMAL);
	}

	// Spawn a bomber near a position (used by #spawnbomber). Returns the entity or null.
	static IEntity SpawnNear(vector pos)
	{
		if (!s_Instance)
			s_Instance = new FFRX_SuicideBombers();
		return s_Instance.FFRX_SpawnBomberAt(pos);
	}

	//------------------------------------------------------------------------------------------------
	// Detonation tick: any bomber with a resistance player within DETONATE_DIST blows up.
	protected void FFRX_Tick()
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		for (int i = m_aBombers.Count() - 1; i >= 0; i--)
		{
			IEntity bomber = m_aBombers[i];
			if (!bomber || bomber.IsDeleted())
			{
				m_aBombers.Remove(i);
				continue;
			}

			if (FFRX_PlayerWithin(bomber.GetOrigin(), FFRX_DETONATE_DIST))
			{
				FFRX_Detonate(bomber);
				m_aBombers.Remove(i);
				continue;
			}

			// Sinon : il VA CHERCHER le joueur au lieu d'attendre sur place.
			FFRX_Charge(bomber, i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le kamikaze marche vers le joueur le plus proche -- mais pas en ligne droite, et pas
	//! en ignorant le danger.
	//!
	//! LE CHOIX DE LA PRIORITE EST LE COEUR DU COMPORTEMENT. On sort a CHARGE_PRIORITY = 100 :
	//!   - au-dessus de l'attaque normale (70 / 90) -> il avance vraiment, il ne s'arrete pas
	//!     pour echanger des coups de feu ;
	//!   - mais EN DESSOUS de 110-125, qui sont les reflexes de survie du jeu de base (repli
	//!     sous le feu, mise a couvert, soin critique). Resultat : quand on lui tire dessus il
	//!     se met a l'abri, puis reprend sa progression au tick suivant parce qu'on repousse
	//!     l'ordre. C'est ca, le "un peu intelligent" : ce n'est pas nous qui codons la mise a
	//!     couvert, c'est nous qui laissons l'IA du jeu la faire.
	//! (Le VBIED, lui, sort a 130 : une voiture lancee ne se met pas a couvert.)
	//!
	//! On decale aussi la cible lateralement, en alternant d'un cote et de l'autre a chaque
	//! passage : une approche en ligne droite est triviale a abattre et fait tres robot.
	protected void FFRX_Charge(IEntity bomber, int index)
	{
		IEntity target = FFRX_NearestPlayerEntity(bomber.GetOrigin());
		if (!target)
			return;

		AIControlComponent ctrl = AIControlComponent.Cast(bomber.FindComponent(AIControlComponent));
		if (!ctrl)
			return;   // pas d'IA sur ce civil : il restera immobile (ancien comportement)

		AIAgent agent = ctrl.GetControlAIAgent();
		if (!agent)
			return;

		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(bomber.FindComponent(SCR_AIUtilityComponent));
		if (!utility)
			return;

		vector goal = target.GetOrigin();

		// Zigzag : perpendiculaire a l'axe d'approche, cote alterne selon le tick.
		vector dir = goal - bomber.GetOrigin();
		dir[1] = 0;
		if (dir.LengthSq() > 1)
		{
			dir.Normalize();
			vector side = Vector(-dir[2], 0, dir[0]);
			// Pas d'operateur '%' en Enforce -> parite a la main.
			int half = m_iChargeTick / 2;
			int parity = m_iChargeTick - (half * 2);
			float lane = (parity * 2 - 1) * FFRX_CHARGE_ZIGZAG;
			goal = goal + side * lane;
		}

		// On annule l'ordre precedent : sinon les comportements s'empilent et il poursuit
		// une position perimee (meme piege que dans FFRX_AIAssault et FFRX_VBIED).
		utility.SetStateAllActionsOfType(SCR_AIMoveAndInvestigateBehavior, EAIActionState.FAILED);

		SCR_AIMoveAndInvestigateBehavior move = new SCR_AIMoveAndInvestigateBehavior(
			utility,
			null,
			goal,
			FFRX_CHARGE_PRIORITY,
			SCR_AIActionBase.PRIORITY_LEVEL_NORMAL,
			FFRX_DETONATE_DIST,
			true,                       // dangereux -> reste en alerte
			EAIUnitType.UnitType_Infantry,
			FFRX_TICK_MS / 1000.0);

		utility.AddAction(move);
		m_iChargeTick++;
	}

	//------------------------------------------------------------------------------------------------
	protected int m_iChargeTick;

	//------------------------------------------------------------------------------------------------
	//! Entite du joueur resistant le plus proche, null si aucun a portee utile.
	protected IEntity FFRX_NearestPlayerEntity(vector fromPos)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return null;

		array<int> ids = {};
		pm.GetPlayers(ids);

		IEntity best = null;
		float bestSq = FFRX_CHARGE_MAX_DIST * FFRX_CHARGE_MAX_DIST;
		foreach (int pid : ids)
		{
			IEntity e = pm.GetPlayerControlledEntity(pid);
			if (!e)
				continue;
			float dSq = vector.DistanceSq(e.GetOrigin(), fromPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = e;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_Detonate(IEntity bomber)
	{
		Resource res = Resource.Load(FFRX_EXPLOSION);
		if (res && res.IsValid())
		{
			EntitySpawnParams sp = new EntitySpawnParams();
			sp.TransformMode = ETransformMode.WORLD;
			vector mat[4];
			bomber.GetTransform(mat);
			sp.Transform = mat;
			GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		}
		Print("[FFRX][Bomber] Kamikaze detone.", LogLevel.NORMAL);
		SCR_EntityHelper.DeleteEntityAndChildren(bomber);
	}

	//------------------------------------------------------------------------------------------------
	// Auto-spawn a bomber near a random player in enemy/contested territory.
	protected void FFRX_AutoSpawn()
	{
		if (m_aBombers.Count() >= FFRX_MAX_ACTIVE)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);
		if (ids.IsEmpty())
			return;

		int pid = ids.GetRandomElement();
		IEntity pe = pm.GetPlayerControlledEntity(pid);
		if (!pe)
			return;

		// A point 120-260 m from the player, on the ground.
		float ang  = Math.RandomFloat(0, Math.PI2);
		float dist = Math.RandomFloat(FFRX_SPAWN_MIN, FFRX_SPAWN_MAX);
		vector pos = pe.GetOrigin() + Vector(Math.Cos(ang) * dist, 0, Math.Sin(ang) * dist);
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);

		// Only in enemy / frontline territory (asymmetric threat in contested areas).
		if (!FFRX_InEnemyTerritory(pos))
			return;

		vector checkedPos = pos;
		if (!SCR_WorldTools.FindEmptyTerrainPosition(checkedPos, checkedPos, 3, 1.5, 2, TraceFlags.ENTS | TraceFlags.WORLD, GetGame().GetWorld()))
			return;

		FFRX_SpawnBomberAt(checkedPos);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FFRX_SpawnBomberAt(vector pos)
	{
		ResourceName prefab = m_aBomberPrefabs.GetRandomElement();
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return null;

		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(sp.Transform);
		sp.Transform[3] = pos;

		IEntity bomber = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		if (!bomber)
			return null;

		m_aBombers.Insert(bomber);
		Print(string.Format("[FFRX][Bomber] Kamikaze civil spawn a %1.", pos.ToString()), LogLevel.NORMAL);
		return bomber;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FFRX_PlayerWithin(vector pos, float radius)
	{
		float rSq = radius * radius;
		PlayerManager pm = GetGame().GetPlayerManager();
		array<int> ids = {};
		pm.GetPlayers(ids);
		foreach (int pid : ids)
		{
			IEntity pe = pm.GetPlayerControlledEntity(pid);
			if (pe && vector.DistanceSqXZ(pe.GetOrigin(), pos) <= rSq)
				return true;
		}
		return false;
	}

	protected bool FFRX_InEnemyTerritory(vector pos)
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

//----------------------------------------------------------------------------------------------------
// Dev command "#spawnbomber": spawn a civilian kamikaze ~8 m in front of the caller (approach to test).
[BaseContainerProps()]
class FFRX_SpawnBomberCommand : ScrServerCommand
{
	override string GetKeyword() { return "spawnbomber"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		vector mat[4];
		ent.GetTransform(mat);
		vector pos = ent.GetOrigin() + mat[2] * 8.0;
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);

		if (FFRX_SuicideBombers.SpawnNear(pos))
			return ScrServerCmdResult("Kamikaze civil spawn devant toi (approche-toi a <4m).", EServerCmdResultType.OK);
		return ScrServerCmdResult("Echec spawn kamikaze.", EServerCmdResultType.ERR);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
