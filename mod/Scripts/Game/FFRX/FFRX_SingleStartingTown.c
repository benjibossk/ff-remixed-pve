// FF - REMIXED - PVE
// Brique A: ZERO starting support everywhere.
//
// Previous design seeded one friendly starting town. New design (see FFRX_DefaultFob)
// starts the resistance on a single default FOB instead, so no town should be
// pacified at launch: we override FF's initial support distribution to place NOTHING.
// Every town/POI then starts fully contested/enemy -> livelier map, and towns (which
// are only spawnable once they have enough supporters) are never a valid spawn at
// launch -- the FOB is the sole spawn.
//
// NOTE: we intentionally do NOT call super.DistributeStartingSupport() -- that is
// exactly the vanilla behaviour we are replacing.

modded class JWK_HeartsAndMindsManagerComponent
{
	override protected void DistributeStartingSupport()
	{
		JWK_Log.Log(this, "[FFRX] Starting support = 0 everywhere (default-FOB start).");
	}
}
