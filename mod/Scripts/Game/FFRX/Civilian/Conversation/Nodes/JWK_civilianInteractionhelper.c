// ============================================================================================================================
//  MCD_CivilianInteractionHelper
//
//  Utilitaire centralisé pour tous les nodes de conversation civile MCD.
// ============================================================================================================================
class MCD_CivilianInteractionHelper
{
	// ⚠️ Pas d'initialiseur immediat sur un champ statique (meme avec des valeurs) : ils
	// sont hisses dans UNE fonction d'init partagee par vanilla et TOUS les mods, dont le
	// buffer de 64 Ko deborde en "Too many instructions per function" sur des fichiers
	// innocents. Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<ResourceName> s_aWeaponPool;

	static array<ResourceName> WEAPON_POOL()
	{
		if (!s_aWeaponPool)
		{
			s_aWeaponPool = new array<ResourceName>();
			s_aWeaponPool.Insert("{C0F7DD85A86B2900}Prefabs/Weapons/Handguns/PM/Handgun_PM.et");
			s_aWeaponPool.Insert("{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et");
		}

		return s_aWeaponPool;
	}

	static const float PROXIMITY_WANTED_RADIUS = 25.0;
	// FF 0.70.0: hearts&minds modifiers are now a JWK_EOverTimeModifier enum, not string IDs.
	static const JWK_EOverTimeModifier MP_CALL_MODIFIER = JWK_EOverTimeModifier.THREAT_RESISTANCE_REPORTED_CALL;

