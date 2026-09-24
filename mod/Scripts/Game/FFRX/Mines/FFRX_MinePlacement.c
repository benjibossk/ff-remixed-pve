// FF - REMIXED - PVE
// Auto-placed enemy minefields (ported from FFMI_PlaceMines; REMIXED now owns this instead of
// depending on FFMI). Driven by the FF territory system:
//   - ANTI-TANK mines on roads in ENEMY / border territory.
//   - ANTI-PERSONNEL mines around enemy military points (garrisons, checkpoints) -> minefield zones
//     (registered in FFRX_MineRegistry so the intel system can reveal them).
// Mines are the base-game prefabs (ACE Explosives overrides them -> bury/detector/disarm actions).
// Spawned inert, ARMED in place (FFRX_Arm) when a player approaches; faction immunity
// (FFRX_MineTrigger) means only the resistance side triggers them. Server only, fresh game only.
//
// Standalone class (REMIXED already mods SCR_BaseGameMode elsewhere) -> Boot() from FFRX_Groups.c.
// ASCII strings. Dev command: #regeneratemines.

class FFRX_MinePlacement
{
	protected static ref FFRX_MinePlacement s_Instance;

	// ---- Config (defaults; can be moved to FF settings later) ----
	protected static const int   FFRX_AT_GROUPS             = 40;    // groups of 3 AT mines (territory roads)
	protected static const int   FFRX_AP_PER_BASE_MIN       = 2;
	protected static const int   FFRX_AP_PER_BASE_MAX       = 6;
	protected static const float FFRX_ARM_RADIUS            = 80.0;  // arm when a player is within this
	protected static const int   FFRX_ARM_TICK_MS           = 2000;
	protected static const float FFRX_MIN_ROAD_WIDTH        = 3.0;
	protected static const float FFRX_TERRITORY_ROAD_RADIUS = 150.0;
	protected static const float FFRX_AT_SPACING            = 3.0;
	protected static const float FFRX_AP_SPACING            = 2.0;
	protected static const float FFRX_CP_RADIUS             = 30.0;
	protected static const float FFRX_GARRISON_RADIUS       = 100.0;
	protected static const bool  FFRX_ALLOW_BURYING         = true;
	// Placement chances (hardcoded; FFMI exposed these as settings).
	protected static const float FFRX_AP_GARRISON_CHANCE    = 0.6;
	protected static const float FFRX_AP_CHECKPOINT_CHANCE  = 0.4;
	protected static const float FFRX_AT_ROAD_CHANCE        = 0.3;

	// Mine prefabs = ACE Explosives (bury/detector/disarm). AT: TM62M/M15AT ; AP: PMN4/M14.
	protected static const ResourceName FFRX_MINE_TM62M = "{CCC00D009D4949B0}Prefabs/Weapons/Explosives/Mine_TM62M/Mine_TM62M_base.et"; // AT USSR
	protected static const ResourceName FFRX_MINE_M15AT = "{3BF82FD68BBC845C}Prefabs/Weapons/Explosives/Mine_M15AT/Mine_M15AT_base.et"; // AT US
	protected static const ResourceName FFRX_MINE_PMN4  = "{20EFA9ACB024108F}Prefabs/Weapons/Explosives/Mine_PMN4/Mine_PMN4_base.et";   // AP USSR
	protected static const ResourceName FFRX_MINE_M14   = "{91CE54ECA33BCB05}Prefabs/Weapons/Explosives/Mine_M14/Mine_M14_base.et";     // AP US

