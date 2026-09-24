// FF - REMIXED - PVE
// Dev command "#spawnat": spawn one enemy Javelin AT specialist (~120 m from you) so you can
// drive a vehicle nearby and verify the AI locks + fires a Javelin at it. Test Brique 2 (AT).
[BaseContainerProps()]
class FFRX_ATCommand : ScrServerCommand
{
	override string GetKeyword() { return "spawnat"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity pe = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!pe)
			return ScrServerCmdResult("Pas de perso joueur.", EServerCmdResultType.ERR);

		if (!FFRX_AirDefense.SpawnNear(pe.GetOrigin(), FFRX_AirDefense.AT_PREFAB, true))
			return ScrServerCmdResult("Echec (pas de AT_PREFAB, ou spawn rate).", EServerCmdResultType.ERR);

		return ScrServerCmdResult("Specialiste Javelin ennemi spawn pres de toi. Prends un vehicule.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
