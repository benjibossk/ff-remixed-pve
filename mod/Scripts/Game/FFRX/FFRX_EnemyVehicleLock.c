// FF - REMIXED - PVE
// Empty enemy vehicles are LOCKED (Pilier 8 / D5 -- booty loop).
//
// Design (Benji): an ABANDONED enemy vehicle (crew killed / no AI left) can NOT simply be
// driven away by the resistance. It is "locked": the only way to profit from it is to TOW
// it back with a wrecker and SELL it (Phase 2). This turns enemy vehicle kills into a
// logistics/salvage objective instead of free transport.
//
// FF already blocks entering an OCCUPIED enemy vehicle (JWK_VehicleUtils.IsVehicleHostile
// only returns true when a LIVING enemy sits inside). Once the crew is dead the vehicle is
// empty -> IsVehicleHostile is false -> vanilla lets the player hop in. We close that gap:
// we deny "Get in" on ANY enemy-FACTION vehicle (empty or not) for the resistance side.
// Enemy AI are NOT affected (they keep using their own vehicles).
//
// Client-safe: the vehicle faction is replicated, so the CanBePerformedScript check works
// on the owning client (the action greys out) and the server re-validation agrees.

class FFRX_EnemyVehicleLock
{
	// True if `user` (resistance) must NOT be allowed to enter `vehicle` (enemy-owned).
	static bool IsLockedEnemyVehicle(Vehicle vehicle, IEntity user)
	{
		if (!vehicle || !user)
			return false;

		JWK_FactionManager fm = JWK.GetFactions();
		if (!fm)
			return false;

		// Don't restrict enemy AI from using their own vehicles.
		if (fm.GetEntityRole(user) == JWK_EFactionRole.ENEMY)
			return false;

		// Lock enemy-FACTION vehicles (independent of whether a crew is inside).
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return false;

		return fm.GetRole(fac) == JWK_EFactionRole.ENEMY;
	}
}

//----------------------------------------------------------------------------------------------------
// Chains ON TOP of FF's own modded SCR_GetInUserAction (modded classes stack across addons):
// super runs the vanilla + FF checks (destroyed / hostile-occupied / lock / ownership), then
// we add the empty-enemy-vehicle gate.
modded class SCR_GetInUserAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		if (!super.CanBePerformedScript(user))
			return false;

		BaseCompartmentSlot compartment = GetCompartmentSlot();
		if (compartment)
		{
			Vehicle vehicle = Vehicle.Cast(SCR_EntityHelper.GetMainParent(compartment.GetOwner(), true));
			if (FFRX_EnemyVehicleLock.IsLockedEnemyVehicle(vehicle, user))
			{
				SetCannotPerformReason("Vehicule ennemi verrouille - remorquez-le pour le revendre.");
				return false;
			}
		}

		return true;
	}
}