	// ---- Runtime state ----
	protected bool m_bPlaced      = false;
	protected bool m_bArmTickOn   = false;
	protected int  m_iATRetries   = 0;
	protected ref array<IEntity> m_aUnarmed   = {};
	protected ref array<IEntity> m_aArmed     = {};
	protected ref array<vector>  m_aPositions = {};
	protected ref array<ref Shape> m_aDebug   = {}; // WORKBENCH debug shapes (kept so they persist)

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;
		s_Instance = new FFRX_MinePlacement();
		GetGame().GetCallqueue().CallLater(s_Instance.FFRX_Start, 10000, false);
	}

	static void Regenerate()
	{
		if (s_Instance)
			s_Instance.FFRX_RegenerateMines();
	}

	//------------------------------------------------------------------------------------------------
	void FFRX_Start()
	{
		if (!Replication.IsServer() || m_bPlaced)
			return;

		JWK_PersistenceManagerComponent persistence = JWK_PersistenceManagerComponent.GetInstance();
		if (persistence && persistence.HasSaveGame())
		{
			Print("[FFRX][Mines] Sauvegarde existante -> pas de pose initiale (#regeneratemines pour forcer).", LogLevel.NORMAL);
			FFRX_StartArmingTick();
			return;
		}

		m_bPlaced = true;
		Print("[FFRX][Mines] Nouvelle partie -> pose des mines dans 2s...", LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(FFRX_PlaceAll, 2000, false);
	}

	void FFRX_RegenerateMines()
	{
		if (!Replication.IsServer())
			return;
		FFRX_CleanupAll();
		m_bPlaced = true;
		GetGame().GetCallqueue().CallLater(FFRX_PlaceAll, 2000, false);
	}

	protected void FFRX_PlaceAll()
	{
		Print("[FFRX][Mines] ===== POSE (AT territoire + AP bases/checkpoints) =====", LogLevel.NORMAL);
		FFRX_PlaceAT();
		FFRX_PlaceAP();
		FFRX_StartArmingTick();
		Print(string.Format("[FFRX][Mines] ===== Pose OK : %1 mines inertes en attente d'armement. =====", m_aUnarmed.Count()), LogLevel.NORMAL);
	}

	// ================================================================== AT (territory roads)
	protected void FFRX_PlaceAT()
	{
		ResourceName atPrefab = FFRX_GetEnemyATPrefab();

		array<JWK_TerritoryControlNodeComponent> nodes = FFRX_GetQualifiedNodes();
		if (nodes.IsEmpty())
			return;

		JWK_RoadNetworkManagerComponent roadMgr = JWK_RoadNetworkManagerComponent.GetInstance();
		if (!roadMgr || !roadMgr.m_Dataset)
		{
			if (m_iATRetries < 12)
			{
				m_iATRetries++;
				GetGame().GetCallqueue().CallLater(FFRX_PlaceAT, 5000, false);
			}
			return;
		}

		int placed = 0;
		int budget = FFRX_AT_GROUPS * 6;

		while (placed < FFRX_AT_GROUPS && budget > 0)
		{
			budget--;
			JWK_TerritoryControlNodeComponent node = nodes.GetRandomElement();

			JWK_Road road;
			int roadPt;
			if (!roadMgr.FindNearestRoadWidthPoint(node.GetOwner().GetOrigin(), FFRX_MIN_ROAD_WIDTH, FFRX_TERRITORY_ROAD_RADIUS, road, roadPt))
				continue;

			vector basePos = road.points[roadPt];
			basePos[1] = GetGame().GetWorld().GetSurfaceY(basePos[0], basePos[2]);

			if (!FFRX_IsInQualifiedTerritory(basePos, nodes)) continue;
			if (FFRX_IsTooClose(basePos, FFRX_AT_SPACING)) continue;
			if (Math.RandomFloat01() >= FFRX_AT_ROAD_CHANCE) continue;

			vector checkedPos = basePos;
			if (!SCR_WorldTools.FindEmptyTerrainPosition(checkedPos, checkedPos, 3, 1.5, 2, TraceFlags.ENTS | TraceFlags.WORLD, GetGame().GetWorld()))
				continue;

			float spread = Math.Clamp(road.width * 0.5, 0.5, 3.0);
			FFRX_SpawnMine(atPrefab, basePos, true, FFRX_MineSurfaceUtils.BURY_DEPTH);
			FFRX_SpawnMine(atPrefab, basePos + Vector(spread, 0, spread),  true, FFRX_MineSurfaceUtils.BURY_DEPTH);
			FFRX_SpawnMine(atPrefab, basePos + Vector(-spread, 0, spread), true, FFRX_MineSurfaceUtils.BURY_DEPTH);
			placed++;
		}

		Print(string.Format("[FFRX][Mines] %1 groupes AT poses (%2 mines).", placed, placed * 3), LogLevel.NORMAL);
	}

	// ================================================================== AP (garrisons + checkpoints)
	protected void FFRX_PlaceAP()
	{
		ResourceName apPrefab = FFRX_GetEnemyAPPrefab();
		array<JWK_TerritoryControlNodeComponent> enemyNodes = FFRX_GetQualifiedNodes();

		// Enemy garrisons.
		array<EntityID> garrisons = JWK_IndexSystem.Get().GetAll(JWK_AIGarrisonComponent);
		foreach (EntityID id : garrisons)
		{
			JWK_AIGarrisonComponent g = JWK_CompTU<JWK_AIGarrisonComponent>.FindIn(id);
			if (!g) continue;
			IEntity owner = g.GetOwner();
			if (!owner) continue;

			vector center = owner.GetOrigin();
			JWK_FactionControlComponent fc = JWK_CompTU<JWK_FactionControlComponent>.FindIn(owner);
			bool isEnemy = (fc && fc.IsEnemyFaction());
			if (!isEnemy)
				isEnemy = FFRX_IsInQualifiedTerritory(center, enemyNodes);
			if (!isEnemy) continue;

			if (Math.RandomFloat01() >= FFRX_AP_GARRISON_CHANCE) continue;
			int nGarr = FFRX_ScatterAP(apPrefab, center, FFRX_GARRISON_RADIUS, Math.RandomIntInclusive(FFRX_AP_PER_BASE_MIN, FFRX_AP_PER_BASE_MAX));
			if (nGarr > 0) FFRX_MineRegistry.RegisterMinefield(center);
		}

		// Enemy checkpoints.
		array<EntityID> checkpoints = JWK_IndexSystem.Get().GetAll(JWK_CheckpointEntity);
		foreach (EntityID id : checkpoints)
		{
			JWK_CheckpointEntity cp = JWK_CheckpointEntity.Cast(GetGame().GetWorld().FindEntityByID(id));
			if (!cp || !cp.IsSpawnedIn()) continue;

			vector center = cp.GetOrigin();
			if (!FFRX_IsInQualifiedTerritory(center, enemyNodes)) continue;

			if (Math.RandomFloat01() >= FFRX_AP_CHECKPOINT_CHANCE) continue;
			int nCp = FFRX_ScatterAP(apPrefab, center, FFRX_CP_RADIUS, Math.RandomIntInclusive(FFRX_AP_PER_BASE_MIN, FFRX_AP_PER_BASE_MAX));
			if (nCp > 0) FFRX_MineRegistry.RegisterMinefield(center);
		}
	}

	protected int FFRX_ScatterAP(ResourceName prefab, vector center, float radius, int count)
	{
		int placed = 0;
		int budget = count * 8;

		while (placed < count && budget > 0)
		{
			budget--;
			float ang  = Math.RandomFloat(0, Math.PI2);
			float dist = Math.RandomFloat(radius * 0.2, radius);
			vector pos = center + Vector(Math.Cos(ang) * dist, 0, Math.Sin(ang) * dist);
			pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);

			if (FFRX_IsTooClose(pos, FFRX_AP_SPACING)) continue;

			vector checkedPos = pos;
			if (!SCR_WorldTools.FindEmptyTerrainPosition(checkedPos, checkedPos, 2, 1.0, 2, TraceFlags.ENTS | TraceFlags.WORLD, GetGame().GetWorld()))
				continue;

			FFRX_SpawnMine(prefab, pos, true, FFRX_MineSurfaceUtils.BURY_DEPTH_AP);
			placed++;
		}
		return placed;
	}

	// ================================================================== Spawn + register
	protected void FFRX_SpawnMine(ResourceName prefab, vector origin, bool allowBury, float buryDepth)
	{
		vector pos = origin;
		pos[1] = FFRX_MineSurfaceUtils.ComputePlacementY(pos, allowBury && FFRX_ALLOW_BURYING, buryDepth);

		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return;

		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(sp.Transform);
		sp.Transform[3] = pos;

		IEntity mine = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		if (!mine)
			return;

		FFRX_MineRegistry.Register(mine);

		// Force INERT on spawn (dormant until a player is within FFRX_ARM_RADIUS) -> enemy AI /
		// civilians can't set it off before a player is near, on top of the faction immunity.
		// Handles ACE mines that might spawn armed by default.
		SCR_PressureTriggerComponent trig = SCR_PressureTriggerComponent.Cast(mine.FindComponent(SCR_PressureTriggerComponent));
		if (trig && trig.IsActivated())
			trig.DisarmTrigger();

		m_aUnarmed.Insert(mine);
		m_aPositions.Insert(pos);
		FFRX_DrawDebug(pos);
	}

	protected void FFRX_DrawDebug(vector pos)
	{
	#ifdef WORKBENCH
		int color = ARGB(220, 255, 40, 40);
		Shape sphere = Shape.CreateSphere(color, ShapeFlags.NOZBUFFER | ShapeFlags.WIREFRAME, pos, 1.0);
		if (sphere)
			m_aDebug.Insert(sphere);
		vector top = pos;
		top[1] = top[1] + 12.0;
		Shape line = Shape.CreateArrow(pos, top, 0.4, color, ShapeFlags.NOZBUFFER);
		if (line)
			m_aDebug.Insert(line);
	#endif
	}

	// ================================================================== Arming (single global tick)
	protected void FFRX_StartArmingTick()
	{
		if (m_bArmTickOn)
			return;
		GetGame().GetCallqueue().CallLater(FFRX_GlobalArmingTick, FFRX_ARM_TICK_MS, true);
		m_bArmTickOn = true;
	}

	protected void FFRX_GlobalArmingTick()
	{
		int armedThisTick = 0;
		for (int i = m_aUnarmed.Count() - 1; i >= 0; i--)
		{
			IEntity mine = m_aUnarmed[i];
			if (!mine || mine.IsDeleted())
			{
				m_aUnarmed.Remove(i);
				continue;
			}

			if (!FFRX_IsPlayerNearby(mine.GetOrigin(), FFRX_ARM_RADIUS))
				continue;

			SCR_PressureTriggerComponent trig = SCR_PressureTriggerComponent.Cast(mine.FindComponent(SCR_PressureTriggerComponent));
			if (trig)
				trig.FFRX_Arm();

			m_aUnarmed.Remove(i);
			m_aArmed.Insert(mine);
			armedThisTick++;
		}

		if (armedThisTick > 0)
			Print(string.Format("[FFRX][Mines] %1 mine(s) armee(s) (joueur a proximite) -> %2 armees / %3 encore inertes.",
				armedThisTick, m_aArmed.Count(), m_aUnarmed.Count()), LogLevel.NORMAL);
	}

	// ================================================================== Cleanup
	protected void FFRX_CleanupAll()
	{
		foreach (IEntity mine : m_aUnarmed)
			if (mine && !mine.IsDeleted()) { FFRX_MineRegistry.Unregister(mine); SCR_EntityHelper.DeleteEntityAndChildren(mine); }
		foreach (IEntity mine : m_aArmed)
			if (mine && !mine.IsDeleted()) { FFRX_MineRegistry.Unregister(mine); SCR_EntityHelper.DeleteEntityAndChildren(mine); }
		m_aUnarmed.Clear();
		m_aArmed.Clear();
		m_aPositions.Clear();
		FFRX_MineRegistry.ClearMinefields();
	}

	// ================================================================== Helpers
	protected array<JWK_TerritoryControlNodeComponent> FFRX_GetQualifiedNodes()
	{
		array<JWK_TerritoryControlNodeComponent> result = {};
		array<EntityID> ids = JWK_IndexSystem.Get().GetAll(JWK_TerritoryControlNodeComponent);
		foreach (EntityID id : ids)
		{
			JWK_TerritoryControlNodeComponent node = JWK_CompTU<JWK_TerritoryControlNodeComponent>.FindIn(id);
			if (!node || !node.IsEnabled() || !node.HasTerritory()) continue;
			if (node.GetFactionRole() == JWK_EFactionRole.ENEMY || node.IsBorder())
				result.Insert(node);
		}
		return result;
	}

	protected bool FFRX_IsInQualifiedTerritory(vector pos, array<JWK_TerritoryControlNodeComponent> nodes)
	{
		foreach (JWK_TerritoryControlNodeComponent node : nodes)
			if (node.IsPointIncluded(pos)) return true;
		return false;
	}

	protected bool FFRX_IsTooClose(vector pos, float minDist)
	{
		float mSq = minDist * minDist;
		foreach (vector mp : m_aPositions)
			if (vector.DistanceSqXZ(pos, mp) < mSq) return true;
		return false;
	}

	protected bool FFRX_IsPlayerNearby(vector pos, float radius)
	{
		float rSq = radius * radius;
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int pid : playerIds)
		{
			IEntity pe = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (pe && vector.DistanceSqXZ(pe.GetOrigin(), pos) <= rSq)
				return true;
		}
		return false;
	}

	protected ResourceName FFRX_GetEnemyATPrefab()
	{
		JWK_Faction enemy = JWK.GetFactions().GetJWKFactionByRole(JWK_EFactionRole.ENEMY);
		if (enemy && enemy.GetKey() == "US")
			return FFRX_MINE_M15AT;
		return FFRX_MINE_TM62M;
	}

	protected ResourceName FFRX_GetEnemyAPPrefab()
	{
		JWK_Faction enemy = JWK.GetFactions().GetJWKFactionByRole(JWK_EFactionRole.ENEMY);
		if (enemy && enemy.GetKey() == "US")
			return FFRX_MINE_M14;
		return FFRX_MINE_PMN4;
	}
}

//----------------------------------------------------------------------------------------------------
// Dev command "#regeneratemines": clear + re-place the enemy minefields.
[BaseContainerProps()]
class FFRX_RegenerateMinesCommand : ScrServerCommand
{
	override string GetKeyword() { return "regeneratemines"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		FFRX_MinePlacement.Regenerate();
		return ScrServerCmdResult("Regeneration des champs de mines lancee.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
