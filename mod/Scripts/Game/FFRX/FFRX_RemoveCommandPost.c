// FF - REMIXED - PVE
// Remove the buildable CommandPost (the FOB tent that spawns a resistance officer
// NPC). Officer services are moving to fixed props instead (armory ->
// BuildingService_FIA, loadouts -> Fleet crate), so the command-post tent and its
// officer are dropped.
//
// Build items are declared in the JWK_BuildItemsConfig root config; its
// Initialize() prunes disabled/invalid entries. We extend it to also drop the
// "CommandPost" entry, so it never appears in the construction menu. Restating
// [BaseContainerProps(configRoot: true)] is required when modding a configRoot
// class or its config binding breaks.
[BaseContainerProps(configRoot: true)]
modded class JWK_BuildItemsConfig
{
	override void Initialize()
	{
		super.Initialize();
		if (!m_aBuildItems) return;

		for (int i = m_aBuildItems.Count() - 1; i >= 0; i--) {
			if (m_aBuildItems[i] && m_aBuildItems[i].m_sName == "CommandPost")
				m_aBuildItems.Remove(i);
		}
	}
}
