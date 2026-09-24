// FF - REMIXED - PVE
// Blank map for non-admins.
//
// Every FF map layer (POI/icons, territory-control overlay, road routes, radio
// range, illegal zones, player markers, debug) derives from JWK_BaseMapModule.
// That base class already gates all of its drawing on m_bVisible:
//   - OnMapOpen hides the module root widget when m_bVisible is false;
//   - Update() early-outs (draws nothing) when m_bVisible is false.
// So a single modded class that forces m_bVisible=false for non-admins wipes
// EVERY FF layer at once -- no need to touch each module individually.
//
// We hook two points:
//   OnMapOpen  -> force the layer hidden as soon as the map opens.
//   SetVisible -> CLAMP: a non-admin can never turn a layer back on, even if a
//                 subclass' OnMapOpen or the map display-settings UI tries to.
//
// Admins are unaffected: LocalPlayerSeesMap() returns true, so both overrides
// fall through to the vanilla behaviour.
//
// NOTE: this covers all FF (JWK_) layers. Residual base-game map elements (own
// quest/task markers, spawn points, player-drawn markers, legend/scale) are a
// separate, smaller follow-up if they turn out to still show for players.
modded class JWK_BaseMapModule
{
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		if (!FFRX_MapAccess.LocalPlayerSeesMap())
			SetVisible(false);
	}

	override void SetVisible(bool visible)
	{
		if (visible && !FFRX_MapAccess.LocalPlayerSeesMap())
			visible = false;

		super.SetVisible(visible);
	}
}
