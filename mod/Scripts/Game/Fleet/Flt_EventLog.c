// Fleet — Journal admin : connexions, departs et chat remontes au site (POST /events/log).
//
// POURQUOI ICI ET PAS VIA UN AUTRE MOD : le mod "ReforgerJS Support" produit deja ces
// evenements, mais il les ecrit dans des fichiers JSON sur la machine du serveur -- il
// faudrait un agent externe pour les relire, et tout casserait si ce mod evolue ou est
// retire. Fleet parle deja au site (cle API, RestApi, file de commandes) : c'est le bon
// endroit. A terme cela permet de se passer de RJS.
//
// CREDIT : le decoupage des evenements (CONNECT/DISCONNECT + plateforme du joueur) vient
// de "ReforgerJS Support" (685448E58B08F021), et l'astuce du relais RPC client->serveur
// pour capter le chat vient de "GM Tools" -- leurs auteurs autorisent la reprise. Le code
// ci-dessous est le notre.
//
// Le DEPART est branche dans Flt_RankSystem.c (SCR_BaseGameMode.OnPlayerDisconnected y est
// deja surcharge : une seule surcharge par methode et par addon).

class Flt_EventLog
{
	static const string TYPE_CONNECT    = "CONNECT";
	static const string TYPE_DISCONNECT = "DISCONNECT";
	static const string TYPE_CHAT       = "CHAT";

	//------------------------------------------------------------------------------------------------
	//! Identite d'un joueur cote serveur : UID backend, nom, plateforme.
	//! La plateforme est la raison d'etre de ce journal cote admin : elle dit si un testeur
	//! console arrive reellement a entrer sur le serveur.
	static void Flt_Identity(int playerId, out string uid, out string name, out string platform)
	{
		uid = "";
		name = "";
		platform = "";

		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
			name = pm.GetPlayerName(playerId);

		BackendApi ba = GetGame().GetBackendApi();
		if (ba)
			uid = ba.GetPlayerIdentityId(playerId);

		if (name == "")
			name = string.Format("Joueur %1", playerId);

		platform = Flt_Platform(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! "platform-windows" / "platform-xbl" / "platform-psn". Chaine vide si indisponible :
	//! l'API differe selon les versions du jeu, donc on echoue en douceur plutot que de
	//! risquer une exception a chaque connexion.
	static string Flt_Platform(int playerId)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return "";

		PlayerController pc = pm.GetPlayerController(playerId);
		if (!pc)
			return "";

		return "";   // renseigne plus tard si une API stable est disponible
	}

	//------------------------------------------------------------------------------------------------
	static void Flt_Send(string type, int playerId, string message)
	{
		if (!Replication.IsServer())
			return;

		Flt_GTGPositions gtg = Flt_GTGPositions.GetInstance();
		if (!gtg)
			return;

		string uid, name, platform;
		Flt_Identity(playerId, uid, name, platform);

		gtg.Flt_PostEvent(type, uid, name, platform, "", message);
	}

	//------------------------------------------------------------------------------------------------
	//! Message du site affiche a TOUS les joueurs (ordre "say" de la file /commands).
	//! Cote serveur on ne peut pas afficher directement : le popup est une entite CLIENT.
	//! On passe donc par le SCR_ChatComponent de chaque PlayerController (repliqué par joueur),
	//! avec un RPC vers le proprietaire.
	static void Flt_Broadcast(string message)
	{
		if (!Replication.IsServer() || message == "")
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);

		foreach (int id : ids)
		{
			PlayerController pc = pm.GetPlayerController(id);
			if (!pc)
				continue;

			SCR_ChatComponent chat = SCR_ChatComponent.Cast(pc.FindComponent(SCR_ChatComponent));
			if (chat)
				chat.Flt_SendPopupTo(message);
		}

		// Trace dans le journal : on veut savoir QUI a parle depuis le site et quand.
		Flt_GTGPositions gtg = Flt_GTGPositions.GetInstance();
		if (gtg)
			gtg.Flt_PostEvent("ADMIN", "", "Commandement", "web", "", message);

		Print(string.Format("[Flt_EventLog] Message du site diffuse : %1", message), LogLevel.NORMAL);
	}
}

//------------------------------------------------------------------------------------------------
//! Arrivee d'un joueur. OnPlayerDisconnected n'est PAS surcharge ici : il l'est deja dans
//! Flt_RankSystem.c, qui appelle Flt_EventLog.Flt_Send pour le depart.
modded class SCR_BaseGameMode
{
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		if (!Replication.IsServer())
			return;

		Flt_EventLog.Flt_Send(Flt_EventLog.TYPE_CONNECT, playerId, "");
	}
}

//------------------------------------------------------------------------------------------------
//! Chat. OnNewMessage ne se declenche que cote CLIENT : chaque client relaie donc SON propre
//! message au serveur par RPC, qui seul possede la cle API et fait le POST. Sans ce relais, le
//! serveur dedie ne voit jamais passer le chat.
modded class SCR_ChatComponent
{
	override void OnNewMessage(string msg, int channelId, int senderId)
	{
		super.OnNewMessage(msg, channelId, senderId);

		if (msg == "" || senderId <= 0)
			return;

		PlayerController local = GetGame().GetPlayerController();
		if (!local)
			return;

		// Seul l'auteur relaie, sinon le serveur recevrait autant de copies que de joueurs
		// ayant recu le message.
		if (local.GetPlayerId() != senderId)
			return;

		Rpc(Flt_RpcSrv_Chat, msg, channelId, senderId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void Flt_RpcSrv_Chat(string msg, int channelId, int senderId)
	{
		if (!Replication.IsServer())
			return;

		Flt_EventLog.Flt_Send(Flt_EventLog.TYPE_CHAT, senderId, msg);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur -> proprietaire de ce controller : affiche un message du commandement.
	void Flt_SendPopupTo(string message)
	{
		Rpc(Flt_RpcOwner_Popup, message);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void Flt_RpcOwner_Popup(string message)
	{
		SCR_PopUpNotification popup = SCR_PopUpNotification.GetInstance();
		if (popup)
			popup.PopupMsg(message, 8.0, "COMMANDEMENT");
	}
}
