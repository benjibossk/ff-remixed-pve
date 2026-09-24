// FF - REMIXED - PVE
// Fix: FF's PLAYABLE worlds (Arland, Everon) ship WITHOUT a SCR_LoadoutManager in their
// system.layer -- only FF_TestWorld has one. On a dedicated server this spams
//   SCRIPT (E): Loadout manager is missing in the world!
// and any base-game arsenal / player-loadout feature (our arsenal crate loadouts) has no
// manager to talk to.
//
// The manager SELF-REGISTERS via ArmaReforgerScripted.RegisterLoadoutManager() when its
// component inits, so simply spawning the LoadoutManager_Base.et prefab at runtime makes it
// THE loadout manager of the game. World-agnostic (works on Arland, Everon, ...), no editing of
// the packed FF world. Called once, server-side, from FFRX_Groups.c -> OnGameModeStart().

class FFRX_WorldFix
{
	// SCR_LoadoutManager prefab (from FF_TestWorld_Layers/system.layer).
	protected static const ResourceName LOADOUT_MGR_PREFAB = "{AA4E7419A1FF65B0}Prefabs/MP/Managers/Loadouts/LoadoutManager_Base.et";

	//------------------------------------------------------------------------------------------------
	// Spawn a SCR_LoadoutManager if the world doesn't already provide one. Server authority only.
	static void EnsureLoadoutManager()
	{
		if (GetGame().GetLoadoutManager())
			return;   // world already has one (e.g. FF_TestWorld) -> nothing to do

		Resource res = Resource.Load(LOADOUT_MGR_PREFAB);
		if (!res.IsValid())
		{
			Print("[FFRX][WorldFix] LoadoutManager prefab introuvable -> skip.", LogLevel.WARNING);
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		IEntity mgr = GetGame().SpawnEntityPrefab(res, world, params);

		if (mgr && GetGame().GetLoadoutManager())
			Print("[FFRX][WorldFix] SCR_LoadoutManager absent du monde FF -> spawn + enregistrement OK.", LogLevel.NORMAL);
		else
			Print("[FFRX][WorldFix] Echec spawn/enregistrement du SCR_LoadoutManager.", LogLevel.WARNING);
	}
}
