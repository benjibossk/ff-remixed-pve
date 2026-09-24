// FF - REMIXED - PVE — Field Manual
// New entry IDs for the "REMIXED - PVE" chapter. Appends to the base-game
// EFieldManualEntryId enum (FF already appends its own JWK_* values the same way;
// modded enum extensions from multiple addons stack).
//
// The chapter itself is data-driven:
//   Configs/FieldManual/FFRX.conf                         -> the category + entries (reference these IDs)
//   Configs/Modded/FieldManual/FieldManualConfigRoot.conf -> appends the category to the manual root ({17295EF80DC38D53})
//   localization_MCD.* (.st / .fr_fr / .en_us)            -> the #FFRX-FM-* strings
modded enum EFieldManualEntryId
{
	FFRX_OVERVIEW,
	FFRX_SPAWN,
	FFRX_INTEL,
	FFRX_ROLES,
	FFRX_ARSENAL,
	FFRX_ECONOMY,
	FFRX_ENEMY,
	FFRX_REALISM,
	FFRX_FACTION
}
