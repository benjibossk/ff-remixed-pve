// FF - More Civilian Discussion
// Dev command "#testintel": tip the nearest enemy checkpoint's grid coords to the
// caller (as a hint), to validate the text-intel path. Remove for release.
[BaseContainerProps()]
class FFRX_TestIntelCommand : ScrServerCommand
{
	override string GetKeyword() { return "testintel"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		bool ok = FFRX_IntelSystem.TipNearestCheckpoint(playerId, ent.GetOrigin(), 999999);
		if (!ok)
			return ScrServerCmdResult("Aucun checkpoint ennemi actif trouve.", EServerCmdResultType.OK);

		return ScrServerCmdResult("Tuyau checkpoint envoye (regarde le hint).", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
