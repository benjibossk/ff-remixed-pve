// FF - REMIXED - PVE
// Mine trigger with faction immunity (ported from FFMI's FFMI_FactionTrigger, now REMIXED-owned).
//
// For OUR mines only (registered in FFRX_MineRegistry): only the resistance side (role PLAYER or
// SUPPORTING) triggers them; the enemy AI that "owns" the minefield + civilians walk over safely.
// Mines that aren't ours (e.g. player-placed ACE mines) keep vanilla behavior untouched.
//
// Also adds FFRX_Arm(): public wrapper to arm an inert placed mine (ActivateTrigger is internal).
//
// NOTE: ACE Explosives does NOT mod SCR_PressureTriggerComponent, so this modded class is safe
// (only REMIXED + base game touch it).

modded class SCR_PressureTriggerComponent : SCR_BaseTriggerComponent
{
	// Arm the mine (called by the arming tick when a player approaches).
	void FFRX_Arm()
	{
		ActivateTrigger();
	}

	override void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		// Non-REMIXED mines -> vanilla behavior unchanged.
		if (!FFRX_MineRegistry.IsFFRXMine(owner))
		{
			super.EOnContact(owner, other, contact);
			return;
		}

		// Vanilla anti-spam guard.
		if (GetGame().GetWorld().GetWorldTime() - m_fLastTryTime < MIN_DELAY)
			return;

		if (!other || !other.GetPhysics())
			return;

		// Only the resistance side triggers; enemy (owner) + civilians are spared.
		if (!FFRX_IsResistanceSide(other))
			return;

		super.EOnContact(owner, other, contact);
	}

	protected bool FFRX_IsResistanceSide(IEntity ent)
	{
		IEntity toCheck = ent;

		// Vehicle -> judge by the pilot (empty vehicle = no trigger).
		BaseVehicle vehicle = BaseVehicle.Cast(ent);
		if (vehicle)
		{
			Vehicle v = Vehicle.Cast(ent);
			IEntity pilot;
			if (v) pilot = v.GetPilot();
			if (!pilot) return false;
			toCheck = pilot;
		}

		SCR_ChimeraCharacter chr = SCR_ChimeraCharacter.Cast(toCheck);
		if (!chr) return false;

		SCR_FactionAffiliationComponent aff = SCR_FactionAffiliationComponent.Cast(
			chr.FindComponent(SCR_FactionAffiliationComponent));
		if (!aff) return false;

		JWK_EFactionRole role = JWK.GetFactions().GetRole(aff);
		return (role == JWK_EFactionRole.PLAYER || role == JWK_EFactionRole.SUPPORTING);
	}
}
