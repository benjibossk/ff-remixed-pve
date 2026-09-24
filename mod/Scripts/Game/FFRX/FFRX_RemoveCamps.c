// FF - REMIXED - PVE
// Remove player camps / tents (not RP enough): players can no longer deploy the
// CampingKit that creates a JWK_PlayerCampEntity spawn.
//
// A camp is deployed as a placeable item (Prefabs/Items/Misc/CampingKit) whose
// placement runs through JWK_CampPlaceableItemHandler. FF gates every placement
// on CanPlace() (called on both client preview and server), so forcing it false
// blocks the tent from ever being placed. The MedicalTent BUILD item is a
// separate medical building and is unaffected.
//
// Spawn tiers stay MOB > FOB > Town (no Camp tier in practice now). The armory /
// medic / genie facilities are separate and keep working.
modded class JWK_CampPlaceableItemHandler
{
	override bool CanPlace(
		int playerId,
		JWK_PlaceableItemComponent itemComp,
		IEntity attachedTo,
		vector mat[4],
		bool isPreview,
		out JWK_EFeedback outReason
	) {
		outReason = JWK_EFeedback.BUILDING_CANT_BUILD_THIS;
		return false;
	}
}
