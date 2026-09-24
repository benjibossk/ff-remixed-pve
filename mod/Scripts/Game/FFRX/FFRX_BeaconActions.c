// FF - REMIXED - PVE
// BALISE GPS -- le marqueur sur la carte, l'action d'inventaire, et le pont client->serveur.
// Le coeur (batterie, registre, tic) est dans FFRX_Beacon.c.
//
// ======================================================================================
//  LE MARQUEUR : UNE POSITION TRANSMISE, PAS UN SUIVI
// ======================================================================================
// Premiere version : marqueur DYNAMIQUE accroche a l'entite, qui la suivait tout seul.
// Abandonne le 21/09 a la demande de Benji -- "le signal saccade plus il s'eloigne de la
// portee radio de la base". Un marqueur qui colle a l'objet ne peut pas saccader : il n'y a
// aucun moment ou l'on pourrait degrader quoi que ce soit.
//
// On pose donc un marqueur STATIQUE dont le SERVEUR repousse la position a intervalle
// variable (cf. FFRX_BeaconSignal) : rapide et exact pres d'un relais, rare et approximatif
// loin de tout, fige hors de portee. Le marqueur devient une DERNIERE POSITION CONNUE, ce
// qui est a la fois plus juste et plus interessant a jouer.
//
// Effet de bord heureux : on se debarrasse d'`InsertDynamicMarker`, qui n'avait jamais ete
// essaye dans ce mod. Les marqueurs statiques, eux, sont le chemin deja eprouve par les
// champs de mines (FFRX_ProcurementShop.FFRX_RpcOwner_MarkMinefield).
//
// Il est pose par CHAQUE client, en local, sur ordre du serveur. On ne compte pas sur la
// replication native des marqueurs : notre carte est deliberement videe pour les non-admins
// (FFRX_EmptyMap), et on veut maitriser exactement ce qui s'affiche.
//
// LE LIBELLE porte le pourcentage de batterie et l'etat du signal ("BALISE 87 pct - signal
// faible"), via `SetCustomText`. C'est un libelle PERMANENT, pas une infobulle au survol :
// le moteur n'expose pas de tooltip sur ces marqueurs, et un chiffre toujours visible sert
// de toute facon mieux une carte tactique qu'une valeur qu'il faut aller chercher.
// A noter : `SetCustomText` ne declenche PAS le filtre de grossieretes (il faut un appel
// explicite a `RequestProfanityFilter`), donc notre texte passe intact.
//
// ======================================================================================
//  POURQUOI UN RPC
// ======================================================================================
// `SCR_InventoryAction.PerformAction` s'execute chez le CLIENT. Y basculer l'etat ne ferait
// rien sur le dedie : l'autorite ne verrait jamais le changement. C'est exactement le bug
// deja paye avec l'action "Prendre dotation" (memoire `dedie-useraction-canperform-client-gate`).
// Le mod drones fait le tour de la meme facon (`SCR_PlayerController.ToggleJammer`).
//
// Ce fichier porte donc SON PROPRE `modded class SCR_PlayerController`, et c'est volontaire :
// contrairement a ce que disaient plusieurs en-tetes du mod, Enforce accepte PLUSIEURS
// `modded class` de la meme classe dans un meme addon -- ils s'enchainent (mesure du
// 2026-09-21 : 6 blocs coexistent, dont 2 sur la meme methode, 0 erreur). Inutile donc
// d'entasser ce code dans un fichier sans rapport.
//
// Chaines ASCII (le dedie compile en strict).

// ---------------------------------------------------------------------------
//  Marqueur
// ---------------------------------------------------------------------------
class FFRX_BeaconMarker
{
	//! Marqueurs poses localement, par entite balise. Sert a les retirer a l'extinction.
	//!
	//! Indexe par identifiant reseau de la balise -- et non par entite : le client n'a pas
	//! forcement l'entite sous la main (une balise rangee dans un sac n'est pas streamee), or
	//! il doit quand meme pouvoir deplacer et retirer son marqueur.
	protected static ref map<int, ref SCR_MapMarkerBase> s_mMarkers;

