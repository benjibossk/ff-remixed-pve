// FF - REMIXED - PVE
// DARC missions integration, bent to our design:
//   1. NO money reward (fog-of-war PVE: no cash economy on missions).
//   2. NO mission map icons (players find missions via intel text coords, not markers;
//      matches the empty-map-for-non-admins design).
//   3. CONVOY = supply raid: a defeated convoy truck is filled with SUPPLIES to haul
//      back (Pillar 3 / D5 pillage), reusing FF's own vehicle-supply API.
//
// DARC is a packed dependency (DarcCore + DarcMissions + CompatFF). We only mod its
// classes from here; DARC's own scripts stay precompiled in their paks (no EPF break).

// --- 1. Kill the money reward -----------------------------------------------------
// The FF compat adds `modded SDRC_Mission.GiveReward()` -> JWK.AddMoney_S + a money
// notification. FF-REMIXED loads AFTER the compat, so our override is outermost; by
// NOT calling super we skip the compat's money grant entirely.
// SDRC_MapMarkerHelper is a SEALED class (can't be modded), so we suppress markers at
// the SOURCE instead: SDRC_Mission.ShowMarker() is what calls CreateMapMarker. DARC
// draws SCR_MapMarkerBase markers (a separate system from FF's JWK map layers, so
// FFRX_EmptyMap doesn't hide them). Overriding ShowMarker() to no-op means no mission
// icon ever appears (players navigate by intel coords -- fog-of-war).
modded class SDRC_Mission
{
	// --- 1. No money reward ---
	override void GiveReward()
	{
		// No money reward in FF-REMIXED. (FF has no XP awarder either, so nothing to give.)
	}

	// --- 2. No mission map marker ---
	override void ShowMarker()
	{
		// Intentionally empty: no DARC mission markers on the map.
	}

	// --- 3. Attribution des spawns au recensement -------------------------------
	// DARC ne passe pas par la file de spawn FF et ses helpers sont `sealed`
	// (SDRC_AIHelper), donc on ne peut pas intercepter son spawn. En revanche
	// MissionStart() est publique et c'est elle qui declenche tout : on pose un
	// indice de source juste avant, et les groupes qui naissent dans la foulee le
	// ramassent (cf. FFRX_SpawnCensus.SetSourceHint / ResolveSource).
	// On ne change RIEN au comportement : super.MissionStart() est appele normalement.
	override void MissionStart()
	{
		FFRX_SpawnCensus.SetSourceHint("DARC " + ClassName());
		super.MissionStart();
	}
}

// --- 3. Convoy = supply raid (Pilier 3 / D5) --------------------------------------
// A defeated DARC CONVOY becomes a physical supply haul: we fill the convoy vehicle's
// SUPPLIES cargo to the brim so players drive it back to a base and unload via FF's
// native logistics (= the "vulnerable logistics / pillage" revenue source). DARC empties
// the vehicle INVENTORY on spawn and only adds ITEM loot (weapons) on win -- it never
// touches the SUPPLIES resource, so this is the missing economy piece. DoWin runs on the
// server authority (called from SDRC_GMHelper.EndMission), so no RPC needed.
modded class SDRC_Mission_Convoy
{
	override void DoWin()
	{
		super.DoWin();               // DARC: puts item loot into the convoy vehicle
		FFRX_LoadConvoySupplies();
	}

	// The convoy vehicle is inserted into the protected m_EntityList at spawn; find it
	// and top up its supply cargo.
	protected void FFRX_LoadConvoySupplies()
	{
		IEntity veh = null;
		foreach (IEntity e : m_EntityList)
		{
			if (e && SDRC_VehicleHelper.IsVehicle(e))
			{
				veh = e;
				break;
			}
		}
		if (!veh)
		{
			Print("[FFRX][Darc] Convoi gagne mais aucun vehicule trouve -> pas de supply charge.", LogLevel.WARNING);
			return;
		}

		int added = FFRX_DarcSupply.FillVehicleSupplies(veh);
		if (added < 0)
			Print("[FFRX][Darc] Convoi gagne : vehicule sans stockage supply (voiture/armed) -> rien a charger.", LogLevel.NORMAL);
		else if (added == 0)
			Print("[FFRX][Darc] Convoi gagne : camion deja plein de supplies.", LogLevel.NORMAL);
		else
			Print(string.Format("[FFRX][Darc] +++ Convoi pille : camion charge a bloc (+%1 supplies). Ramene-le a une base.", added), LogLevel.NORMAL);
	}
}

// Fill a vehicle's SUPPLIES cargo container to its maximum. Returns the amount added,
// 0 if already full, or -1 if the vehicle has no supply container. Uses the exact
// base-game API FF itself uses (SCR_ResourceComponent.FindResourceComponent +
// GetContainer(SUPPLIES) + SetResourceValue), see JWK_AmbientTrafficSystem.
class FFRX_DarcSupply
{
	static int FillVehicleSupplies(IEntity veh)
	{
		SCR_ResourceComponent resourceComp = SCR_ResourceComponent.FindResourceComponent(veh);
		SCR_ResourceContainer resourceContainer;
		if (!resourceComp || !resourceComp.GetContainer(EResourceType.SUPPLIES, resourceContainer))
			return -1;

		float max = resourceContainer.GetMaxResourceValue();
		float toAdd = max - resourceContainer.GetResourceValue();
		if (toAdd <= 0)
			return 0;

		resourceContainer.SetResourceValue(max);
		return (int)toAdd;
	}
}
