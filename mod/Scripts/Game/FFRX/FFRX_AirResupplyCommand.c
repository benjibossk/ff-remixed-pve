// FF - REMIXED - PVE
// Dev command "#airresupply": force an ENEMY air-resupply run now (a chopper flies to an
// enemy point to reinforce/resupply it), instead of waiting for the timer. Test Brique 1.
[BaseContainerProps()]
class FFRX_AirResupplyCommand : ScrServerCommand
{
	override string GetKeyword() { return "airresupply"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		if (!FFRX_EnemyAirResupply.TryResupply())
			return ScrServerCmdResult("Echec (pas de CHOPPER_PREFAB, ou aucun point ennemi).", EServerCmdResultType.ERR);

		return ScrServerCmdResult("Helico ennemi en route pour ravitailler un point.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
