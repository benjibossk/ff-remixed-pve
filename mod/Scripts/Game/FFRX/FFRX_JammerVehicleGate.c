// FF - REMIXED - PVE
// Brick C -- vehicle jammer spawn gate. Add this component to an enemy vehicle base prefab that
// has a DISABLED jammer slot (an EntitySlotInfo whose bag does not spawn by default). At spawn
// it rolls the FF admin setting % and, on a hit, spawns the JammerBagStatic and attaches it to
// that slot -> only ~X% of these vehicles carry a jammer, no spawn-and-delete churn.
//
// The attached bag's SAL_DroneJammerComponent then goes situational via FFRX_JammerSituational
// (carrier = the vehicle -> auto-activates only when a hostile drone is near, range reduced
// indoors). So this component ONLY decides "does this vehicle get a jammer", nothing else.
//
// Set m_sSlotName to the exact name of the slot you created. Server-side. ASCII strings.

class FFRX_JammerSpawnGateClass : ScriptComponentClass
{
}

class FFRX_JammerSpawnGate : ScriptComponent
{
	[Attribute("Jammer", desc: "Nom EXACT du slot jammer cree sur le vehicule (SlotManagerComponent).")]
	protected string m_sSlotName;

	[Attribute("{163A6815DEEE28E3}Prefabs/Items/Equipment/Backpacks/JammerBagStatic.et", UIWidgets.ResourcePickerThumbnail, "Prefab jammer a poser dans le slot.", "et")]
	protected ResourceName m_sJammerPrefab;

	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	override void EOnInit(IEntity owner)
	{
		#ifndef WORKBENCH
		if (!Replication.IsServer())
			return;
		#endif
		// Let the vehicle's slots finish setting up before we touch them.
		GetGame().GetCallqueue().CallLater(FFRX_Roll, 1000, false, owner);
	}

	void FFRX_Roll(IEntity owner)
	{
		if (!owner)
			return;

		int pct = FFRX_Pct();
		if (Math.RandomInt(0, 100) >= pct)
		{
			Print(string.Format("[FFRX][Jammer] Vehicule -> PAS de jammer (tirage vs %1 pct).", pct), LogLevel.NORMAL);
			return;
		}

		SlotManagerComponent slotMgr = SlotManagerComponent.Cast(owner.FindComponent(SlotManagerComponent));
		if (!slotMgr)
		{
			Print("[FFRX][Jammer] Vehicule -> pas de SlotManagerComponent.", LogLevel.WARNING);
			return;
		}

		EntitySlotInfo slot = slotMgr.GetSlotByName(m_sSlotName);
		if (!slot)
		{
			Print(string.Format("[FFRX][Jammer] Vehicule -> slot '%1' introuvable (verifie le nom).", m_sSlotName), LogLevel.WARNING);
			return;
		}

		if (slot.GetAttachedEntity())
			return;   // something already in the slot

		Resource res = Resource.Load(m_sJammerPrefab);
		if (!res || !res.IsValid())
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = owner.GetOrigin();

		IEntity bag = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!bag)
			return;

		slot.AttachEntity(bag);   // snaps to the slot's predefined local transform
		// NOTE: the JammerBagStatic has a RigidBody. The slot's "merge physics" option (set on the
		// vehicle prefab) fuses that collider into the vehicle so it does NOT simulate as a loose
		// dynamic body (which caused a 1 FPS physics blow-up). If a vehicle still lags, re-check
		// that its jammer slot has merge-physics enabled.

		// Make the bag NON-PERSISTENT: it is re-rolled every session, so it must never be written
		// to the save (a bugged bag once bloated the Arland save). FF/EPF: PauseTracking().
		EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(bag.FindComponent(EPF_PersistenceComponent));
		if (persistence)
			persistence.PauseTracking();

		Print(string.Format("[FFRX][Jammer] Vehicule -> jammer POSE dans '%1' (tirage vs %2 pct).", m_sSlotName, pct), LogLevel.NORMAL);
	}

	// % of enemy vehicles that carry a jammer, from the FF admin setting (default 30).
	int FFRX_Pct()
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return 30;
		return Math.ClampInt((int)cache.m_fFFRX_JammerVehiclePct, 0, 100);
	}
}
