// FF - REMIXED - PVE
// Intel from enemy deaths (server-side). Two paths:
//   1. OFFICER (SERGEANT+) killed by a player -> RELIABLE intel to the killer (precise
//      coords of an active DARC mission / checkpoint, no false lead). The D7 "reliable
//      source" -- the counterpart to the imprecise, sometimes-false civilian rumor.
//   2. ANY enemy -> rare (3%) chance to drop the cache NOTE into the lootable corpse;
//      reading it reveals a native FF clear-camp mission (FFRX_CacheNote ->
//      FFRX_CacheSpawner). Cap-gated so we never exceed MAX_ACTIVE (3) camps.
// ⚠️ ATTRIBUT DE CLASSE REDECLARE -- NE PAS RETIRER.
// Un `modded class` qui omet l'attribut de l'original perd sa deserialisation.
// Le compilateur ne le dit PAS : il rend des dizaines de "Too many instructions
// per function" et "Incompatible parameter" sur des fichiers du JEU DE BASE et de
// FF (JWK_ConvoyAIDeployer, JWK_ShopContext...), aucun ne citant ce fichier, puis
// "Can't compile Game script module!". Panne du dedie le 2026-09-15.
[ComponentEditorProps(category: "GameScripted/Character", description: "Scripted character controller", icon: HYBRID_COMPONENT_ICON)]
modded class SCR_CharacterControllerComponent
{
	static const ResourceName FFRX_INTEL_DOC         = "{921CB34046441F46}Prefabs/Items/Misc/Caches/Campaign_CacheNote_Base.et";
	static const float        FFRX_ENEMY_DROP_CHANCE = 0.03; // rare (3% of enemy kills)
	static const float        FFRX_OFFICER_DOC_CHANCE = 0.5; // an officer doesn't ALWAYS carry papers

	override void OnDeath(IEntity instigatorEntity, notnull Instigator instigator)
	{
		super.OnDeath(instigatorEntity, instigator);

		if (!Replication.IsServer())
			return;

		IEntity character = GetCharacter();
		if (!character)
			return;

		// Enemy faction only (never players / friendlies / civilians).
		SCR_FactionAffiliationComponent fac =
			JWK_CompTU<SCR_FactionAffiliationComponent>.FindIn(character);
		if (!fac)
			return;
		if (JWK.GetFactions().GetRole(fac) != JWK_EFactionRole.ENEMY)
			return;

		// (1) OFFICER killed BY A PLAYER -> reliable "documents on the body" intel to the
		// killer (precise, no false lead -- the D7 reliable source). "Officer" = a faction
		// officer prefab, exactly how FF's KillEnemyHvt job defines its HVT
		// (JWK_CombatFaction.GetMergedForce().m_aOfficerCharacters), not a generic rank.
		// He doesn't carry papers every time (FFRX_OFFICER_DOC_CHANCE).
		// Delivered as PAPERS ON THE BODY, not as a HUD hint: the player has to loot
		// the corpse and read the item's description. Intel is an object you carry,
		// keep and can hand to someone else -- same treatment as the cache note below.
		if (FFRX_IsEnemyOfficer(character) && JWK.Random.RandFloat01() < FFRX_OFFICER_DOC_CHANCE)
		{
			int killerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(instigatorEntity);
			if (killerId > 0)
				GetGame().GetCallqueue().CallLater(FFRX_DropOfficerPapers, 500, false, character);
		}

		// (2) Any enemy: rare chance to drop the cache NOTE (spawns a camp) into the
		// lootable corpse. Deferred so FF's loot balancer runs first. Cap-gated.
		if (FFRX_CacheSpawner.CanSpawn() && JWK.Random.RandFloat01() < FFRX_ENEMY_DROP_CHANCE)
			GetGame().GetCallqueue().CallLater(FFRX_DropIntelNote, 500, false, character);
	}

	// The officer's papers: precise, never a false lead (D7 reliable source). Written
	// on the same note item, armed through FFRX_RadioIntel so our SCR_CacheNoteComponent
	// override fills THIS note with the papers instead of spawning a camp.
	protected void FFRX_DropOfficerPapers(IEntity character)
	{
		if (!character)
			return;

		string text = FFRX_IntelSystem.BuildReliableDocText(character.GetOrigin());
		if (text == "")
			return; // nothing worth writing down

		SCR_InventoryStorageManagerComponent inv =
			JWK_CompTU<SCR_InventoryStorageManagerComponent>.FindIn(character);
		if (!inv)
			return;

		FFRX_RadioIntel.ArmDocument("Documents sur l'officier\n" + text);
		bool ok = inv.TrySpawnPrefabToStorage(FFRX_INTEL_DOC);
		// Only disarm if nothing spawned. On success the note's OnPostInit consumes
		// it -- clearing here would break if the spawn is deferred by a frame.
		if (!ok)
			FFRX_RadioIntel.ConsumeDocument();

		Print("[FFRX][Intel] Officer papers left on the body -> " + ok, LogLevel.NORMAL);
	}

	protected void FFRX_DropIntelNote(IEntity character)
	{
		if (!character)
			return;
		SCR_InventoryStorageManagerComponent inv =
			JWK_CompTU<SCR_InventoryStorageManagerComponent>.FindIn(character);
		if (!inv)
			return;
		bool ok = inv.TrySpawnPrefabToStorage(FFRX_INTEL_DOC);
		Print("[FFRX][Intel] Enemy corpse dropped intel note -> " + ok, LogLevel.NORMAL);
	}

	// True if this character was spawned from one of the enemy faction's OFFICER prefabs
	// (JWK_CombatFaction merged force m_aOfficerCharacters -- the same pool FF's KillHVT
	// job draws its target from). That's FF's own notion of an HVT/officer.
	protected bool FFRX_IsEnemyOfficer(IEntity character)
	{
		EntityPrefabData pd = character.GetPrefabData();
		if (!pd)
			return false;
		ResourceName prefab = pd.GetPrefabName();
		if (prefab == string.Empty)
			return false;

		JWK_CombatFactionTrait cft = JWK_FactionTraitTU<JWK_CombatFactionTrait>.GetByRole(JWK_EFactionRole.ENEMY);
		if (!cft)
			return false;
		JWK_FactionForceConfig force = cft.GetMergedForce();
		if (!force)
			return false;

		// FF 0.70.0: m_aOfficerCharacters is now protected -> read via the public getter.
		foreach (ResourceName officer : force.GetCharactersOfType(JWK_EForceCharacterType.OFFICER))
			if (officer == prefab)
				return true;
		return false;
	}
}
