// FF - REMIXED - PVE
// Garage rework (Benji): a claimed/parked vehicle is persistent forever at ANY
// distance (already the case: the garage sets DeleteOnStreamOut=false on park).
// We DROP the paid teleport recall and instead let the player drop a MAP MARKER
// on their vehicle to go get it physically.
//
// The garage "Retrieve" button already routes to
// JWK_PlayerControllerGarageComponent.RpcAsk_SpotRetrieveVehicle (server). We
// repurpose that: no cost, no teleport, no distance/obstruction gate -- the
// server reads the (streamed-out) vehicle position and sends it back to the
// requesting client, which drops a LOCAL static marker (PLACED_CUSTOM). Local
// markers are drawn by the base-game SCR_MapMarkersUI, which our empty-map
// (FFRX_EmptyMap only hides JWK layers) does NOT hide -- so the owner sees just
// their vehicle marker on an otherwise-blank map.
//
// TODO (cosmetic, needs layout): the button still reads "Retrieve" and the panel
// still shows a supplies/money cost. Functionally free now; relabel later.

modded class JWK_PlayerGarageSpotComponent
{
	// Server: world position of the parked vehicle (works while streamed out).
	vector FFRX_GetVehicleOrigin()
	{
		if (m_Vehicle) return m_Vehicle.GetOrigin();
		return vector.Zero;
	}
}

modded class JWK_PlayerControllerGarageComponent
{
	override void RpcAsk_SpotRetrieveVehicle(RplId garageSpotRpl)
	{
		JWK_PlayerGarageSpotComponent spot =
			JWK_CompTU<JWK_PlayerGarageSpotComponent>.FindRpl(garageSpotRpl);
		if (!spot || !spot.HasVehicle()) return;

		vector pos = spot.FFRX_GetVehicleOrigin();
		if (pos == vector.Zero) return;

		// Hand the position to the owning client so it can drop its own marker.
		Rpc(FFRX_RpcOwner_MarkVehicle, pos);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcOwner_MarkVehicle(vector pos)
	{
		SCR_MapMarkerManagerComponent mgr = SCR_MapMarkerManagerComponent.GetInstance();
		if (!mgr) return;

		// isLocal = true -> only this player sees it.
		mgr.InsertStaticMarkerByType(SCR_EMapMarkerType.PLACED_CUSTOM, pos[0], pos[2], true);
	}

	// Procurement gate (D12): a non-etat-major player requesting a vehicle queues
	// a validation request instead of spawning it. Etat-major spawn directly.
	override void RpcAsk_SpotProcureVehicle(RplId garageSpotRpl, ResourceName prefab)
	{
		int pid = GetOwnerPlayerId();
		if (FFRX_GroupsManager.IsEtatMajor(pid)) {
			super.RpcAsk_SpotProcureVehicle(garageSpotRpl, prefab);
			return;
		}

		FFRX_Procurement.CreateGarageRequest(pid, garageSpotRpl, prefab);
	}
}
