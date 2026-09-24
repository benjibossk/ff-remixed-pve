// FF - REMIXED - PVE
// Jammer rework STEP 1 -- make jammers SITUATIONAL instead of always-on.
//
// Stock SAL_DroneJammerComponent auto-registers on EOnInit (jams 24/7), which would make
// player drones useless anywhere near an enemy squad = kills the drone feature. We drop the
// auto-on and split behaviour by who carries the JammerBag:
//   - AI-carried (enemy squads): auto-activate ONLY while a HOSTILE active drone is within the
//     jam range, deactivate otherwise -> player drones work until they close on an enemy jammer
//     (cat & mouse). Faction-aware via the carrier (the bag item has no faction of its own).
//   - Player-carried: NO auto. OFF by default; the player uses the mod's manual toggle
//     (SAL_ToggleJammerCharacter) to switch it on when they hear a drone.
//
// Steps 2 (battery/drain) and 3 (detectability) build on this. Server-side, ASCII strings.

modded class SAL_DroneJammerComponent
{
	protected bool m_bFFRXActive = false;   // our AI auto on/off state (separate from manual toggle)
	protected bool m_bFFRXAuto = false;     // this jammer runs the AI auto-loop
	protected float m_fFFRXBaseRange = -1.0;   // open-air jam range, captured once
	protected bool m_bFFRXIndoor = false;
	protected float m_fFFRXBattery = -1.0;   // seconds of ACTIVE jamming left (-1 = not init yet)
	protected bool m_bFFRXDepleted = false;  // battery empty -> cannot jam until recharged
	// One timer to rule them all. This used to be THREE recurring CallLater per jammer
	// (range 3 s + battery 3 s + auto-check 2.5 s), i.e. about one call per second per jammer,
	// each with its own queue entry -- and the range check does a raycast. They are merged into
	// a single 5 s tick: same behaviour, roughly a third of the cost.
	// If you change this value, the battery drain follows automatically (see FFRX_BatteryTick).
	protected static const float FFRX_TICK = 5000.0;
	protected static const float FFRX_INDOOR_FACTOR = 0.2;   // range x0.2 indoors (e.g. 300 -> 60 m)
	protected static const float FFRX_ROOF_TRACE_UP = 8.0;   // metres to look up for a roof
	protected static const float FFRX_BATTERY_MAX = 300.0;   // seconds of active jamming on a full battery
	protected static const float FFRX_IDLE_RECHARGE = 0.25;  // AI jammers recover at this fraction of drain while idle
	protected static const float FFRX_AI_SWAP_MS = 45000.0;  // how long an AI operator takes to fit a spare cell
	// NB: the spare cell itself is declared in JammerBag.et (MultiSlots), not here.
	protected float m_fFFRXSwapReadyAt = 0;

	// Replace the stock auto-register (base EOnInit -> RegisterJammer). Keep base OnPostInit
	// (it sets the INIT event mask) so EOnInit still fires.
	override void EOnInit(IEntity owner)
	{
		#ifndef WORKBENCH
		if (!Replication.IsServer())
			return;
		#endif
		// Wait for the loadout to settle (bag worn -> carrier reachable), then decide behaviour.
		GetGame().GetCallqueue().CallLater(FFRX_Setup, 2000, false, owner);
	}

	void FFRX_Setup(IEntity owner)
	{
		if (!owner)
			return;

		IEntity carrier = FFRX_Carrier(owner);

		// The % of vehicles that CARRY a jammer is handled at the vehicle level by
		// FFRX_JammerSpawnGate (it only spawns+attaches the bag on a lucky roll). Here a
		// vehicle-mounted bag just becomes situational like any AI jammer.

		// Capture the open-air jam range once, then keep m_fJammingRange updated by environment
		// (indoors = drastically reduced). Runs for ALL jammers (player + AI + vehicle).
		if (m_fFFRXBaseRange < 0)
		{
			m_fFFRXBaseRange = m_fJammingRange;
			if (m_fFFRXBaseRange < 1.0)
				m_fFFRXBaseRange = 300.0;
		}
		if (m_fFFRXBattery < 0)
			m_fFFRXBattery = FFRX_BATTERY_MAX;   // starts on a full charge
		GetGame().GetCallqueue().CallLater(FFRX_Tick, FFRX_TICK, true, owner);

		// Character carrier: player = manual toggle, AI = auto. (Vehicles already fall through as AI.)
		bool isPlayer = false;
		if (carrier && !Vehicle.Cast(carrier))
		{
			PlayerManager pm = GetGame().GetPlayerManager();
			if (pm && pm.GetPlayerIdFromControlledEntity(carrier) > 0)
				isPlayer = true;
		}

		if (isPlayer)
			return;   // player jammer -> manual toggle, OFF by default

		// NB: the spare cell is NOT spawned here. It ships with the bag via the prefab's
		// MultiSlots (JammerBag.et) -- the engine's own pre-fill, same as the AMF TECPACK
		// backpacks. No script, no spawn call, and it works for AI, players and static bags.
		m_bFFRXAuto = true;   // AI jammer: FFRX_Tick will also run the auto on/off check
	}

	//------------------------------------------------------------------------------------------------
	//! Single periodic tick for a jammer: environment (range), battery, and -- for AI jammers --
	//! the automatic on/off decision. Merged from three separate timers.
	void FFRX_Tick(IEntity owner)
	{
		if (!owner)
			return;

		FFRX_UpdateRange(owner);
		FFRX_BatteryTick(owner);

		if (m_bFFRXAuto)
			FFRX_Check(owner);
	}

	// Drastically reduce the jam range when the jammer is INDOORS (walls contain the RF). Since
	// SAL and JamAiDrones both read m_fJammingRange, updating it changes the effective range for
	// player AND AI drones for free. No fixed 300 m bubble through walls.
	void FFRX_UpdateRange(IEntity owner)
	{
		if (!owner)
			return;

		bool indoor = FFRX_IsIndoor(owner);
		if (indoor == m_bFFRXIndoor && m_fJammingRange > 0.1)
			return;   // no change

		m_bFFRXIndoor = indoor;

		string env;
		if (indoor)
		{
			m_fJammingRange = m_fFFRXBaseRange * FFRX_INDOOR_FACTOR;
			env = "INTERIEUR";
		}
		else
		{
			m_fJammingRange = m_fFFRXBaseRange;
			env = "exterieur";
		}

		Print(string.Format("[FFRX][Jammer] Environnement: %1 -> portee jam = %2 m.", env, (int)m_fJammingRange), LogLevel.NORMAL);
	}

	// Drain the battery while the jammer is ACTIVELY jamming (registered). When empty, force it
	// off and block reactivation until a battery is swapped in. Runs for player + AI jammers.
	void FFRX_BatteryTick(IEntity owner)
	{
		if (!owner)
			return;

		SAL_DroneConnectionManager mgr = SAL_DroneConnectionManager.GetInstance();
		if (!mgr || !mgr.m_aJammers)
			return;

		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		if (!rpl)
			return;

		bool active = (mgr.m_aJammers.Find(rpl.Id()) != -1);
		float step = FFRX_TICK / 1000.0;

		if (!active)
		{
			if (!m_bFFRXAuto || m_fFFRXBattery >= FFRX_BATTERY_MAX)
				return;

			// AI operators carry spare cells like anyone else: when the pack runs dry, the man
			// swaps in a battery from his own inventory. That is the "real" recharge -- it costs
			// him a cell, and once he is out of cells his jammer is done for good.
			if (m_bFFRXDepleted && FFRX_TryAiBatterySwap(owner))
				return;

			// No cell left: a slow trickle so a depleted enemy jammer is not permanently dead
			// weight. Player jammers never auto-recharge -- they must be fed a battery.
			m_fFFRXBattery = Math.Min(m_fFFRXBattery + step * FFRX_IDLE_RECHARGE, FFRX_BATTERY_MAX);
			if (m_fFFRXBattery > 5)
				m_bFFRXDepleted = false;

			return;
		}

		// Active: drain.
		m_fFFRXBattery = m_fFFRXBattery - step;
		if (m_fFFRXBattery > 0)
			return;

		// depleted -> shut it down until recharged
		m_fFFRXBattery = 0;
		m_bFFRXDepleted = true;
		mgr.UnregisterJammer(rpl.Id());
		m_bFFRXActive = false;
		Print("[FFRX][Jammer] Batterie VIDE -> jammer coupe.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! AI battery swap.
	//!
	//! An AI operator carries a finite number of spare cells. When his pack runs dry he fits one,
	//! after a delay that stands for the swap itself -- so a jammed-out enemy jammer stays down
	//! long enough to be exploited, then comes back.
	//!
	//! The cells are REAL inventory items (given at spawn by FFRX_GiveSpareCells), so killing the
	//! operator before he swaps lets you loot them -- and once he is out, his jammer is done.
	//!
	//! \return true when a cell was consumed (or a swap is already under way).
	protected bool FFRX_TryAiBatterySwap(IEntity bag)
	{
		IEntity cell = FFRX_FindCarriedCell(bag);
		if (!cell)
			return false;

		float now = GetGame().GetWorld().GetWorldTime();

		// Swap in progress: hold the jammer down until it completes.
		if (m_fFFRXSwapReadyAt > 0)
		{
			if (now < m_fFFRXSwapReadyAt)
				return true;

			m_fFFRXSwapReadyAt = 0;

			float cap = FFRX_BATTERY_MAX;
			SAL_BatteryComponent bc = SAL_BatteryComponent.Cast(cell.FindComponent(SAL_BatteryComponent));
			if (bc)
				cap = bc.m_fBatteryStorage;

			FFRX_Recharge(cap);
			SCR_EntityHelper.DeleteEntityAndChildren(cell);
			Print("[FFRX][Jammer] Operateur IA: batterie de rechange installee.");
			return true;
		}

		m_fFFRXSwapReadyAt = now + FFRX_AI_SWAP_MS;
		Print("[FFRX][Jammer] Operateur IA: batterie vide, remplacement en cours...");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Find a drone cell: first inside the pack, then on whoever carries it (so a player can also
	//! feed it from his own kit).
	protected IEntity FFRX_FindCarriedCell(IEntity bag)
	{
		if (!bag)
			return null;

		IEntity found = FFRX_FindCellIn(bag);
		if (found)
			return found;

		return FFRX_FindCellIn(FFRX_Carrier(bag));
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FFRX_FindCellIn(IEntity holder)
	{
		if (!holder)
			return null;

		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(holder.FindComponent(InventoryStorageManagerComponent));
		if (!inv)
			return null;

		array<IEntity> items = {};
		inv.GetItems(items);

		foreach (IEntity item : items)
		{
			if (item && item.FindComponent(SAL_BatteryComponent))
				return item;
		}

		return null;
	}

	// Refill the battery, called by the FFRX_JammerReplaceBattery inventory action, by the
	// drag-and-drop path (FFRX_JammerBatteryDrop) or by an AI battery swap. amount = the
	// consumed cell's capacity (seconds); a full drone battery therefore tops it right up.
	void FFRX_Recharge(float amount)
	{
		if (amount <= 0)
			amount = FFRX_BATTERY_MAX;
		if (m_fFFRXBattery < 0)
			m_fFFRXBattery = 0;
		m_fFFRXBattery = Math.Min(m_fFFRXBattery + amount, FFRX_BATTERY_MAX);
		m_bFFRXDepleted = false;
		Print(string.Format("[FFRX][Jammer] Batterie rechargee -> %1 s.", (int)m_fFFRXBattery), LogLevel.NORMAL);
	}

	bool FFRX_IsDepleted()
	{
		return m_bFFRXDepleted;
	}

	// Current charge as 0..100 (for UI / the replace-battery action name).
	int FFRX_BatteryPct()
	{
		if (m_fFFRXBattery < 0)
			return 100;   // not initialised yet -> assume full
		return Math.ClampInt((int)(m_fFFRXBattery / FFRX_BATTERY_MAX * 100.0), 0, 100);
	}

	// Vertical trace up from the jammer: if it hits a roof within FFRX_ROOF_TRACE_UP metres, the
	// jammer is considered indoors.
	bool FFRX_IsIndoor(IEntity owner)
	{
		BaseWorld world = owner.GetWorld();
		if (!world)
			return false;

		vector pos = owner.GetOrigin();
		TraceParam trace = new TraceParam();
		trace.Start = Vector(pos[0], pos[1] + 1.0, pos[2]);
		trace.End = Vector(pos[0], pos[1] + 1.0 + FFRX_ROOF_TRACE_UP, pos[2]);
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;

		IEntity carrier = FFRX_Carrier(owner);
		if (carrier)
			trace.Exclude = carrier;

		float frac = world.TraceMove(trace, null);
		return frac < 0.99;   // hit a roof above -> indoors
	}

	void FFRX_Check(IEntity owner)
	{
		if (!owner)
			return;

		bool near = FFRX_HostileDroneNear(owner);

		if (near && !m_bFFRXActive && !m_bFFRXDepleted)
		{
			RegisterJammer(owner);   // base: inserts into m_aJammers + sets m_Id
			m_bFFRXActive = true;
			Print("[FFRX][Jammer] IA -> jammer ON (drone hostile dans la portee).", LogLevel.NORMAL);
		}
		else if (!near && m_bFFRXActive)
		{
			SAL_DroneConnectionManager mgr = SAL_DroneConnectionManager.GetInstance();
			if (mgr)
				mgr.UnregisterJammer(m_Id);
			m_bFFRXActive = false;
			Print("[FFRX][Jammer] IA -> jammer OFF (plus de drone proche).", LogLevel.NORMAL);
		}
	}

	// A hostile active drone (different faction from the carrier) within jam range.
	bool FFRX_HostileDroneNear(IEntity owner)
	{
		SAL_DroneConnectionManager mgr = SAL_DroneConnectionManager.GetInstance();
		if (!mgr || !mgr.m_aActiveDrones)
			return false;

		FactionKey myFac = FFRX_CarrierFaction(owner);

		float r = m_fJammingRange;
		if (r < 1.0)
			r = 300.0;
		float r2 = r * r;

		vector jp = owner.GetOrigin();

		int n = mgr.m_aActiveDrones.Count();
		for (int i = 0; i < n; i++)
		{
			Managed item = Replication.FindItem(mgr.m_aActiveDrones[i]);
			RplComponent rpl = RplComponent.Cast(item);
			if (!rpl)
				continue;

			IEntity drone = rpl.GetEntity();
			if (!drone)
				continue;

			// skip friendly drones (same faction as the jammer's carrier) -> no self-jam
			if (myFac != "")
			{
				FactionAffiliationComponent dfac = FactionAffiliationComponent.Cast(drone.FindComponent(FactionAffiliationComponent));
				if (dfac)
				{
					Faction df = dfac.GetAffiliatedFaction();
					if (df && df.GetFactionKey() == myFac)
						continue;
				}
			}

			vector dp = drone.GetOrigin();
			float dx = dp[0] - jp[0];
			float dy = dp[1] - jp[1];
			float dz = dp[2] - jp[2];
			if (dx * dx + dy * dy + dz * dz <= r2)
				return true;
		}

		return false;
	}

	// Walk up the entity hierarchy from the worn bag to the character (first entity with a
	// FactionAffiliationComponent).
	IEntity FFRX_Carrier(IEntity bag)
	{
		IEntity e = bag;
		int g = 0;
		while (e && g < 10)
		{
			if (FactionAffiliationComponent.Cast(e.FindComponent(FactionAffiliationComponent)))
				return e;
			e = e.GetParent();
			g++;
		}
		return null;
	}

	FactionKey FFRX_CarrierFaction(IEntity bag)
	{
		IEntity c = FFRX_Carrier(bag);
		if (!c)
			return "";
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(c.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return "";
		Faction f = fac.GetAffiliatedFaction();
		if (!f)
			return "";
		return f.GetFactionKey();
	}
}
