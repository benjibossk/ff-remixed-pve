// FF - REMIXED - PVE
// Procurement request/validation (D12 + D4).
//
// A non-etat-major player who acquires a VEHICLE/helo does NOT get it directly:
// it creates a pending REQUEST. A member of the etat-major (command) squad
// validates it (from the command-post interface, built next) -> the vehicle is
// then delivered and supplies charged. Etat-major players acquire directly.
//
// Two acquisition paths are gated:
//   GARAGE - garage procurement (JWK_PlayerControllerGarageComponent, FFRX_GarageMarker)
//   SHOP   - vehicle shop / import buy (JWK_PlayerControllerComponent, FFRX_ProcurementShop)
//
// This file is the UI-agnostic backend: the request store + fulfillment. Server only.

enum FFRX_EProcKind
{
	GARAGE,
	SHOP
}

class FFRX_ProcRequest
{
	int          id;
	int          requesterId;
	int          kind;       // FFRX_EProcKind
	RplId        targetRpl;  // garage spot (GARAGE) or shop component (SHOP)
	ResourceName prefab;
}

class FFRX_Procurement
{
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	static ref array<ref FFRX_ProcRequest> s_aRequests;

	static array<ref FFRX_ProcRequest> Requests()
	{
		if (!s_aRequests)
			s_aRequests = new array<ref FFRX_ProcRequest>();

		return s_aRequests;
	}
	static int s_iNextId = 1;

	static void CreateGarageRequest(int requesterId, RplId spotRpl, ResourceName prefab)
	{
		Add(requesterId, FFRX_EProcKind.GARAGE, spotRpl, prefab);
	}

	static void CreateShopRequest(int requesterId, RplId shopRpl, ResourceName prefab)
	{
		Add(requesterId, FFRX_EProcKind.SHOP, shopRpl, prefab);
	}

	protected static void Add(int requesterId, FFRX_EProcKind kind, RplId targetRpl, ResourceName prefab)
	{
		FFRX_ProcRequest req = new FFRX_ProcRequest();
		req.id          = s_iNextId;
		s_iNextId       = s_iNextId + 1;
		req.requesterId = requesterId;
		req.kind        = kind;
		req.targetRpl   = targetRpl;
		req.prefab      = prefab;
		Requests().Insert(req);

		Print(string.Format("[FFRX][Proc] Demande #%1 par joueur %2 pour %3 (kind=%4, en attente etat-major).",
			req.id, requesterId, JWK_PrefabUtils.GetPrefabFileName(prefab), kind));

		Notify(req);
	}

	// Popup feedback: tell the requester their demand was sent. Etat-major is NOT
	// pinged anymore -- the per-request notification spammed them; instead they review
	// and validate pending requests on demand with the #demandes command.
	protected static void Notify(FFRX_ProcRequest req)
	{
		PopupTo(req.requesterId, "Demande de vehicule envoyee a l'etat-major.");
	}

	// Human-readable line for a pending request (used by the #demandes dialog).
	static string DescribeRequest(FFRX_ProcRequest r)
	{
		string veh = JWK_PrefabUtils.GetPrefabFileName(r.prefab);
		string who = GetGame().GetPlayerManager().GetPlayerName(r.requesterId);
		return string.Format("Accepter la demande de %1 (par %2) ?", veh, who);
	}

	protected static void PopupTo(int playerId, string text)
	{
		JWK_PlayerControllerComponent jpc = JWK.GetPlayerController(playerId);
		if (jpc) jpc.FFRX_Popup(text);
	}

	static int GetPending(out array<FFRX_ProcRequest> outReqs)
	{
		outReqs.Clear();
		foreach (FFRX_ProcRequest r : Requests())
			outReqs.Insert(r);
		return outReqs.Count();
	}

	static FFRX_ProcRequest FindRequest(int id)
	{
		foreach (FFRX_ProcRequest r : Requests())
			if (r.id == id) return r;
		return null;
	}

	// Etat-major validates -> deliver the vehicle, then drop the request.
	static bool Approve(int id, int byPlayerId)
	{
		if (!FFRX_GroupsManager.IsEtatMajor(byPlayerId)) return false;

		FFRX_ProcRequest r = FindRequest(id);
		if (!r) return false;

		bool ok;
		if (r.kind == FFRX_EProcKind.GARAGE)
			ok = DoSpawnGarage(r.targetRpl, r.requesterId, r.prefab);
		else
			ok = DoBuyShop(r.targetRpl, r.requesterId, r.prefab);

		Remove(id);
		Print(string.Format("[FFRX][Proc] Demande #%1 validee par %2 -> ok=%3.", id, byPlayerId, ok));
		return ok;
	}

	static bool Deny(int id, int byPlayerId)
	{
		if (!FFRX_GroupsManager.IsEtatMajor(byPlayerId)) return false;
		if (!FindRequest(id)) return false;

		Remove(id);
		Print(string.Format("[FFRX][Proc] Demande #%1 refusee par %2.", id, byPlayerId));
		return true;
	}

	static void Remove(int id)
	{
		for (int i = Requests().Count() - 1; i >= 0; i--) {
			if (Requests()[i].id == id) {
				Requests().Remove(i);
				return;
			}
		}
	}

	// --- Fulfillment ---

	// Garage: spawn + charge, replicating JWK_PlayerControllerGarageComponent.RpcAsk_SpotProcureVehicle.
	static bool DoSpawnGarage(RplId spotRpl, int requesterId, ResourceName prefab)
	{
		JWK_PlayerGarageSpotComponent spot = JWK_CompTU<JWK_PlayerGarageSpotComponent>.FindRpl(spotRpl);
		if (!spot || spot.HasVehicle()) return false;

		JWK_ShopItemAttributes attr = JWK.GetShopManager().GetItemAttributes(prefab);
		if (!attr) return false;
		int cost = attr.m_iSuppliesPrice;

		JWK_LogisticsStorageControllerComponent storage = spot.GetGarage().GetLogistics();
		if (!storage || storage.GetResources(JWK_ELogisticsResourceType.SUPPLIES) < cost) return false;

		if (!spot.ProcureVehicle(prefab)) return false;

		storage.TakeResources(JWK_ELogisticsResourceType.SUPPLIES, cost);

		// Lock the delivered vehicle to the requester (keys) + etat-major pass-key.
		if (spot.GetOwner())
			FFRX_VehicleLock.LockProcuredNear(spot.GetOwner().GetOrigin(), requesterId);
		return true;
	}

	// Shop: run the real purchase for the requester (charges + spawns at the shop).
	static bool DoBuyShop(RplId shopRpl, int requesterId, ResourceName prefab)
	{
		JWK_BaseShopComponent shop = JWK_CompTU<JWK_BaseShopComponent>.FindRpl(shopRpl);
		if (!shop) return false;

		shop.PerformPlayerItemPurchase_S(requesterId, prefab);

		// Lock the purchased vehicle to the requester (keys) + etat-major pass-key.
		if (shop.GetOwner())
			FFRX_VehicleLock.LockProcuredNear(shop.GetOwner().GetOrigin(), requesterId);
		return true;
	}
}
