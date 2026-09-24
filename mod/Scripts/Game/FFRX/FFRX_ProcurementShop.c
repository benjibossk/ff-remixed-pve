// FF - REMIXED - PVE
// Procurement gate for the VEHICLE SHOP / import path (the one players actually
// use to buy/import a vehicle). Complements the garage gate in FFRX_GarageMarker.
//
// Player shop buy -> JWK_PlayerControllerComponent.RpcAsk_ShopBuy (server) ->
// shop.PerformPlayerItemPurchase_S. We intercept RpcAsk_ShopBuy: if the item is a
// vehicle AND the buyer is not etat-major, queue a request instead of buying.
// Non-vehicle items (weapons, ammo...) are unaffected.
modded class JWK_PlayerControllerComponent
{
	override protected void RpcAsk_ShopBuy(RplId shopId, ResourceName resource)
	{
		int pid = GetOwnerPlayerId();

		if (JWK_VehicleUtils.IsVehiclePrefab(resource) && !FFRX_GroupsManager.IsEtatMajor(pid)) {
			FFRX_Procurement.CreateShopRequest(pid, shopId, resource);
			return;
		}

		super.RpcAsk_ShopBuy(shopId, resource);
	}

	// Server -> owner client: show a feedback message in FF's own hint/feedback
	// style (same UI as "can't do in hostile territory" when importing on a zone
	// you don't own). Arbitrary text via a custom JWK_FeedbackUIInfo, no config.
	void FFRX_Popup(string text)
	{
		Rpc(FFRX_RpcOwner_Popup, text);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcOwner_Popup(string text)
	{
		JWK_HintManagerComponent hint = JWK.GetHint();
		if (!hint) return;

		JWK_FeedbackUIInfo info = new JWK_FeedbackUIInfo();
		info.SetDescription(text);
		hint.ShowFeedback(info, false, true); // ignoreShown = true -> always display
	}

	// --- Procurement validation dialog (etat-major) ---

	// Server -> owner client: pop the Accepter/Refuser confirmation dialog.
	void FFRX_ProcDialog(int reqId, string text)
	{
		Rpc(FFRX_RpcOwner_ProcDialog, reqId, text);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcOwner_ProcDialog(int reqId, string text)
	{
		FFRX_ProcDialogClient.Show(this, reqId, text);
	}

	// Owner client -> server: the etat-major member's answer.
	void FFRX_ProcRespond(int reqId, bool approve)
	{
		Rpc(FFRX_RpcServer_ProcRespond, reqId, approve);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void FFRX_RpcServer_ProcRespond(int reqId, bool approve)
	{
		int pid = GetOwnerPlayerId();
		if (approve)
			FFRX_Procurement.Approve(reqId, pid);
		else
			FFRX_Procurement.Deny(reqId, pid);
	}

	// --- #build command: open the construction menu on this player's client ---
	// Server (the #build command) validates etat-major, then asks the owner client to
	// open FF's construction asset-selection menu directly -- WITHOUT requiring a
	// shovel in hand. Non-etat-major players are unaffected (they still build the
	// normal way, with the shovel). See FFRX_BuildCommand.
	void FFRX_OpenBuild()
	{
		Rpc(FFRX_RpcOwner_OpenBuild);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcOwner_OpenBuild()
	{
		// Mirrors JWK_AssetSelectionMainMenuController.OnPerformed for construction.
		// The handler self-initialises from the LOCAL controlled entity + build area.
		JWK_ConstructionSelectionMenuHandler handler = new JWK_ConstructionSelectionMenuHandler();
		JWK_UIContextTU<JWK_AssetSelectionMenuContext>.Get().SetHandler(handler);
		JWK.GetUI().OpenContext(JWK_AssetSelectionMenuContext);
	}

	// --- Intel hint (folded from MCD) ---
	// Server -> owner client: show a text hint (grid coords) in FF's feedback style.
	// Text-only, no map marker (the player reads/remembers the numbers).
	void FFRX_SendIntelHint(string text)
	{
		Rpc(FFRX_RpcOwner_IntelHint, text);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcOwner_IntelHint(string text)
	{
		if (text == "") return;
		JWK_HintManagerComponent hint = JWK.GetHint();
		if (!hint) return;

		JWK_FeedbackUIInfo info = new JWK_FeedbackUIInfo();
		info.SetDescription(text);
		hint.ShowFeedback(info, false, true);
	}

	// --- Minefield marker (intel: reveal an enemy minefield as a danger zone) ---
	// Server -> owner client: drop a LOCAL map marker (only this player sees it), same
	// mechanism as the garage marker. A minefield must be VISIBLE to be avoided / cleared.
	void FFRX_MarkMinefield(vector pos)
	{
		Rpc(FFRX_RpcOwner_MarkMinefield, pos);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcOwner_MarkMinefield(vector pos)
	{
		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!mgr) return;
		// isLocal = true -> only this player sees it (drawn by base-game SCR_MapMarkersUI,
		// which the empty-map override does not hide).
		mgr.InsertStaticMarkerByType(SCR_EMapMarkerType.PLACED_CUSTOM, pos[0], pos[2], true);
	}

	// --- Auto-spawn (no spawn-selection map) ---
	// FF opens the spawn map via JWK.GetPlayerController(id).OpenSpawnSelectionMenu().
	// We intercept THAT (on the controller we already mod) and deploy to the best base
	// instead -- deliberately NOT modding JWK_RespawnSystemComponent, because modding
	// that class re-triggers FF's EPF_CharacterSaveData static poison in a large module
	// (DARC deps). FF already did the bookkeeping (persistence id, null control) before
	// calling this, so we just pick + deploy. First spawn = instant; after-death = a
	// short "dead" delay so death isn't instant. Priority MOB > FOB/Camp > Town.
	static const int FFRX_DEATH_DEPLOY_DELAY_MS = 12000;
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	static ref set<int> s_aDeployedOnce;

	static set<int> DeployedOnce()
	{
		if (!s_aDeployedOnce)
			s_aDeployedOnce = new set<int>();

		return s_aDeployedOnce;
	}

	override void OpenSpawnSelectionMenu()
	{
		int playerId = GetOwnerPlayerId();
		JWK_PlayerRespawnLocationComponent best = FFRX_PickBestSpawn(playerId);

		// First spawn vs death respawn. OpenSpawnSelectionMenu is NOT called for the
		// initial join (that deploys via auto-spawn), so the old DeployedOnce() set
		// was empty on the FIRST death -> it was misread as a first spawn -> instant
		// deploy -> the death screen never had time to show. Use FF's authoritative
		// respawn counter instead: GetRespawns_S() == 0 is a genuine first spawn
		// (instant), > 0 means the player has spawned before -> this is a death (hold
		// the delay so the death screen is visible).
		int respawns = -1;
		JWK_PlayerProfileComponent prof = JWK.GetPlayerProfile(playerId);
		if (prof)
			respawns = prof.GetRespawns_S();
		bool isDeath = respawns > 0;

		Print(string.Format("[FFRX][Spawn] OpenSpawnSelectionMenu: pid=%1 best=%2 respawns=%3 isDeath=%4",
			playerId, best != null, respawns, isDeath));
		if (best)
		{
			if (isDeath)
			{
				Print("[FFRX][Spawn] -> DEATH: deferring deploy 12s (death screen)");
				GetGame().GetCallqueue().CallLater(FFRX_DeferredDeploy, FFRX_DEATH_DEPLOY_DELAY_MS, false, playerId);
			}
			else
			{
				Print("[FFRX][Spawn] -> FIRST spawn: deploy now");
				DeployedOnce().Insert(playerId);
				FFRX_DeployNow(playerId, best);
			}
			return;
		}

		// Aucune base utilisable POUR L'INSTANT -- ce n'est pas forcement definitif.
		//
		// Sur une sauvegarde neuve, FFRX_DefaultFob place la FOB de depart en differe
		// (CallLater 3 s, puis relances toutes les 3 s tant que l'index territorial
		// n'est pas pret). Le premier joueur qui se connecte arrive donc AVANT elle :
		// PickBestSpawn renvoie null, on basculait sur la carte vanilla... qui est vide
		// puisqu'aucune base n'existe encore. Et quand la FOB apparaissait, plus rien ne
		// relancait le spawn -> le joueur restait coince et devait se reconnecter.
		//
		// On reessaie donc, exactement comme le fait deja le cas "mort" ci-dessous.
		// La carte vanilla reste le repli, mais seulement apres FFRX_SPAWN_MAX_RETRIES :
		// si aucune base n'apparait vraiment (toutes perdues), il faut bien rendre la
		// main au joueur plutot que de le laisser attendre indefiniment.
		Print("[FFRX][Spawn] -> no usable base yet -> retry while the starting FOB spawns");
		SpawnRetries().Set(playerId, 0);
		GetGame().GetCallqueue().CallLater(FFRX_DeferredDeploy, FFRX_SPAWN_RETRY_MS, false, playerId);
	}

	// Attente max ~30 s (15 x 2 s) avant de rendre la main a la carte vanilla.
	static const int FFRX_SPAWN_RETRY_MS = 2000;
	static const int FFRX_SPAWN_MAX_RETRIES = 15;
	static ref map<int, int> s_mSpawnRetries;

	static map<int, int> SpawnRetries()
	{
		if (!s_mSpawnRetries)
			s_mSpawnRetries = new map<int, int>();

		return s_mSpawnRetries;
	}

	protected void FFRX_DeferredDeploy(int playerId)
	{
		JWK_PlayerRespawnLocationComponent best = FFRX_PickBestSpawn(playerId);
		if (best)
		{
			SpawnRetries().Remove(playerId);
			FFRX_DeployNow(playerId, best);
			return;
		}

		int tries = 0;
		if (SpawnRetries().Contains(playerId))
			tries = SpawnRetries().Get(playerId);
		tries = tries + 1;

		if (tries > FFRX_SPAWN_MAX_RETRIES)
		{
			// Toujours rien apres ~30 s : ce n'est plus une course au demarrage.
			Print(string.Format("[FFRX][Spawn] pid=%1 : aucune base apres %2 essais -> carte vanilla",
				playerId, tries));
			SpawnRetries().Remove(playerId);
			super.OpenSpawnSelectionMenu();
			return;
		}

		SpawnRetries().Set(playerId, tries);
		GetGame().GetCallqueue().CallLater(FFRX_DeferredDeploy, FFRX_SPAWN_RETRY_MS, false, playerId);
	}

	protected void FFRX_DeployNow(int playerId, JWK_PlayerRespawnLocationComponent best)
	{
		JWK_RespawnSystemComponent rsc =
			JWK_CompTU<JWK_RespawnSystemComponent>.FindIn(GetGame().GetGameMode());
		if (rsc)
			rsc.OnPlayerSpawnPointSelected(playerId, best);
	}

	// Highest-priority respawn location this player may currently use (runs FF's
	// CanPlayerRespawn, so FFRX_SpawnGating applies).
	protected JWK_PlayerRespawnLocationComponent FFRX_PickBestSpawn(int playerId)
	{
		SCR_PlayerController controller = SCR_PlayerController.Cast(
			GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!controller) return null;

		JWK_PlayerRespawnLocationComponent best;
		int bestTier = -1;

		array<EntityID> ids = JWK_IndexSystem.Get().GetAll(JWK_PlayerRespawnLocationComponent);
		foreach (EntityID id : ids)
		{
			JWK_PlayerRespawnLocationComponent loc =
				JWK_CompTU<JWK_PlayerRespawnLocationComponent>.FindIn(id);
			if (!loc) continue;

			JWK_EPlayerRespawnUnavailableReason reason;
			if (!loc.CanPlayerRespawn(controller, reason)) continue;

			int tier = FFRX_SpawnTiers.TierOf(loc);
			if (tier > bestTier)
			{
				bestTier = tier;
				best = loc;
			}
		}

		return best;
	}
}

// FF bug workaround: buying a VEHICLE in the shop makes DoBuy invoke the navbar
// CLOSE action synchronously, re-entering the navbar's ScriptInvoker mid-invoke
// ("SCRIPT (E): Recursive call of Invoke!"). Defer that close by a frame so the
// original invoke has returned first. (Vehicle branch only; normal items untouched.)
modded class JWK_ShopInterfaceUIComponent
{
	override protected void DoBuy()
	{
		if (!m_Controller.Buy(m_rSelectedItem)) return;

		if (JWK_VehicleUtils.IsVehiclePrefab(m_rSelectedItem)) {
			GetGame().GetCallqueue().CallLater(FFRX_DeferredClose, 0, false);
			return;
		}

		SelectItem(m_rSelectedItem);
	}

	protected void FFRX_DeferredClose()
	{
		if (m_Navbar)
			m_Navbar.GetOnActionRequested().Invoke(JWK_EShopInterfaceNavbarAction.CLOSE);
	}
}
