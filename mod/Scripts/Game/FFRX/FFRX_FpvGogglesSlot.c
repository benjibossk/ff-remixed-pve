// FF - REMIXED - PVE
// The FPV goggles were moved from the HEADCOVER (helmet) slot to the GOGGLES (eyewear) slot
// via the FPV_Goggles.et prefab override, so a player can wear a helmet AND the goggles.
//
// The RealisticCombatDrones mod (SAL_) hard-codes storage slot index 0 (= headgear) in three
// "is the player still wearing the FPV goggles?" checks. Once the goggles live in the eyewear
// slot, Get(0) returns the HELMET, so those checks fail and the player can get dropped out of
// the drone view. We override the three spots to test the goggles slot-agnostically (scan all
// loadout slots for the FPV_Goggles prefab).
//
// The PRIMARY enter/exit trigger (HandleOnItemAddedToInventory) already uses FindItemSlot and
// is slot-agnostic, so it is left untouched.
//
// Load order: base -> RealisticCombatDrones -> AIUseDrones -> JamAiDrones -> REMIXED, so these
// modded overrides sit below SAL's in the chain and override cleanly. ASCII in strings.

class FFRX_FpvGoggles
{
	static const ResourceName GOGGLES = "{8951045BFE8BC8E4}Prefabs/Characters/HeadGear/FPV_Goggles.et";

	// True if the character has the FPV goggles equipped in ANY loadout slot (helmet, eyewear,
	// whatever). Replaces the mod's hard-coded storage.Get(0) == goggles test.
	static bool IsWorn(IEntity character)
	{
		if (!character)
			return false;

		SCR_CharacterInventoryStorageComponent storage = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!storage)
			return false;

		int count = storage.GetSlotsCount();
		for (int i = 0; i < count; i++)
		{
			IEntity it = storage.Get(i);
			if (!it)
				continue;

			if (it.GetPrefabData() && it.GetPrefabData().GetPrefabName() == GOGGLES)
				return true;
		}

		return false;
	}
}

modded class SCR_PlayerController
{
	// Auto-enter the drone when connecting while the goggles are already worn.
	// Original tested storage.Get(0) == goggles; now slot-agnostic.
	override void EnterDroneTest()
	{
		if (FFRX_FpvGoggles.IsWorn(GetLocalControlledEntity()))
			EnterDrone(SAL_DroneConnectionManager.GetInstance().GetPlayersDroneRplId(SCR_PlayerController.GetLocalPlayerId()));
	}
}

modded class SCR_EditorManagerEntity
{
	// SAL reopens the drone view when the editor (drone camera) closes IF the goggles are still
	// worn, but it tested storage.Get(0). We let SAL run (its OPEN branch is fine, its CLOSE
	// goggle test now just fails silently), then add the CLOSE reopen with a slot-agnostic test.
	override void StartEvents(EEditorEventOperation type = EEditorEventOperation.NONE)
	{
		super.StartEvents(type);

		if (type != EEditorEventOperation.CLOSE)
			return;

		SAL_DroneConnectionManager droneManager = SAL_DroneConnectionManager.GetInstance();
		if (!droneManager)
			return;

		if (!SCR_PlayerController.GetLocalControlledEntity())
			return;

		if (!droneManager.IsPlayerDroneOwner(SCR_PlayerController.GetLocalPlayerId()))
			return;

		if (FFRX_FpvGoggles.IsWorn(SCR_PlayerController.GetLocalControlledEntity()))
			GetGame().GetCallqueue().CallLater(OpenDrone, 1000, false);
	}
}

modded class NOVA_Drons
{
	// The AI drone operator also wears the FPV goggles (cosmetic). The stock EquipGoggles()
	// removed + deleted the operator's first cloth item (assumed "the helmet") to FREE the
	// headcover slot, then equipped the goggles there. Now the goggles use the EYEWEAR slot, so
	// there is nothing to free: we spawn the goggles straight into the free eyewear slot and keep
	// the helmet on. We never set m_sSavedHelmetPrefab, so the stock RemoveGogglesAndRestoreHelmet
	// just removes the goggles and skips the (empty) helmet restore -- no second override needed.
	override protected void EquipGoggles()
	{
		if (!m_Owner)
			return;

		SCR_InventoryStorageManagerComponent storageMgr = SCR_InventoryStorageManagerComponent.Cast(m_Owner.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!storageMgr)
			return;

		SCR_CharacterInventoryStorageComponent characterStorage = SCR_CharacterInventoryStorageComponent.Cast(m_Owner.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!characterStorage)
			return;

		if (m_sPrefabFPVGoggles.IsEmpty())
			return;

		Resource gogglesRes = Resource.Load(m_sPrefabFPVGoggles);
		if (!gogglesRes)
			return;

		if (!gogglesRes.IsValid())
			return;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[3] = m_Owner.GetOrigin();

		IEntity goggles = GetGame().SpawnEntityPrefab(gogglesRes, m_World, spawnParams);
		if (!goggles)
			return;

		bool inserted = storageMgr.TryInsertItemInStorage(goggles, characterStorage);
		if (!inserted)
			SCR_EntityHelper.DeleteEntityAndChildren(goggles);
	}
}

modded class SCR_CharacterInventoryStorageComponent
{
	// Re-enter the drone when a drone item is removed but the goggles are still worn.
	// Original tested this.Get(0) == goggles; now slot-agnostic. Fully reimplemented (the first
	// block -- goggles removed -> ExitDrone -- is unchanged from SAL).
	override void GoggleCheck(IEntity item, BaseInventoryStorageComponent storageOwner)
	{
		if (!item)
			return;

		if (item.GetPrefabData() && item.GetPrefabData().GetPrefabName() == FFRX_FpvGoggles.GOGGLES)
			if (!InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent)).GetParentSlot())
				SCR_PlayerController.Cast(GetGame().GetPlayerController()).ExitDrone();

		if (item.FindComponent(SAL_DroneControllerComponent))
		{
			if (SAL_DroneConnectionManager.GetInstance().IsDronePlayers(item))
				if (FFRX_FpvGoggles.IsWorn(SCR_PlayerController.GetLocalControlledEntity()))
					SCR_PlayerController.Cast(GetGame().GetPlayerController()).EnterDrone(SAL_DroneConnectionManager.GetInstance().GetPlayersDroneRplId(SCR_PlayerController.GetLocalPlayerId()));
		}
	}
}
