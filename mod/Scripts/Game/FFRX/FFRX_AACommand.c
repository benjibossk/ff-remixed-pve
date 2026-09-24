// FF - REMIXED - PVE
// Dev command "#spawnaa": spawn one enemy MANPADS specialist (~120 m from you) so you can
// take off in a helicopter and verify the AI locks + fires a Stinger/Igla at you. Test Brique 2.
[BaseContainerProps()]
class FFRX_AACommand : ScrServerCommand
{
	override string GetKeyword() { return "spawnaa"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity pe = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!pe)
			return ScrServerCmdResult("Pas de perso joueur.", EServerCmdResultType.ERR);

		if (!FFRX_AirDefense.SpawnNear(pe.GetOrigin(), FFRX_AirDefense.AA_PREFAB, false))
			return ScrServerCmdResult("Echec (pas de AA_PREFAB, ou spawn rate).", EServerCmdResultType.ERR);

		return ScrServerCmdResult("Specialiste MANPADS ennemi spawn pres de toi. Prends un helico.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
