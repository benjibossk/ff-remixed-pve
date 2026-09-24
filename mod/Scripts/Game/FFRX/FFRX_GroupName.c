// FF - REMIXED - PVE
// Drop the auto callsign suffix on group names.
//
// Group lists / labels show a group via SCR_AIGroup.GetCustomNameWithOriginal(), which
// returns  "<custom name> ( <callsign> )". Our RP names ("ALPHA - Infanterie", ...) were
// therefore shown as "ALPHA - Infanterie ( 1ere Compagnie - N )" -- and since every squad
// is in company 1, "1ere Compagnie" appeared on every line. We drop the "( callsign )"
// suffix: when a group has a custom name (all of ours do), return just that name; only
// nameless groups fall back to the vanilla callsign.
modded class SCR_AIGroup
{
	override string GetCustomNameWithOriginal()
	{
		string cn = GetCustomName();
		if (cn != "")
			return cn;

		return super.GetCustomNameWithOriginal();
	}
}
