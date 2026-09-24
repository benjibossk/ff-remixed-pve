// FF - REMIXED - PVE
// Central predicate for "who is allowed to see map content".
//
// PVE design: regular players get a BLANK map (no icons, no aids). Only server
// admins keep the full tactical map. Kept as a single helper so the rule is
// trivial to change later (e.g. also reveal for the "etat-major" squad, or gate
// on a game setting) without touching every map hook.
//
// SCR_Global.IsAdmin() is a client-side static that reports whether the LOCAL
// player is a server admin, so this is safe to call from map UI code.
class FFRX_MapAccess
{
	static bool LocalPlayerSeesMap()
	{
		return SCR_Global.IsAdmin();
	}
}
