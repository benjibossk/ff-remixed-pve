// ============================================================================
//  Flt_XpCommand — commande chat "#xp" : affiche au joueur son grade, son XP et
//  le palier courant (plancher -> plafond). Sert à vérifier que l'XP de temps de
//  jeu monte bien et où se situe le plafond du grade.
//
//  Usage en jeu : ouvrir le chat et taper  #xp
//  Serveur-side ; la réponse est renvoyée au joueur qui a tapé la commande.
//  Accessible à TOUT le monde (pas seulement admin).
// ============================================================================
[BaseContainerProps()]
class Flt_XpCommand : ScrServerCommand
{
	//------------------------------------------------------------------------------------------------
	override string GetKeyword() { return "xp"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.NONE; }	// tout joueur voit SON XP
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return BuildReport(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Commande joueur uniquement (#xp en jeu).", EServerCmdResultType.OK);
	}
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }

	// ----------------------------------------------------------------------------------------------------

	protected ScrServerCmdResult BuildReport(int playerId)
	{
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba)
			return ScrServerCmdResult("XP indisponible.", EServerCmdResultType.ERR);

		string uid = ba.GetPlayerIdentityId(playerId);
		if (uid == "")
			return ScrServerCmdResult("UID introuvable, réessayez.", EServerCmdResultType.ERR);

		Flt_XpProgression.Load();

		int grade   = Flt_XpProgression.GradeForUid(uid);
		int xp      = Flt_XpProgression.GetXp(uid);
		int floorXP = Flt_XpProgression.GradeFloor(grade);
		int ceilXP  = Flt_XpProgression.GradeCeiling(grade);

		int minutes = Flt_XpProgression.GetPlaytime(uid) / 60;

		string msg = string.Format(
			"Grade #%1 (%2) | XP %3 | palier %4 -> %5 | temps de jeu %6 min",
			grade, Flt_RankNames.Get(grade), xp, floorXP, ceilXP, minutes
		);
		return ScrServerCmdResult(msg, EServerCmdResultType.OK);
	}
}
