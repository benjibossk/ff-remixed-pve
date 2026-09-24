// FF - REMIXED - PVE
// Inventory action for the jammer bags (JammerBag / JammerBagStatic). Add it to the bag prefab's
// ActionsManagerComponent -> additionalActions (next to SAL_ToggleJammerCharacter).
//
// It consumes one drone battery (SAL_BatteryComponent item) from the user's inventory and refills
// the jammer's charge via FFRX_Recharge() on the (modded) SAL_DroneJammerComponent. Same shared
// battery resource as the drones, so batteries become a real logistics item.
//
// Server-authoritative delete + recharge. ASCII strings.

class FFRX_JammerReplaceBattery : SCR_InventoryAction
{
	protected IEntity m_eBag;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_eBag = pOwnerEntity;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// pOwnerEntity = the jammer bag ; pUserEntity = the player.
		if (!pOwnerEntity || !pUserEntity)
			return;

		SAL_DroneJammerComponent jammer = SAL_DroneJammerComponent.Cast(pOwnerEntity.FindComponent(SAL_DroneJammerComponent));
		if (!jammer)
			return;

		IEntity battery = FFRX_FindBattery(pUserEntity);
		if (!battery)
			return;

		float cap = 0;
		SAL_BatteryComponent bc = SAL_BatteryComponent.Cast(battery.FindComponent(SAL_BatteryComponent));
		if (bc)
			cap = bc.m_fBatteryStorage;

		jammer.FFRX_Recharge(cap);
		SCR_EntityHelper.DeleteEntityAndChildren(battery);   // consume the cell
	}

	//------------------------------------------------------------------------------------------------
	// Show the action only when the user carries a compatible (SAL) battery.
	override bool CanBePerformedScript(IEntity user)
	{
		return FFRX_FindBattery(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		int pct = 100;
		if (m_eBag)
		{
			SAL_DroneJammerComponent j = SAL_DroneJammerComponent.Cast(m_eBag.FindComponent(SAL_DroneJammerComponent));
			if (j)
				pct = j.FFRX_BatteryPct();
		}
		outName = string.Format("Changer la batterie du jammer (%1 pct)", pct);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity FFRX_FindBattery(IEntity user)
	{
		if (!user)
			return null;

		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(user.FindComponent(InventoryStorageManagerComponent));
		if (!inv)
			return null;

		array<IEntity> items = {};
		inv.GetItems(items);
		foreach (IEntity it : items)
		{
			if (it && it.FindComponent(SAL_BatteryComponent))
				return it;
		}
		return null;
	}
}
