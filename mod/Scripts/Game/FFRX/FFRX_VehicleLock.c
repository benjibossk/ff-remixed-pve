// FF - REMIXED - PVE
// Vehicle keys / lock for PROCURED vehicles (D12 + Pilier 8).
//
// Design (Benji): a vehicle bought via procurement is LOCKED by default; the player who
// REQUESTED it becomes the OWNER (gets the keys). The etat-major squad (KILO) is a
// PASS-KEY: its members can enter any procured vehicle.
//
// We reuse FF's native ownership system 100% -- no new lock/enforcement code:
//   - JWK_OwnershipAccessComponent : owner UID (m_sOwnerPersId) + allowed UIDs
//     (m_aAllowedPersId), SetPlayerOwner_S / SetLocked_S. Both are RplProps -> replicated.
//   - FF's get-in action (JWK_SCR_GetInUserAction) already blocks entry via
//     HasLocalPlayerAccess() = owner OR in the allowed list.
//   - Gated by the FF setting m_bAllowLockVics (forced ON in FFRX_GameSettings).
//
// Etat-major pass-key WITHOUT any client-side replication plumbing: we simply push every
// current etat-major member's UID into the vehicle's ALLOWED list (which is already
// replicated) at lock time. So HasLocalPlayerAccess() returns true for them natively on
// clients -- no need for IsEtatMajor() to work client-side. When a player JOINS the
// etat-major later, FFRX_Groups re-grants them access to all currently-locked vehicles
// (GrantEtatMajorAccess). Server only.

// A pending "lock the vehicle spawned near here" request (the procurement spawn happens
// inside FF, so we can't grab the entity directly -> we scan for it a beat later).
class FFRX_PendingVehicleLock
{
	vector       pos;
	int          ownerId;
	int          attempts;
}

class FFRX_VehicleLock
{
	protected static ref FFRX_VehicleLock s_Instance;

	protected static const float SCAN_RADIUS   = 14.0;  // m around the delivery point
	protected static const int   SCAN_TICK_MS  = 400;
	protected static const int   SCAN_MAX_TRY  = 8;

	protected ref array<ref FFRX_PendingVehicleLock> m_aPending = {};
	protected ref array<IEntity> m_aLocked = {};   // every vehicle we locked (for re-grants)
	protected ref array<IEntity> m_aScan   = {};   // AABB query accumulator

