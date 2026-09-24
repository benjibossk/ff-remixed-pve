// FF - REMIXED - PVE
// Chat command "#demandes": an etat-major member opens the procurement
// validation ON DEMAND (instead of an intrusive auto-popup). Shows the oldest
// pending request's Accepter/Refuser dialog; type it again for the next one.
//
// Usage in-game chat:  #demandes
[BaseContainerProps()]
class FFRX_DemandesCommand : ScrServerCommand
{
	override string GetKeyword() { return "demandes"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		if (!FFRX_GroupsManager.IsEtatMajor(playerId))
			return ScrServerCmdResult("Reserve a l'etat-major.", EServerCmdResultType.ERR);

		array<FFRX_ProcRequest> reqs = {};
		FFRX_Procurement.GetPending(reqs);
		if (reqs.IsEmpty())
			return ScrServerCmdResult("Aucune demande en attente.", EServerCmdResultType.OK);

		JWK_PlayerControllerComponent jpc = JWK.GetPlayerController(playerId);
		if (!jpc)
			return ScrServerCmdResult("Controller introuvable.", EServerCmdResultType.ERR);

		// Oldest pending request first.
		FFRX_ProcRequest r = reqs[0];
		jpc.FFRX_ProcDialog(r.id, FFRX_Procurement.DescribeRequest(r));

		return ScrServerCmdResult(
			string.Format("%1 demande(s) en attente.", reqs.Count()),
			EServerCmdResultType.OK
		);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Commande joueur uniquement (#demandes en jeu).", EServerCmdResultType.OK);
	}
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
