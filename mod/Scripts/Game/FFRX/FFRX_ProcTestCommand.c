// FF - REMIXED - PVE
// TEMP dev command "#testproc": shows the etat-major ALERT notification to YOU
// (same FF hint style as "can't import on an uncaptured zone"), telling you to
// type #demandes to validate. Lets you see the alert without being in KILO or
// needing a second player. Throwaway dev helper.
//
// Usage in-game chat:  #testproc
[BaseContainerProps()]
class FFRX_ProcTestCommand : ScrServerCommand
{
	override string GetKeyword() { return "testproc"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		// Create a REAL pending request so #demandes has something to open
		// (RplId.Invalid -> approval spawn is a no-op, but the dialog flow works).
		// The requester popup ("... envoyee a l'etat-major") is shown by Notify();
		// the "be in KILO" hint stays in the chat response only, not a popup.
		FFRX_Procurement.CreateShopRequest(playerId, RplId.Invalid(), "M998 Humvee (test)");

		return ScrServerCmdResult("Demande de test creee. Sois en KILO puis tape #demandes.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Commande joueur uniquement (#testproc en jeu).", EServerCmdResultType.OK);
	}
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