	//------------------------------------------------------------------------------------------------
	static FFRX_VehicleLock Get()
	{
		if (!s_Instance) s_Instance = new FFRX_VehicleLock();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	// Lock a KNOWN vehicle entity to its owner + grant the current etat-major.
	static void LockToPlayer(IEntity vehicle, int ownerPlayerId)
	{
		if (!vehicle || !Replication.IsServer()) return;

		JWK_OwnershipAccessComponent own = JWK_CompTU<JWK_OwnershipAccessComponent>.FindIn(vehicle);
		if (!own)
		{
			Print("[FFRX][VehLock] Vehicule sans JWK_OwnershipAccessComponent -> non verrouillable.", LogLevel.WARNING);
			return;
		}

		string uid = FFRX_LoadoutSystem.UidOfPlayer(ownerPlayerId);
		own.SetPlayerOwner_S(uid);       // owner
		own.SetLocked_S(true);           // locked

		// Pass-key: every current etat-major member gets access (added to the replicated
		// allowed list -> works client-side with no extra plumbing).
		array<int> em = {};
		FFRX_GroupsManager.GetEtatMajorPlayers(em);
		foreach (int pid : em)
		{
			string euid = FFRX_LoadoutSystem.UidOfPlayer(pid);
			own.FFRX_AddAllowed_S(euid);
		}

		Get().m_aLocked.Insert(vehicle);
		Print(string.Format("[FFRX][VehLock] Vehicule verrouille -> proprietaire pid=%1 uid='%2' (+%3 etat-major).",
			ownerPlayerId, uid, em.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// Lock the fresh, unowned vehicle spawned near `pos` (procurement delivery). Deferred
	// because FF spawns it after this call returns.
	static void LockProcuredNear(vector pos, int ownerPlayerId)
	{
		if (!Replication.IsServer()) return;

		FFRX_PendingVehicleLock p = new FFRX_PendingVehicleLock();
		p.pos      = pos;
		p.ownerId  = ownerPlayerId;
		p.attempts = 0;
		Get().m_aPending.Insert(p);
		GetGame().GetCallqueue().CallLater(Get().ScanTick, SCAN_TICK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	// A player just joined the etat-major -> give them the keys to every locked vehicle.
	static void GrantEtatMajorAccess(int playerId)
	{
		if (!Replication.IsServer()) return;
		string uid = FFRX_LoadoutSystem.UidOfPlayer(playerId);
		if (uid == "") return;

		FFRX_VehicleLock self = Get();
		int granted = 0;
		for (int i = self.m_aLocked.Count() - 1; i >= 0; i--)
		{
			IEntity v = self.m_aLocked[i];
			if (!v || v.IsDeleted()) { self.m_aLocked.Remove(i); continue; }
			JWK_OwnershipAccessComponent own = JWK_CompTU<JWK_OwnershipAccessComponent>.FindIn(v);
			if (own) { own.FFRX_AddAllowed_S(uid); granted++; }
		}
		if (granted > 0)
			Print(string.Format("[FFRX][VehLock] Etat-major pid=%1 -> clefs de %2 vehicule(s).", playerId, granted), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void ScanTick()
	{
		for (int i = m_aPending.Count() - 1; i >= 0; i--)
		{
			FFRX_PendingVehicleLock p = m_aPending[i];
			IEntity veh = FindFreshVehicle(p.pos);
			if (veh)
			{
				FFRX_VehicleLock.LockToPlayer(veh, p.ownerId);
				m_aPending.Remove(i);
				continue;
			}
			p.attempts = p.attempts + 1;
			if (p.attempts >= SCAN_MAX_TRY)
			{
				Print("[FFRX][VehLock] Vehicule procure introuvable pres du point de livraison (abandon).", LogLevel.WARNING);
				m_aPending.Remove(i);
			}
		}
		if (!m_aPending.IsEmpty())
			GetGame().GetCallqueue().CallLater(ScanTick, SCAN_TICK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	// Closest UNOWNED vehicle within SCAN_RADIUS of pos (the one procurement just spawned).
	protected IEntity FindFreshVehicle(vector pos)
	{
		m_aScan.Clear();
		BaseWorld world = GetGame().GetWorld();
		if (!world) return null;

		vector mins = pos - Vector(SCAN_RADIUS, 12, SCAN_RADIUS);
		vector maxs = pos + Vector(SCAN_RADIUS, 12, SCAN_RADIUS);
		world.QueryEntitiesByAABB(mins, maxs, AddVehicle, FilterVehicle, EQueryEntitiesFlags.DYNAMIC);

		IEntity best = null;
		float bestSq = SCAN_RADIUS * SCAN_RADIUS;
		foreach (IEntity e : m_aScan)
		{
			JWK_OwnershipAccessComponent own = JWK_CompTU<JWK_OwnershipAccessComponent>.FindIn(e);
			if (!own) continue;
			if (own.GetPlayerOwnerUid() != "") continue;   // already owned -> not the fresh one
			float dSq = vector.DistanceSqXZ(e.GetOrigin(), pos);
			if (dSq < bestSq) { bestSq = dSq; best = e; }
		}
		return best;
	}

	protected bool FilterVehicle(IEntity e) { return Vehicle.Cast(e) != null; }
	protected bool AddVehicle(IEntity e) { m_aScan.Insert(e); return true; }
}

//----------------------------------------------------------------------------------------------------
// Add a public "grant access to this UID" to the FF ownership component (m_aAllowedPersId
// is protected, no public adder in FF). Used for the etat-major pass-key.
modded class JWK_OwnershipAccessComponent
{
	void FFRX_AddAllowed_S(string uid)
	{
		if (uid == "" || uid == m_sOwnerPersId) return;
		if (m_aAllowedPersId.Contains(uid)) return;
		m_aAllowedPersId.Insert(uid);
		Replication.BumpMe();
	}
}
