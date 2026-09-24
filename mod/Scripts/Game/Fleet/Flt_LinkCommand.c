// ============================================================================
//  Flt_LinkCommand — commande chat "#link <code>" pour lier son compte web/Discord
//  à son identité en jeu (UID).
//
//  Flux : le joueur récupère un code sur le site -> en jeu il tape "#link <code>"
//  -> Fleet envoie {code, uid, nom} au site (POST /api/v1/link) -> le site matche
//  le code et lie le compte à l'UID.
//
//  ScrServerCommand est auto-enregistré par le moteur (comme #rank de GMTools).
//  Serveur uniquement, ouvert à TOUS les joueurs.
// ============================================================================
[BaseContainerProps()]
class Flt_LinkCommand : ScrServerCommand
{
	//------------------------------------------------------------------------------------------------
	override string GetKeyword() { return "link"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.NONE; }	// tout joueur peut lier
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		if (argv.Count() < 2)
			return ScrServerCmdResult("Usage: #link <code>", EServerCmdResultType.PARAMETERS);

		string code = argv[1];

		BackendApi ba = GetGame().GetBackendApi();
		PlayerManager pm = GetGame().GetPlayerManager();
		string uid = "";
		string name = "";
		if (ba && playerId > 0)
			uid = ba.GetPlayerIdentityId(playerId);
		if (pm && playerId > 0)
			name = pm.GetPlayerName(playerId);

		if (uid == "")
			return ScrServerCmdResult("UID introuvable, réessayez.", EServerCmdResultType.ERR);

		Flt_GTGPositions.GetInstance().Flt_PostLink(code, uid, name);
		return ScrServerCmdResult("Code envoyé au site — vérifiez la liaison de votre compte.", EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Commande joueur uniquement.", EServerCmdResultType.OK);
	}
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