	// ------------------------------------------------------------------------------------------------------------------------
	//  ResolveContext
	// ------------------------------------------------------------------------------------------------------------------------
	static bool ResolveContext(
		JWK_ConversationContext context,
		out JWK_CivilianCharacterComponent outCiv,
		out JWK_PlayerControllerComponent outPlayerCtrl,
		out IEntity outPlayerEntity
	)
	{
		outPlayerEntity = context.GetPlayer().GetOwnerPlayerEntity();
		if (!outPlayerEntity)
		{
			Print("[FF][MCD][ERROR] ResolveContext: Player entity is null");
			return false;
		}

		outPlayerCtrl = JWK_CompTU<JWK_PlayerControllerComponent>.FindIn(
			context.GetPlayer().GetOwner()
		);

		outCiv = JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(
			context.GetTarget().GetOwner()
		);

		if (!outCiv)
		{
			Print("[FF][MCD][ERROR] ResolveContext: Civilian component not found");
			return false;
		}

		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  CheckAndApplyEnemyAlert
	// ------------------------------------------------------------------------------------------------------------------------
	static bool CheckAndApplyEnemyAlert(string callerNode, JWK_CivilianCharacterComponent civ, IEntity playerEntity)
	{
		bool triggered = false;

		bool enemyNearPlayer = HasEnemySoldiersNearby(playerEntity, playerEntity, PROXIMITY_WANTED_RADIUS);
		Print("[FF][MCD][ALERT] (" + callerNode + ") proximityCheck r=" + PROXIMITY_WANTED_RADIUS + "m -> " + enemyNearPlayer);

		if (enemyNearPlayer)
		{
			ApplyHeat(playerEntity, JWK_WantedHeatComponent.HEAT_DETECTED_COMBATANT);
			triggered = true;
		}

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (cache)
		{
			float radius    = cache.m_fMCD_GreetHostileAlertRadius;
			float heatLevel = cache.m_fMCD_GreetHostileAlertHeat;

			if (radius > 0 && heatLevel > 0)
			{
				bool enemyNearCiv = HasEnemySoldiersNearby(civ.GetOwner(), playerEntity, radius);
				Print("[FF][MCD][ALERT] (" + callerNode + ") civilAlertCheck r=" + radius + "m heat=" + heatLevel + " -> " + enemyNearCiv);

				if (enemyNearCiv)
				{
					ApplyHeat(playerEntity, heatLevel);
					triggered = true;
				}
			}
		}

		return triggered;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  TryCallMilitaryPolice_S
	// ------------------------------------------------------------------------------------------------------------------------
	static bool TryCallMilitaryPolice_S(JWK_CivilianCharacterComponent civ, IEntity playerEntity)
	{
		if (!Replication.IsServer())
		{
			Print("[FF][MCD][MP] TryCallMilitaryPolice_S: NOT server, skipping");
			return false;
		}

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache) return false;

		float mpChance = cache.m_fMCD_CivCallMpChance;
		float roll     = JWK.Random.RandFloat01();
		Print("[FF][MCD][MP] TryCallMilitaryPolice_S: chance=" + mpChance + " roll=" + roll);

		if (roll > mpChance)
		{
			Print("[FF][MCD][MP] TryCallMilitaryPolice_S: roll failed, no MP call");
			return false;
		}

		JWK_HeartsAndMindsLocationComponent hnm =
			JWK.GetHeartsAndMinds().GetNearest(civ.GetOwner().GetOrigin());

		if (!hnm)
		{
			Print("[FF][MCD][MP] TryCallMilitaryPolice_S: No H&M location found, no QRF");
			return false;
		}

		if (hnm.HasModifier(JWK_ETownModifierSystem.THREAT, MP_CALL_MODIFIER))
		{
			Print("[FF][MCD][MP] TryCallMilitaryPolice_S: modifier already active, skipping duplicate QRF");
			return false;
		}

		IEntity qrfController = JWK.GetQrfManager().CreateQrfEvent_S(
			playerEntity.GetOrigin(),
			JWK_EFactionRole.ENEMY,
			JWK_EQrfType.MILITARY_POLICE_RESPONSE
		);

		if (!qrfController)
		{
			Print("[FF][MCD][MP] TryCallMilitaryPolice_S: QRF creation failed (prefab manquant ?)");
			return false;
		}

		hnm.AddModifierByType(JWK_ETownModifierSystem.THREAT, MP_CALL_MODIFIER);

		Print("[FF][MCD][MP] TryCallMilitaryPolice_S: QRF SPAWNED at " + playerEntity.GetOrigin().ToString());
		return true;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  HasEnemySoldiersNearby
	// ------------------------------------------------------------------------------------------------------------------------
	static bool HasEnemySoldiersNearby(IEntity originEntity, IEntity playerEntity, float radius)
	{
		if (!originEntity || !playerEntity || radius <= 0)
			return false;

		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld) return false;

		FactionAffiliationComponent playerFacComp = JWK_CompTU<FactionAffiliationComponent>.FindIn(playerEntity);
		if (!playerFacComp) return false;

		Faction playerFaction = playerFacComp.GetAffiliatedFaction();
		if (!playerFaction) return false;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		vector origin  = originEntity.GetOrigin();
		float radiusSq = radius * radius;
		int checked    = 0;

		foreach (AIAgent agent : agents)
		{
			IEntity entity = agent.GetControlledEntity();
			if (!entity || entity == playerEntity) continue;

			if (vector.DistanceSq(entity.GetOrigin(), origin) > radiusSq) continue;

			ChimeraCharacter character = ChimeraCharacter.Cast(entity);
			if (!character) continue;

			DamageManagerComponent dmg = JWK_CompTU<DamageManagerComponent>.FindIn(entity);
			if (dmg && dmg.IsDestroyed()) continue;

			checked++;

			FactionAffiliationComponent facComp = JWK_CompTU<FactionAffiliationComponent>.FindIn(entity);
			if (!facComp) continue;

			Faction entityFaction = facComp.GetAffiliatedFaction();
			if (!entityFaction) continue;

			if (playerFaction.IsFactionEnemy(entityFaction))
			{
				Print("[FF][MCD][ALERT] HasEnemySoldiersNearby: FOUND (" + entityFaction.GetFactionKey() + ") dist=" + vector.Distance(entity.GetOrigin(), origin) + "m after " + checked + " checks");
				return true;
			}
		}

		Print("[FF][MCD][ALERT] HasEnemySoldiersNearby: none (checked " + checked + "/" + agents.Count() + " agents in radius)");
		return false;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  ApplyHeat
	// ------------------------------------------------------------------------------------------------------------------------
	static void ApplyHeat(IEntity playerEntity, float heatLevel)
	{
		JWK_WantedHeatComponent heat = JWK_CompTU<JWK_WantedHeatComponent>.FindIn(playerEntity);
		if (!heat) return;

		float before = heat.GetHeatLevel();
		heat.SetBaseHeatLevel_S(heatLevel);
		float after = heat.GetHeatLevel();

		Print("[FF][MCD][HEAT] ApplyHeat: requested=" + heatLevel + " | before=" + before + " | after=" + after);
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  TurnMilitarilyHostile
	//  Fix Johnny : SetResistanceAttitude_S → persiste l'état hostile pour les conversations futures
	// ------------------------------------------------------------------------------------------------------------------------
	static bool TurnMilitarilyHostile(JWK_CivilianCharacterComponent civ)
	{
		IEntity civEntity = civ.GetOwner();
		if (!civEntity) return false;

		// Persister l'attitude NEGATIVE via FF — important pour que les choices
		// s'adaptent correctement lors des conversations suivantes avec ce civil
		civ.SetResistanceAttitude_S(JWK_EResistanceAttitude.NEGATIVE);

		AIControlComponent aiControl = JWK_CompTU<AIControlComponent>.FindIn(civEntity);
		if (!aiControl) return false;

		AIAgent agent = aiControl.GetControlAIAgent();
		if (!agent) return false;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group) return false;

		JWK_FactionManager factionManager = JWK_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager) return false;

		Faction enemyFaction = factionManager.GetEnemyFaction();
		if (!enemyFaction) return false;

		return group.SetFaction(enemyFaction);
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  AlterNearestHeartsAndMinds
	// ------------------------------------------------------------------------------------------------------------------------
	static void AlterNearestHeartsAndMinds(
		JWK_CivilianCharacterComponent civ,
		int supportersDelta,
		int hostilesDelta,
		bool applyThreatModifier = false,
		JWK_EOverTimeModifier threatModifier = JWK_EOverTimeModifier.THREAT_CIVILIAN_EXTORTED
	)
	{
		JWK_HeartsAndMindsLocationComponent hnm =
			JWK.GetHeartsAndMinds().GetNearest(civ.GetOwner().GetOrigin());

		if (!hnm) return;

		if (supportersDelta != 0) hnm.AlterSupporters(supportersDelta);
		if (hostilesDelta != 0)   hnm.AlterHostiles(hostilesDelta);
		if (applyThreatModifier)  hnm.AddModifierByType(JWK_ETownModifierSystem.THREAT, threatModifier);
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  GiveRandomFactionItem
	//  Donne au joueur un item NON-ARME (catalogue ITEM) de sa propre faction — via
	//  le système de catalogue FF, donc respecte automatiquement les intégrations de mods.
	// ------------------------------------------------------------------------------------------------------------------------
	static bool GiveRandomFactionItem(IEntity playerEntity)
	{
		if (!playerEntity)
			return false;

		FactionAffiliationComponent facComp = JWK_CompTU<FactionAffiliationComponent>.FindIn(playerEntity);
		if (!facComp)
			return false;

		SCR_Faction faction = SCR_Faction.Cast(facComp.GetAffiliatedFaction());
		if (!faction)
			return false;

		SCR_EntityCatalogManagerComponent catalogMgr = SCR_EntityCatalogManagerComponent.GetInstance();
		if (!catalogMgr)
			return false;

		SCR_EntityCatalog itemCatalog = catalogMgr.GetFactionEntityCatalogOfType(EEntityCatalogType.ITEM, faction, false);
		if (!itemCatalog)
		{
			Print("[FF][MCD][GIFT] No ITEM catalog for player faction");
			return false;
		}

		array<SCR_EntityCatalogEntry> entries = {};
		itemCatalog.GetEntityList(entries);
		if (entries.IsEmpty())
		{
			Print("[FF][MCD][GIFT] ITEM catalog is empty");
			return false;
		}

		SCR_InventoryStorageManagerComponent inventory =
			JWK_CompTU<SCR_InventoryStorageManagerComponent>.FindIn(playerEntity);
		if (!inventory)
			return false;

		int index = Math.RandomInt(0, entries.Count());
		ResourceName prefab = entries[index].GetPrefab();
		if (prefab.IsEmpty())
			return false;

		bool ok = inventory.TrySpawnPrefabToStorage(prefab);
		Print("[FF][MCD][GIFT] GiveRandomFactionItem: " + prefab + " -> " + ok);
		return ok;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  GiveIntelDocument
	//  Donne au joueur une "note de cache" (item base-game Campaign_CacheNote_Base). En
	//  FF, FFRX_CacheNote la remplit de coords de grille d'un lieu ennemi (precision
	//  aleatoire) affichees dans la description de l'item. Source d'intel Pilier 2.
	// ------------------------------------------------------------------------------------------------------------------------
	static const ResourceName MCD_INTEL_DOC = "{921CB34046441F46}Prefabs/Items/Misc/Caches/Campaign_CacheNote_Base.et";

	static bool GiveIntelDocument(IEntity playerEntity)
	{
		if (!playerEntity)
			return false;

		SCR_InventoryStorageManagerComponent inventory =
			JWK_CompTU<SCR_InventoryStorageManagerComponent>.FindIn(playerEntity);
		if (!inventory)
			return false;

		bool ok = inventory.TrySpawnPrefabToStorage(MCD_INTEL_DOC);
		Print("[FF][MCD][INTEL] GiveIntelDocument -> " + ok);
		return ok;
	}

	// ------------------------------------------------------------------------------------------------------------------------
	//  GiveRandomHostileWeapon
	// ------------------------------------------------------------------------------------------------------------------------
	static bool GiveRandomHostileWeapon(IEntity civEntity)
	{
		SCR_InventoryStorageManagerComponent inventory =
			JWK_CompTU<SCR_InventoryStorageManagerComponent>.FindIn(civEntity);

		if (!inventory || !WEAPON_POOL() || WEAPON_POOL().IsEmpty()) return false;

		int index = Math.RandomInt(0, WEAPON_POOL().Count());
		return inventory.TrySpawnPrefabToStorage(WEAPON_POOL()[index]);
	}
}
