// FF - REMIXED - PVE
// Make FF read the inline JWK_VehicleAttributesComponent of a vehicle prefab.
//
// Our FIA "Armée française" vehicles (FFRX_GBC180_*, VBL, Griffon, VBCI) carry a
// JWK_VehicleAttributesComponent on the prefab. But the INSTALLED FF's
// JWK_VehicleManagerConfig.GetAttributesForPrefab only looks up the config whitelist
// (m_aAttributeSets) and returns null for anything not listed -> JWK_Faction's
// ValidateVehiclePrefabs flags them "missing or invalid attributes" and the spawn
// filter (FilterVehiclesByAttributes) DROPS them -> they never appear in the FIA
// garages / faction vehicle pool.
//
// Fix (generic): when the whitelist misses, fall back to reading the inline
// JWK_VehicleAttributesComponent from the prefab source (JWK_VehicleUtils already
// provides LoadAttributesFromPrefab). Works for ANY vehicle carrying the component.
// TacticalFlava USSR vehicles are unaffected (their lookup already succeeds via super).
//
// ⚠️ Restate [BaseContainerProps(configRoot: true)] — omitting it on a modded configRoot
// class nulls the component's m_Config binding (crash), same gotcha as JWK_GameSettingsConfig.
[BaseContainerProps(configRoot: true)]
modded class JWK_VehicleManagerConfig
{
	override JWK_VehicleAttributes GetAttributesForPrefab(const ResourceName prefab)
	{
		JWK_VehicleAttributes found = super.GetAttributesForPrefab(prefab);
		if (found)
			return found;

		return JWK_VehicleUtils.LoadAttributesFromPrefab(prefab);
	}
}
