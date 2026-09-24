// FF - REMIXED - PVE
// Spawn hierarchy gating (supersedes the old Brique B town-only gating).
//
// Rule: a respawn location is disabled while any usable base of a STRICTLY
// higher tier exists (tiers in FFRX_SpawnTiers):
//   MOB (2)  -> disables FOB/Camp (1) and Town (0)
//   FOB (1)  -> disables Town (0)
//   MOB      -> never disabled
//
// One override on the base JWK_PlayerRespawnLocationComponent covers the whole
// hierarchy: every subclass (Town / Ticket-Camp / Resource-FOB) calls
// super.CanPlayerRespawn, which runs this suppression check. The probe guard in
// FFRX_SpawnTiers prevents the availability test from recursing.
modded enum JWK_EPlayerRespawnUnavailableReason
{
	FFRX_SUPPRESSED_BY_HIGHER_BASE
}

modded class JWK_PlayerRespawnLocationComponent
{
	override bool CanPlayerRespawn(
		SCR_PlayerController controller,
		out JWK_EPlayerRespawnUnavailableReason outReason
	) {
		// Skip suppression when we are merely probing this location's own
		// availability on behalf of a lower tier (avoids infinite recursion).
		if (!FFRX_SpawnTiers.s_bInAvailabilityProbe) {
			int tier = FFRX_SpawnTiers.TierOf(this);
			if (FFRX_SpawnTiers.HigherTierAvailable(tier, this, controller)) {
				outReason = JWK_EPlayerRespawnUnavailableReason.FFRX_SUPPRESSED_BY_HIGHER_BASE;
				return false;
			}
		}

		return super.CanPlayerRespawn(controller, outReason);
	}

	override LocalizedString GetMessageForUnavailableReason(JWK_EPlayerRespawnUnavailableReason reason)
	{
		if (reason == JWK_EPlayerRespawnUnavailableReason.FFRX_SUPPRESSED_BY_HIGHER_BASE)
			return "Spawn disabled: a more advanced base is available.";

		return super.GetMessageForUnavailableReason(reason);
	}
}
