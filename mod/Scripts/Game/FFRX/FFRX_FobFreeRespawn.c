// FF - REMIXED - PVE
// Free respawn on an UNSUPPLIED FOB.
//
// FF's FOB respawn is charged in SUPPLIES: CanPlayerRespawn refuses unless the FOB's
// logistics storage holds >= GetChargedValue() supplies (see
// JWK_ResourceRespawnLocationComponent.CanPlayerRespawn -> HasResources). But a bare
// FOB -- including our default starting FOB (FFRX_DefaultFob) -- has NO supply
// storage capacity (max 0), so the cost can NEVER be paid and nobody can spawn on it
// (observed: "Supplies seeded: 0 -> 0 (max 0)" then spawn denied).
//
// Fix (never-block policy): charge the normal supply cost ONLY when the FOB actually
// holds enough supplies; otherwise respawn is FREE (charged value 0). This means:
//   - bare FOB (no storage built, 0 supplies)  -> free  (our default starting FOB)
//   - FOB with a supply storage + stock         -> paid, drains the stock (economy)
//   - FOB whose stock ran dry                   -> free again (no lockout)
// So players can ALWAYS spawn on a FOB, and building/filling a supply storage
// (LogisticsStorageSmall/Medium/Large) simply turns the cost back on while stocked.
// HasResources(type, 0) is trivially true, so CanPlayerRespawn always passes here.
//
// Targets JWK_ResistanceResourceRespawnLocationComponent (the FOB's respawn
// component); towns/other spawns are untouched.

modded class JWK_ResistanceResourceRespawnLocationComponent
{
	override int GetChargedValue()
	{
		int cost = super.GetChargedValue(); // FF's configured supply cost

		// Only charge if the FOB can actually pay; never block a respawn otherwise.
		if (m_Logistics && m_Logistics.GetResources(m_iChargedResourceType) >= cost)
			return cost;

		return 0;
	}
}
