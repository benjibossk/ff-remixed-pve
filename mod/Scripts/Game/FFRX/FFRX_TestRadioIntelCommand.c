// FF - REMIXED - PVE
// Dev command "#radiointel": queue a fake radio report and report the state of the
// pickup path, so we do not have to wait out Reoccupation's 30-120 min report cycle
// to test it. Then walk to a radio site you control and stand there ~8 s.
// Remove for release.
//
// NOTE: ASCII only (Enforce compiler desyncs on UTF-8 accents).
[BaseContainerProps()]
class FFRX_TestRadioIntelCommand : ScrServerCommand
{
	override string GetKeyword() { return "radiointel"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		FFRX_RadioIntel.DebugSetPending(
			"RAPPORT DE TEST\nTrafic ennemi intercepte. Ceci est un rapport factice injecte par #radiointel pour valider la remise sur site."
		);

		float dist = FFRX_RadioIntel.NearestFriendlySiteDistance(ent.GetOrigin());

		int level = 0;
		FF_ReoccupationManager mgr = FF_ReoccupationManager.GetExisting();
		if (mgr)
			level = mgr.GetCurrentIntelNetworkLevel();

		string msg = string.Format("Rapport en attente. Reseau radio niveau %1/6.", level);

		if (dist < 0.0)
		{
			msg += " AUCUN site radio operationnel sous ton controle : capture-en un, sinon le rapport perime dans 30 min.";
		}
		else
		{
			msg += string.Format(
				" Site ami le plus proche : %1 m (il faut etre a moins de %2 m et rester %3 s).",
				Math.Round(dist),
				Math.Round(FFRX_RadioIntel.LISTEN_RADIUS_M),
				Math.Round(FFRX_RadioIntel.LISTEN_SECONDS)
			);
		}

		return ScrServerCmdResult(msg, EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
