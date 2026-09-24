// FF - REMIXED - PVE
// Asymmetric warfare -- BRICK C: booby-trapped cars.
//
// Some abandoned CIVILIAN vehicles in enemy/contested territory are rigged: a buried, ARMED
// FFMI anti-personnel mine sits under them. A resistance player who walks up to loot/steal the
// tempting abandoned car steps on it -> boom. FFMI's faction trigger (SCR_PressureTriggerComponent
// .EOnContact) only fires for PLAYER/SUPPORTING, so civilians and enemy AI walk past safely --
// exactly what we want for a trap aimed at the players.
//
// Server-only, periodic scan. Non-persistent (fresh-game, like the FFMI minefields). ASCII strings.
// Booted from FFRX_Groups.c -> OnGameModeStart. Test command: #trapcar (traps nearest civ vehicle).

class FFRX_BoobyTrapCars
{
	protected static ref FFRX_BoobyTrapCars s_Instance;

	protected static const int   FFRX_SCAN_MS     = 90000;   // scan cadence
	// Chance et plafond viennent des REGLAGES FF (menu admin), plus de constantes en dur :
	// un admin doit pouvoir doser la menace en direct, voire la couper (0 %) sans retirer
	// le mod. Les valeurs ci-dessous ne servent que si le cache de reglages n'est pas encore
	// pret (tout debut de partie).
	protected static const float FFRX_TRAP_CHANCE_FALLBACK = 0.35;
	protected static const int   FFRX_MAX_TRAPPED_FALLBACK = 6;

