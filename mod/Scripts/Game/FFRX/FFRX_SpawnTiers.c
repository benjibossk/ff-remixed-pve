// FF - REMIXED - PVE
// Shared spawn-tier logic used by both the gating (FFRX_SpawnGating) and the
// auto-deploy ranker (FFRX_AutoSpawn).
//
// Spawn priority tiers:
//   0 = Town  (admin-set starting town)
//   1 = FOB / Camp  (player-built forward base)
//   2 = MOB   (a FOB standing inside a captured MILITARY_BASE build area)
//
// "MOB" is not a native FF entity: FF only has Camps (ticket-based) and FOBs
// (JWK_ResistanceResourceRespawnLocationComponent, supply-based). In FF a MOB is
// simply a FOB built on a military base -- which is exactly what unlocks the
// "MOB" build items (ArmoryMOB, EquipmentStoreMOB): those require the
// MILITARY_BASE build-area flag (JWK_BuildAreaControllerComponent.GetAreaType()).
// So we detect a MOB as: a FOB respawn location whose position falls inside a
// build area of type MILITARY_BASE.
class FFRX_SpawnTiers
{
	// Reentrancy guard: while true, the gating override skips its higher-tier
	// suppression check and reports only the vanilla availability of a location.
	// This lets us ask "is that higher-tier base actually usable?" without the
	// gating recursing into itself.
	static bool s_bInAvailabilityProbe;

	// A MOB = a player FOB (resource respawn) sitting inside a MILITARY_BASE area.
	static bool IsMOB(JWK_PlayerRespawnLocationComponent loc)
	{
		if (!JWK_ResistanceResourceRespawnLocationComponent.Cast(loc)) return false;

		IEntity owner = loc.GetOwner();
		if (!owner) return false;
		vector pos = owner.GetOrigin();

		array<EntityID> areas = JWK_IndexSystem.Get().GetAll(JWK_BuildAreaControllerComponent);
		foreach (EntityID id : areas) {
			JWK_BuildAreaControllerComponent area = JWK_CompTU<JWK_BuildAreaControllerComponent>.FindIn(id);
			if (!area) continue;
			if (!(area.GetAreaType() & JWK_EBuildAreaType.MILITARY_BASE)) continue;
			if (area.Contains(pos)) return true;
		}

		return false;
	}

	// 0 town / 1 FOB or Camp / 2 MOB.
	static int TierOf(JWK_PlayerRespawnLocationComponent loc)
	{
		if (JWK_TownRespawnLocationComponent.Cast(loc)) return 0;
		if (IsMOB(loc)) return 2;
		return 1;
	}

	// Is this location currently spawnable, ignoring our tier suppression?
	// (Runs vanilla checks only: tickets / supplies / faction / enemies.)
	static bool IsUsable(JWK_PlayerRespawnLocationComponent loc, SCR_PlayerController controller)
	{
		s_bInAvailabilityProbe = true;
		JWK_EPlayerRespawnUnavailableReason reason;
		bool ok = loc.CanPlayerRespawn(controller, reason);
		s_bInAvailabilityProbe = false;
		return ok;
	}

	// True if any usable respawn location of a strictly higher tier than `tier`
	// exists (that base then suppresses the caller).
	static bool HigherTierAvailable(int tier, JWK_PlayerRespawnLocationComponent self, SCR_PlayerController controller)
	{
		array<EntityID> ids = JWK_IndexSystem.Get().GetAll(JWK_PlayerRespawnLocationComponent);
		foreach (EntityID id : ids) {
			JWK_PlayerRespawnLocationComponent loc =
				JWK_CompTU<JWK_PlayerRespawnLocationComponent>.FindIn(id);
			if (!loc || loc == self) continue;
			if (TierOf(loc) <= tier) continue;
			if (IsUsable(loc, controller)) return true;
		}

		return false;
	}
}
