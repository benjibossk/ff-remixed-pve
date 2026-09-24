// FF - REMIXED - PVE
// Dev command "#tipmines": reveal the nearest enemy minefield (FFMI AP zone) to the caller as
// a text hint + a local map marker. Validates the minefield-intel path. Remove for release.
[BaseContainerProps()]
class FFRX_TestMinefieldCommand : ScrServerCommand
{
	override string GetKeyword() { return "tipmines"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		bool ok = FFRX_IntelSystem.TipNearestMinefield(playerId, ent.GetOrigin(), 999999);
		if (!ok)
			return ScrServerCmdResult("Aucun champ de mines FFMI enregistre (mines posees ? nouvelle partie ?).", EServerCmdResultType.OK);

		return ScrServerCmdResult("Tuyau champ de mines envoye (hint + marqueur carte).", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
