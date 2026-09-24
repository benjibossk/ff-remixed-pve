// FF - REMIXED - PVE
// Dev command "#resupply": force a marine resupply vessel to arrive now, near the FOB,
// instead of waiting for the timer. Test the fetch loop.
[BaseContainerProps()]
class FFRX_ResupplyCommand : ScrServerCommand
{
	override string GetKeyword() { return "resupply"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		if (!FFRX_MarineResupply.TryResupply())
			return ScrServerCmdResult("Echec (pas de FOB, ou aucune eau proche).", EServerCmdResultType.ERR);

		return ScrServerCmdResult("Navire de ravitaillement en route pres de la FOB.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