	//------------------------------------------------------------------------------------------------
	//! Chance qu'un candidat soit piege, en 0..1. Reglage FF "Voitures piegees %".
	protected static float FFRX_TrapChance()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return FFRX_TRAP_CHANCE_FALLBACK;
		return c.m_fFFRX_BoobyTrapPct / 100.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre maximum de pieges actifs. Reglage FF "Voitures piegees : max".
	protected static int FFRX_MaxTrapped()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return FFRX_MAX_TRAPPED_FALLBACK;
		return (int)c.m_fFFRX_BoobyTrapMax;
	}
	protected static const float FFRX_QUERY_HALF  = 20000.0; // world scan half-extent

	// AP mine prefabs = ACE Explosives (bury/disarm actions). Enemy US -> M14, else PMN4 (USSR).
	protected static const ResourceName FFRX_MINE_PMN4 = "{20EFA9ACB024108F}Prefabs/Weapons/Explosives/Mine_PMN4/Mine_PMN4_base.et"; // AP USSR
	protected static const ResourceName FFRX_MINE_M14  = "{91CE54ECA33BCB05}Prefabs/Weapons/Explosives/Mine_M14/Mine_M14_base.et";   // AP US

	protected ref map<EntityID, bool> m_mTrapped = new map<EntityID, bool>();
	protected ref array<IEntity> m_aCandidates = {};

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;
		s_Instance = new FFRX_BoobyTrapCars();
		// Let the world/territory settle, then scan periodically.
		GetGame().GetCallqueue().CallLater(s_Instance.FFRX_Scan, FFRX_SCAN_MS, true);
		Print("[FFRX][BoobyTrap] Systeme voitures piegees demarre.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Trap the single civilian vehicle nearest to a position (used by #trapcar). Returns true if rigged.
	static bool TrapNearest(vector fromPos)
	{
		if (!s_Instance)
			s_Instance = new FFRX_BoobyTrapCars();
		return s_Instance.FFRX_TrapNearestImpl(fromPos);
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_Scan()
	{
		int current = FFRX_CountTrapped();
		if (current >= FFRX_MaxTrapped())
			return;

		m_aCandidates.Clear();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		vector mins = Vector(-FFRX_QUERY_HALF, -1000, -FFRX_QUERY_HALF);
		vector maxs = Vector(FFRX_QUERY_HALF, 3000, FFRX_QUERY_HALF);
		world.QueryEntitiesByAABB(mins, maxs, FFRX_CollectCandidate, FFRX_FilterVehicle, EQueryEntitiesFlags.DYNAMIC);

		int budget = FFRX_MaxTrapped() - current;
		int trappedThisCycle = 0;
		foreach (IEntity veh : m_aCandidates)
		{
			if (budget <= 0)
				break;
			if (Math.RandomFloat01() >= FFRX_TrapChance())
				continue;
			if (FFRX_TrapVehicle(veh))
			{
				budget--;
				trappedThisCycle++;
			}
		}

		Print(string.Format("[FFRX][BoobyTrap] Scan : %1 voiture(s) civile(s) eligible(s), %2 piegee(s) ce cycle (%3 actives au total).",
			m_aCandidates.Count(), trappedThisCycle, FFRX_CountTrapped()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool FFRX_FilterVehicle(IEntity ent)
	{
		return Vehicle.Cast(ent) != null;
	}

	// Candidate = a CIVILIAN (AMBIENT role) vehicle, UNOCCUPIED, in ENEMY/border territory, not yet trapped.
	protected bool FFRX_CollectCandidate(IEntity ent)
	{
		Vehicle v = Vehicle.Cast(ent);
		if (!v)
			return true;

		if (m_mTrapped.Contains(v.GetID()))
			return true;

		// Civilian only (don't rig player/enemy vehicles).
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(v.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return true;
		JWK_FactionManager fm = JWK.GetFactions();
		if (!fm)
			return true;
		if (fm.GetRole(fac) != JWK_EFactionRole.AMBIENT)
			return true;

		// Unoccupied only (never rig a car a civilian is currently driving).
		if (FFRX_HasOccupant(v))
			return true;

		// Enemy or frontline territory only.
		if (!FFRX_InEnemyTerritory(v.GetOrigin()))
			return true;

		m_aCandidates.Insert(v);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FFRX_TrapNearestImpl(vector fromPos)
	{
		m_aCandidates.Clear();
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		vector mins = Vector(-FFRX_QUERY_HALF, -1000, -FFRX_QUERY_HALF);
		vector maxs = Vector(FFRX_QUERY_HALF, 3000, FFRX_QUERY_HALF);
		// For the test command we accept ANY civilian vehicle (skip the territory gate).
		world.QueryEntitiesByAABB(mins, maxs, FFRX_CollectAnyCiv, FFRX_FilterVehicle, EQueryEntitiesFlags.DYNAMIC);

		IEntity best = null;
		float bestSq = 999999999.0;
		foreach (IEntity veh : m_aCandidates)
		{
			float dSq = vector.DistanceSqXZ(veh.GetOrigin(), fromPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = veh;
			}
		}
		if (!best)
			return false;
		return FFRX_TrapVehicle(best);
	}

	// Test-command candidate collector: any unoccupied, un-trapped CIVILIAN vehicle (no territory gate).
	protected bool FFRX_CollectAnyCiv(IEntity ent)
	{
		Vehicle v = Vehicle.Cast(ent);
		if (!v)
			return true;
		if (m_mTrapped.Contains(v.GetID()))
			return true;
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(v.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return true;
		JWK_FactionManager fm = JWK.GetFactions();
		if (!fm || fm.GetRole(fac) != JWK_EFactionRole.AMBIENT)
			return true;
		if (FFRX_HasOccupant(v))
			return true;
		m_aCandidates.Insert(v);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Spawn a buried, armed FFMI AP mine under the vehicle. Register it (needed for the FFMI trigger
	// to activate) and mark the vehicle as trapped.
	protected bool FFRX_TrapVehicle(IEntity veh)
	{
		if (!veh)
			return false;

		vector pos = veh.GetOrigin();
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]) - 0.06; // shallow bury

		Resource res = Resource.Load(FFRX_EnemyAPPrefab());
		if (!res || !res.IsValid())
			return false;

		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(sp.Transform);
		sp.Transform[3] = pos;

		IEntity mine = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		if (!mine)
			return false;

		// Register so REMIXED's faction trigger treats it as one of ours (immunity for civ/enemy),
		// then arm it -> only a resistance player who walks over it triggers it.
		FFRX_MineRegistry.Register(mine);
		SCR_PressureTriggerComponent trig = SCR_PressureTriggerComponent.Cast(mine.FindComponent(SCR_PressureTriggerComponent));
		if (trig)
			trig.FFRX_Arm();

		m_mTrapped.Set(veh.GetID(), true);
		Print(string.Format("[FFRX][BoobyTrap] Voiture civile piegee a %1 (mine AP armee).", pos.ToString()), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected int FFRX_CountTrapped()
	{
		// Prune vehicles that no longer exist (streamed out / destroyed), count the rest.
		BaseWorld world = GetGame().GetWorld();
		array<EntityID> keys = {};
		foreach (EntityID id, bool v : m_mTrapped)
			keys.Insert(id);

		int n = 0;
		foreach (EntityID id : keys)
		{
			IEntity e = null;
			if (world)
				e = world.FindEntityByID(id);
			if (!e)
				m_mTrapped.Remove(id);
			else
				n++;
		}
		return n;
	}

	protected bool FFRX_HasOccupant(IEntity v)
	{
		BaseCompartmentManagerComponent cm = BaseCompartmentManagerComponent.Cast(v.FindComponent(BaseCompartmentManagerComponent));
		if (!cm)
			return false;
		array<BaseCompartmentSlot> slots = {};
		cm.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (slot && slot.GetOccupant())
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

	protected ResourceName FFRX_EnemyAPPrefab()
	{
		JWK_Faction enemy = JWK.GetFactions().GetJWKFactionByRole(JWK_EFactionRole.ENEMY);
		if (enemy && enemy.GetKey() == "US")
			return FFRX_MINE_M14;
		return FFRX_MINE_PMN4;
	}
}

//----------------------------------------------------------------------------------------------------
// Dev command "#trapcar": booby-trap the civilian vehicle nearest to the caller. Remove for release.
[BaseContainerProps()]
class FFRX_TrapCarCommand : ScrServerCommand
{
	override string GetKeyword() { return "trapcar"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		if (FFRX_BoobyTrapCars.TrapNearest(ent.GetOrigin()))
			return ScrServerCmdResult("Voiture civile la plus proche piegee (mine AP armee dessous).", EServerCmdResultType.OK);
		return ScrServerCmdResult("Aucune voiture civile inoccupee trouvee a proximite.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