	//------------------------------------------------------------------------------------------------
	//! Identifiant reseau d'une balise, ou 0.
	protected static int IdOf(IEntity beacon)
	{
		if (!beacon)
			return 0;

		RplComponent rpl = RplComponent.Cast(beacon.FindComponent(RplComponent));
		if (!rpl)
			return 0;

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	//! SERVEUR : allumage / extinction. Seule l'extinction passe par ici ; l'allumage se
	//! contente d'amorcer, la premiere position partant au tic suivant (FFRX_Beacons.Tick).
	static void Broadcast(IEntity beacon, bool show)
	{
		int id = IdOf(beacon);
		if (id == 0)
			return;

		if (show)
			return; // rien a afficher tant qu'aucune position n'a ete transmise

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.FFRX_BeaconBroadcastHide(id);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVEUR : transmet une position (deja bruitee par le signal) + l'etat de la balise.
	static void BroadcastPos(IEntity beacon, vector pos, int batteryPct, int signalLevel)
	{
		int id = IdOf(beacon);
		if (id == 0)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.FFRX_BeaconBroadcastPos(id, pos, batteryPct, signalLevel);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT : pose le marqueur s'il n'existe pas, sinon le DEPLACE. Pas de retrait/reinsertion :
	//! ca ferait clignoter l'icone a chaque rafraichissement.
	static void ApplyPos(int id, vector pos, int batteryPct, int signalLevel)
	{
		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!mgr)
			return;

		if (!s_mMarkers)
			s_mMarkers = new map<int, ref SCR_MapMarkerBase>();

		string label = "BALISE " + batteryPct.ToString() + " pct"
			+ FFRX_BeaconSignal.Label(signalLevel);

		SCR_MapMarkerBase m;
		if (s_mMarkers.Contains(id))
			m = s_mMarkers.Get(id);

		if (!m)
		{
			// On CONSTRUIT le marqueur nous-memes plutot que d'appeler
			// `InsertStaticMarkerByType`, qui ne rend rien : il faudrait alors deviner que le
			// notre est "le dernier de la liste", ce qui est faux des qu'un joueur pose un
			// marqueur dans le meme intervalle. `InsertStaticMarker` accepte un marqueur deja
			// fait, donc on en garde la reference -- c'est elle qui permettra de le DEPLACER
			// au lieu de le detruire et le recreer a chaque rafraichissement.
			m = new SCR_MapMarkerBase();
			m.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
			m.SetWorldPos(pos[0], pos[2]);

			// isLocal = true : chaque client pose le sien. Cf. l'en-tete -- notre carte est
			// videe pour les non-admins, on maitrise donc nous-memes ce qui s'affiche.
			mgr.InsertStaticMarker(m, true);
			s_mMarkers.Set(id, m);
		}

		m.SetWorldPos(pos[0], pos[2]);
		m.SetCustomText(label);
	}

	//------------------------------------------------------------------------------------------------
	//! CLIENT : retire le marqueur.
	static void ApplyHide(int id)
	{
		if (!s_mMarkers || !s_mMarkers.Contains(id))
			return;

		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		SCR_MapMarkerBase m = s_mMarkers.Get(id);

		if (mgr && m)
			mgr.RemoveStaticMarker(m);

		s_mMarkers.Remove(id);
	}
}

// ---------------------------------------------------------------------------
//  Pont client -> serveur
// ---------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! CLIENT -> SERVEUR : le joueur a clique "allumer/eteindre" sur une balise.
	void FFRX_BeaconAskToggle(RplId beaconId)
	{
		Rpc(FFRX_RpcServer_BeaconToggle, beaconId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void FFRX_RpcServer_BeaconToggle(RplId beaconId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(beaconId));
		if (!rpl)
			return;

		IEntity beacon = rpl.GetEntity();
		if (!beacon)
			return;

		string msg = FFRX_Beacons.Toggle(beacon);
		if (msg == "")
			return;

		// `FFRX_SendIntelHint` vit sur JWK_PlayerControllerComponent, PAS sur
		// SCR_PlayerController -- on passe donc par le composant FF, comme FFRX_IntelSystem.
		JWK_PlayerControllerComponent jpc = JWK.GetPlayerController(GetPlayerId());
		if (jpc)
			jpc.FFRX_SendIntelHint(msg);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVEUR -> TOUS : nouvelle position transmise par une balise.
	void FFRX_BeaconBroadcastPos(int beaconId, vector pos, int batteryPct, int signalLevel)
	{
		Rpc(FFRX_RpcBroadcast_BeaconPos, beaconId, pos, batteryPct, signalLevel);

		// Le serveur ecoute est aussi un client (cas du Workbench) : il ne recoit pas son
		// propre broadcast, donc on applique localement en plus.
		FFRX_BeaconMarker.ApplyPos(beaconId, pos, batteryPct, signalLevel);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void FFRX_RpcBroadcast_BeaconPos(int beaconId, vector pos, int batteryPct, int signalLevel)
	{
		FFRX_BeaconMarker.ApplyPos(beaconId, pos, batteryPct, signalLevel);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVEUR -> TOUS : la balise s'eteint, le marqueur disparait.
	void FFRX_BeaconBroadcastHide(int beaconId)
	{
		Rpc(FFRX_RpcBroadcast_BeaconHide, beaconId);
		FFRX_BeaconMarker.ApplyHide(beaconId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void FFRX_RpcBroadcast_BeaconHide(int beaconId)
	{
		FFRX_BeaconMarker.ApplyHide(beaconId);
	}
}

// ---------------------------------------------------------------------------
//  Action d'inventaire : allumer / eteindre
// ---------------------------------------------------------------------------
// Le libelle est DYNAMIQUE : il porte l'etat et le niveau de batterie. Il se calcule dans
// `CanBePerformedScript` (rejoue a chaque ouverture du menu) et se rend dans
// `GetActionNameScript` -- c'est le tour de passe-passe de SAL_ToggleJammerCharacter.
class FFRX_BeaconToggle : SCR_InventoryAction
{
	protected IEntity m_eBeacon;
	protected string  m_sLabel = "Balise";

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_eBeacon = pOwnerEntity;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return FFRX_Beacons.Find(m_eBeacon) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		FFRX_BeaconComponent c = FFRX_Beacons.Find(m_eBeacon);
		if (!c)
			return false;

		int pct = c.FFRX_BatteryPct();

		if (c.FFRX_IsOn())
			m_sLabel = string.Format("Balise : eteindre (%1 pct)", pct);
		else if (c.FFRX_IsFlat())
			m_sLabel = "Balise : a plat -- recharge-la";
		else
			m_sLabel = string.Format("Balise : allumer (%1 pct)", pct);

		// A plat, l'action reste VISIBLE mais inerte : le joueur doit comprendre pourquoi
		// rien ne se passe. Une action qui disparait ne dit rien.
		return !c.FFRX_IsFlat();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = m_sLabel;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠️ Tourne chez le CLIENT. On ne bascule rien ici -- on demande au serveur (cf. en-tete).
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity)
			return;

		RplComponent rpl = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		if (!rpl)
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
			return;

		pc.FFRX_BeaconAskToggle(rpl.Id());
	}
}

// ---------------------------------------------------------------------------
//  Action d'inventaire : recharger avec une cellule
// ---------------------------------------------------------------------------
// Doublon volontaire du glisser-deposer (FFRX_JammerBatteryDrop.c) : les deux gestes
// marchent, comme pour le jammer.
class FFRX_BeaconRecharge : SCR_InventoryAction
{
	protected IEntity m_eBeacon;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_eBeacon = pOwnerEntity;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return FFRX_Beacons.Find(m_eBeacon) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Visible seulement si le joueur porte une cellule compatible.
	override bool CanBePerformedScript(IEntity user)
	{
		return FFRX_BeaconFindCell(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		int pct = 100;
		FFRX_BeaconComponent c = FFRX_Beacons.Find(m_eBeacon);
		if (c)
			pct = c.FFRX_BatteryPct();

		outName = string.Format("Recharger la balise (%1 pct)", pct);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !pUserEntity)
			return;

		// Autorite serveur : consommer la cellule chez le client la detruirait localement
		// avant d'etre resynchronisee -- donc une recharge dans le vide.
		if (!Replication.IsServer())
			return;

		FFRX_BeaconComponent c = FFRX_Beacons.Find(pOwnerEntity);
		if (!c)
			return;

		IEntity cell = FFRX_BeaconFindCell(pUserEntity);
		if (!cell)
			return;

		float cap = 0;
		SAL_BatteryComponent bc = SAL_BatteryComponent.Cast(cell.FindComponent(SAL_BatteryComponent));
		if (bc)
			cap = bc.m_fBatteryStorage;

		c.FFRX_Recharge(cap);
		SCR_EntityHelper.DeleteEntityAndChildren(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! Premiere cellule SAL trouvee sur le joueur.
	protected IEntity FFRX_BeaconFindCell(IEntity user)
	{
		if (!user)
			return null;

		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(
			user.FindComponent(InventoryStorageManagerComponent));
		if (!inv)
			return null;

		array<IEntity> items = {};
		inv.GetItems(items);

		foreach (IEntity it : items)
		{
			if (!it)
				continue;

			if (it.FindComponent(SAL_BatteryComponent))
				return it;
		}

		return null;
	}
}

// ---------------------------------------------------------------------------
//  #balise -- etat
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_BeaconCommand : ScrServerCommand
{
	override string GetKeyword() { return "balise"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return Report();
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return Report();
	}

	protected ScrServerCmdResult Report()
	{
		return ScrServerCmdResult(
			"Balises allumees : " + FFRX_Beacons.LitCount().ToString()
			+ " (autonomie a plein : 2 h, recharge par cellule de drone).",
			EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
