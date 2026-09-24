// FF - REMIXED - PVE
// Chat command "#build": an ADMIN or an etat-major (command squad) member opens FF's
// construction menu WITHOUT needing a shovel in hand. Everyone else builds the
// normal way (hold the construction tool, use the radial menu) -- that path is
// untouched. Server-side, same etat-major detection as #demandes / the vehicle
// procurement gate (FFRX_GroupsManager.IsEtatMajor), so no client replication.
//
// Usage in-game chat:  #build
[BaseContainerProps()]
class FFRX_BuildCommand : ScrServerCommand
{
	override string GetKeyword() { return "build"; }
	override bool IsServerSide() { return true; }
	//! Ouverte a TOUS : le filtrage reel se fait ci-dessous, cote serveur. Un joueur qui
	//! porte une pelle valide doit pouvoir construire meme si la roue radiale ne s'ouvre
	//! pas -- c'est le contournement du bug "J ne fait rien" (2026-09-10), et c'est aussi
	//! plus juste sur le fond : l'outil EST la licence de construire.
	override int RequiredChatPermission() { return EPlayerRole.NONE; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		// QUI OUVRE LE MENU DE CONSTRUCTION (modele arrete par Benji, 2026-09-24) :
		//   - le COMMANDEMENT (etat-major) ;
		//   - le GENIE ;
		//   - un ADMIN connecte, qui pilote le serveur.
		//
		// Tous les autres peuvent seulement AIDER a monter un batiment deja pose -- ils
		// pellettent sur un chantier existant, ils ne choisissent pas quoi construire ni ou.
		//
		// ⚠️ DEUX VOIES ONT ETE RETIREES ICI, ne pas les reintroduire sans raison :
		//
		// 1. "PELLE EN MAIN" (ajoutee le 10/09). Elle laissait N'IMPORTE QUI ouvrir le menu
		//    complet : il suffisait de sortir une pelle. C'etait un contournement assume tant
		//    que la touche J etait cassee -- elle ne l'est plus depuis le 19/09, et cette voie
		//    vidait le modele de son sens.
		//
		// 2. "DROIT ACCORDE DEPUIS LE SITE" (ajoutee le 23/09, page /grants). Abandonnee le
		//    lendemain : elle repondait a un besoin qui n'existe pas. Le commandement et le
		//    genie sont deja les bons destinataires, et donner le menu a quelqu'un d'autre
		//    casserait justement la repartition des roles qu'on cherche a tenir.
		//
		// SCR_Global.IsAdmin couvre ADMINISTRATOR *et* SESSION_ADMINISTRATOR : un admin
		// connecte en cours de partie (mot de passe admin) est reconnu, pas seulement celui
		// declare dans la config du serveur.
		//
		// Verification cote SERVEUR uniquement : le client ne decide de rien (cf. memoire
		// dedie-useraction-canperform-client-gate, ou un gate client-only marchait en
		// Workbench et ne faisait rien sur le dedie).
		// ⚠️ `FFRX_GroupsManager.IsGenie(playerId)` et NON `FFRX_BuildDirect.InEngineerSquad()` :
		// cette derniere lit le joueur LOCAL (`GetLocalPlayerId`), donc elle est cote client.
		// L'appeler ici marcherait en Workbench et ne ferait RIEN sur le dedie -- exactement
		// le piege de la memoire `dedie-useraction-canperform-client-gate`. Les deux fonctions
		// repondent a la meme question, mais une seule le fait pour un joueur DONNE.
		if (!SCR_Global.IsAdmin(playerId) && !FFRX_GroupsManager.IsEtatMajor(playerId) && !FFRX_GroupsManager.IsGenie(playerId))
			return ScrServerCmdResult("Menu de construction reserve au commandement et au genie. Tu peux aider a monter un batiment deja pose avec une pelle.", EServerCmdResultType.ERR);

		JWK_PlayerControllerComponent jpc = JWK.GetPlayerController(playerId);
		if (!jpc)
			return ScrServerCmdResult("Controller introuvable.", EServerCmdResultType.ERR);

		jpc.FFRX_OpenBuild();
		return ScrServerCmdResult("Menu construction ouvert.", EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	//! Le joueur tient-il un outil de construction reconnu par FF, en main gauche ?
	//! Meme test que l'entree radiale de FF, reutilise depuis FFRX_BuildDiag (#pelle) pour
	//! qu'il n'existe qu'UNE definition de "outil valide" dans le mod.
	protected bool FFRX_HoldsBuildTool(int playerId)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;

		IEntity player = pm.GetPlayerControlledEntity(playerId);
		if (!player)
			return false;

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(player.FindComponent(SCR_CharacterControllerComponent));
		if (!cc)
			return false;

		return FFRX_BuildDiagUtil.IsTool(cc.GetAttachedGadgetAtLeftHandSlot());
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Commande joueur uniquement (#build en jeu).", EServerCmdResultType.OK);
	}
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
