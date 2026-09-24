// FF - REMIXED - PVE
// Remove camping items (camp kit + deployable tents) from the catalog entirely.
//
// FF adds these via shop item groups (e.g. Configs/Shops/ItemGroups/Camping.conf).
// Every shop/list builds its contents through JWK_BasePrefabsListConfig.GetPrefabs()
// -> so filtering the four camping prefabs out of that result strips them from ALL
// shops at once, in code, with no config override to own/maintain. This is the
// clean "not in the catalog" removal (complements FFRX_RemoveCamps which also
// blocks placement, as belt-and-suspenders for any kit from other sources).
//
// Banned prefabs (GUIDs from Camping.conf):
//   D5B9F855C140A40E  CampingKit
//   9D4607422CBA0336  Tent_Deployable_Base
//   7965218CCA03224E  Tent_Deployable_US
//   2438884C8F370AB9  Tent_Deployable_USSR
modded class JWK_BasePrefabsListConfig
{
	override void GetPrefabs(out array<ResourceName> result, bool sideLoading)
	{
		super.GetPrefabs(result, sideLoading);

		for (int i = result.Count() - 1; i >= 0; i--) {
			if (FFRX_IsCampingPrefab(result[i]))
				result.Remove(i);
		}
	}

	protected bool FFRX_IsCampingPrefab(ResourceName prefab)
	{
		string s = prefab;
		return s.Contains("D5B9F855C140A40E")
			|| s.Contains("9D4607422CBA0336")
			|| s.Contains("7965218CCA03224E")
			|| s.Contains("2438884C8F370AB9");
	}
}
