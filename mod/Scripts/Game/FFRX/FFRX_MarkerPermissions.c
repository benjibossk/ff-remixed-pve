// FF - REMIXED - PVE
// Droits d'edition des marqueurs, par hierarchie militaire.
//
//   - chacun modifie SES marqueurs ;
//   - un CHEF D'ESCOUADE modifie ceux des joueurs de SON escouade ;
//   - le groupe KILO COMMANDEMENT modifie TOUT ;
//   - le Game Master garde ses droits (on ne touche pas a sa voie).
//
// ------------------------------------------------------------------------------------
// CE QUE FAISAIT ANARCHY MARKERS, ET POURQUOI ON RESTREINT
//
// Sa regle (`SM_MarkerNet.CanModify`) juge sur la VISIBILITE : un marqueur "side" etant
// collectif, quiconque peut le VOIR peut le MODIFIER. Concretement, n'importe quel joueur
// pouvait deplacer ou effacer le marqueur de n'importe qui.
//
// Pour un serveur milsim c'est l'inverse de ce qu'on veut : un chef doit pouvoir corriger
// le marqueur mal place d'un de ses hommes, mais un soldat d'une autre escouade n'a rien
// a y faire. On DURCIT donc la regle au lieu de l'assouplir.
//
// ------------------------------------------------------------------------------------
// OU ON SE BRANCHE, ET POURQUOI PAS AILLEURS
//
// `CanModify` serait le point naturel, mais c'est une methode STATIQUE de `SM_MarkerNet` :
// les statiques ne sont pas virtuelles, on ne peut pas les surcharger proprement.
//
// En revanche les RPC d'edition (`RpcAsk_Edit`, `RpcAsk_Remove`, `RpcAsk_Move`) sont des
// methodes D'INSTANCE declarees dans `modded class SCR_PlayerController`. On les
// intercepte donc : on applique notre regle, et on ne delegue a Anarchy que si elle passe.
//
// ⚠️ IMPORTANT -- c'est bien le SERVEUR qui tranche. Verifie dans le code d'Anarchy :
// `RpcAsk_Edit` -> `ServerHandleEdit` -> `CanModify`, avec un `if (!Replication.IsServer())
// return;` en garde. Un controle uniquement cote client serait contournable (cf. memoire
// dedie-useraction-canperform-client-gate).
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------
	//! Le joueur `requesterId` a-t-il le droit de toucher au marqueur `markerId` ?
	protected bool FFRX_CanEditMarker(int requesterId, int markerId)
	{
		if (requesterId <= 0)
			return false;

		SM_MapMarkerStore store = SM_MapMarkerStore.GetInstance();
		if (!store)
			return true;                       // magasin indisponible -> on ne bloque pas

		SM_MapMarkerData m = store.FindById(markerId);
		if (!m)
			return true;                       // marqueur inconnu -> laisser Anarchy repondre

		// Marqueur pose par le SERVEUR (owner -1) : reserve au commandement.
		if (m.m_iOwnerId < 0)
			return FFRX_IsCommand(requesterId);

		// Son propre marqueur : toujours.
		if (m.m_iOwnerId == requesterId)
			return true;

		// Commandement (KILO) : tout.
		if (FFRX_IsCommand(requesterId))
			return true;

		// Chef d'escouade : les marqueurs des joueurs de SON escouade.
		if (FFRX_IsSquadLeaderOf(requesterId, m.m_iOwnerId))
			return true;

		return false;
	}

	//! Membre du groupe de commandement (KILO). On reutilise la detection deja employee
	//! par #demandes et le procurement vehicule : une seule definition de l'etat-major.
	protected bool FFRX_IsCommand(int playerId)
	{
		return FFRX_GroupsManager.IsEtatMajor(playerId);
	}

	//! `leaderId` est-il le chef de l'escouade a laquelle appartient `memberId` ?
	protected bool FFRX_IsSquadLeaderOf(int leaderId, int memberId)
	{
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm)
			return false;

		SCR_AIGroup leaderGroup = gm.GetPlayerGroup(leaderId);
		if (!leaderGroup)
			return false;

		// Chef de SON groupe, et le proprietaire du marqueur est dans CE groupe.
		if (leaderGroup.GetLeaderID() != leaderId)
			return false;

		SCR_AIGroup ownerGroup = gm.GetPlayerGroup(memberId);
		return ownerGroup == leaderGroup;
	}

	//! Message de refus, sinon le marqueur "revient en place" tout seul et le joueur
	//! croit a un bug. Anarchy a deja ce canal, on le reutilise.
	protected void FFRX_DenyMarker()
	{
		SM_SendPlaceDenied(SM_EPlaceDenyReason.MARKER_NO_ACCESS, 0);
	}

	//------------------------------------------------------------------------------------
	//  Interception des trois voies d'edition
	//------------------------------------------------------------------------------------
	override protected void RpcAsk_Edit(int id, array<int> packed, string text, string text2)
	{
		if (Replication.IsServer() && !FFRX_CanEditMarker(GetPlayerId(), id))
		{
			FFRX_DenyMarker();
			return;
		}
		super.RpcAsk_Edit(id, packed, text, text2);
	}

	override protected void RpcAsk_Remove(int id)
	{
		if (Replication.IsServer() && !FFRX_CanEditMarker(GetPlayerId(), id))
		{
			FFRX_DenyMarker();
			return;
		}
		super.RpcAsk_Remove(id);
	}

	override protected void RpcAsk_Move(int id, int posX, int posY)
	{
		if (Replication.IsServer() && !FFRX_CanEditMarker(GetPlayerId(), id))
		{
			FFRX_DenyMarker();
			return;
		}
		super.RpcAsk_Move(id, posX, posY);
	}
}
