// FF - REMIXED - PVE — Field Manual: hide base/FF manual pages for features REMIXED disabled.
//
// The manual UI (SCR_FieldManualUI) already drops any entry whose m_bEnabled is
// false and cleans up now-empty sub-categories/categories. So we simply flag the
// disabled entries: override the (static) config loader, walk the loaded tree, and
// set m_bEnabled = false on entries whose m_eId is in our disabled set. No shadow
// override of FF's FreedomFighters.conf (survives FF updates; no collision).
//
// Disabled in REMIXED (see FFRX_GameSettings resting-OFF, FFRX_RemoveCamps,
// FFRX_RemoveCommandPost, FFRX_TrimMainMenu menu-J trim):
//   RESTING          -> repos + saut de temps forcés OFF
//   RESISTANCE_CAMPS -> camps retirés (catalogue + pose)
//   OB_COMMAND_POSTS -> poste de commandement (tente officier) retiré
//   GAMEPLAY_MONEY   -> Banque retirée du menu J ; éco = supplies/ravito
//   GAMEPLAY_RADIO   -> Radio retirée du menu J (le futur système radio joueur reste à faire)
//   GAMEPLAY_UNDERCOVER -> armée régulière en uniforme, pas d'infiltration civile
//   OB_FAST_TRAVEL   -> voyage rapide coupé
modded class SCR_FieldManualConfigLoader
{
	override static SCR_FieldManualConfigRoot LoadConfigRoot(ResourceName configPath)
	{
		SCR_FieldManualConfigRoot root = super.LoadConfigRoot(configPath);
		if (root)
			FFRX_Prune(root.m_aCategories);
		return root;
	}

	static void FFRX_Prune(array<ref SCR_FieldManualConfigCategory> cats)
	{
		if (!cats)
			return;

		foreach (SCR_FieldManualConfigCategory cat : cats)
		{
			if (!cat)
				continue;

			FFRX_Prune(cat.m_aCategories); // recurse sub-categories

			if (!cat.m_aEntries)
				continue;

			foreach (SCR_FieldManualConfigEntry entry : cat.m_aEntries)
			{
				if (entry && FFRX_IsDisabled(entry.m_eId))
					entry.m_bEnabled = false;
			}
		}
	}

	static bool FFRX_IsDisabled(EFieldManualEntryId id)
	{
		switch (id)
		{
			case EFieldManualEntryId.JWK_GAMEPLAY_RESTING:     return true; // repos + timeskip OFF
			case EFieldManualEntryId.JWK_RESISTANCE_CAMPS:     return true; // camps retirés
			case EFieldManualEntryId.JWK_OB_COMMAND_POSTS:     return true; // poste de commandement retiré
			case EFieldManualEntryId.JWK_GAMEPLAY_MONEY:       return true; // Banque retirée
			case EFieldManualEntryId.JWK_GAMEPLAY_RADIO:       return true; // Radio retirée du menu J
			case EFieldManualEntryId.JWK_GAMEPLAY_UNDERCOVER:  return true; // pas d'infiltration civile
			case EFieldManualEntryId.JWK_OB_FAST_TRAVEL:       return true; // voyage rapide coupé
		}
		return false;
	}
}
