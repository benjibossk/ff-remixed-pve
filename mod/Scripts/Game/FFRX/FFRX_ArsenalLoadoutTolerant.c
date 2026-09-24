// FFRX - restauration de tenue tolerante aux pannes.
//
// Le serialiseur du jeu de base (SCR_PlayerArsenalLoadout) abandonne TOUT le loadout
// des qu'un seul objet echoue a se poser : un accessoire d'arme introuvable, un slot
// deja occupe, un prefab d'un mod absent -> le personnage ne recoit presque rien.
// C'est la cause du "gilet tout seul" et des armes a moitie montees.
//
// On reprend ici l'approche du mod WCS Loadout Editor : au lieu de renvoyer false,
// on SAUTE l'objet fautif en avancant correctement le curseur de lecture, puis on
// continue le reste de la tenue. Le curseur est la partie delicate : si on ne consomme
// pas exactement les donnees de l'objet saute, tout ce qui suit est lu de travers.
//
// Aucune dependance externe : c'est notre propre override de l'API du jeu de base.

[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sLoadoutName")]
modded class SCR_PlayerArsenalLoadout : SCR_FactionPlayerLoadout
{
	//------------------------------------------------------------------------------------------------
	protected static void FFRX_Skipped(string message)
	{
		Print("[FFRX][Loadout] objet ignore : " + message, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	// L'equipement est deja pose quand EndObject echoue : ne pas faire echouer l'apply pour ca.
	override static bool ApplyLoadoutString(IEntity owner, LoadContext context)
	{
		if (!context.StartObject(ARSENALLOADOUT_KEY))
			return false;

		InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(owner.FindComponent(InventoryStorageManagerComponent));
		if (!manager)
			return false;

		bool entityLoadoutOk = ApplyEntityLoadoutString(owner, context, manager);

		// L'arme active n'est relue que si le reste s'est deserialise proprement, sinon
		// le curseur est a une position inconnue et on lirait n'importe quoi.
		if (entityLoadoutOk)
			ApplyCharacterDataLoadoutString(owner, context);
		else
			FFRX_Skipped("ApplyEntityLoadoutString a echoue, arme active non restauree");

		if (!context.EndObject())
			FFRX_Skipped("EndObject en fin de tenue (objets deja poses)");

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override protected static bool ApplyEntityLoadoutString(
		IEntity owner,
		LoadContext context,
		InventoryStorageManagerComponent manager,
		BaseInventoryStorageComponent parentStorage = null,
		int slotId = -1)
	{
		if (parentStorage && slotId != -1)
		{
			ResourceName prefab;
			if (!context.Read(prefab))
				return false;

			const ResourceName currentPrefab = SCR_ResourceNameUtils.GetPrefabName(owner);
			if (prefab != currentPrefab)
			{
				if (owner && !manager.TryDeleteItem(owner))
				{
					IEntity slotContents = parentStorage.Get(slotId);
					if (slotContents)
					{
						FFRX_Skipped("suppression impossible dans le slot " + slotId.ToString() + " pour " + prefab);
						FFRX_SkipEntityLoadoutData(context);
						return true;
					}
				}

				if (!parentStorage.GetOwner())
				{
					FFRX_Skipped("conteneur parent invalide apres suppression pour " + prefab);
					FFRX_SkipEntityLoadoutData(context);
					return true;
				}

				IEntity slotOccupant = parentStorage.Get(slotId);
				if (slotOccupant)
				{
					FFRX_Skipped("slot " + slotId.ToString() + " encore occupe pour " + prefab);
					FFRX_SkipEntityLoadoutData(context);
					return true;
				}

				Resource prefabResource = Resource.Load(prefab);
				if (!prefabResource || !prefabResource.IsValid())
				{
					FFRX_Skipped("prefab introuvable (mod absent ?) : " + prefab);
					FFRX_SkipEntityLoadoutData(context);
					return true;
				}

				if (!manager.TrySpawnPrefabToStorage(prefab, parentStorage, slotId))
				{
					FFRX_Skipped("spawn refuse pour " + prefab + " slot " + slotId.ToString());
					FFRX_SkipEntityLoadoutData(context);
					return true;
				}

				owner = parentStorage.Get(slotId);
				if (!owner)
				{
					FFRX_Skipped("spawn annonce ok mais slot vide pour " + prefab);
					FFRX_SkipEntityLoadoutData(context);
					return true;
				}

				// Sans InventoryItemComponent, l'objet fait planter l'ouverture de l'inventaire.
				if (!owner.FindComponent(InventoryItemComponent))
				{
					FFRX_Skipped("objet sans InventoryItemComponent : " + prefab);
					manager.TryDeleteItem(owner);
					FFRX_SkipEntityLoadoutData(context);
					return true;
				}
			}
		}

		if (!ApplyEntityCustomDataString(owner, context))
			return false;

		return ApplyEntityStorageString(owner, context, manager, parentStorage, slotId);
	}

	//------------------------------------------------------------------------------------------------
	override protected static bool ApplyEntityStorageString(
		IEntity owner,
		LoadContext context,
		InventoryStorageManagerComponent manager,
		BaseInventoryStorageComponent parentStorage = null,
		int slotId = -1)
	{
		int storageCount;
		context.StartArray("storages", storageCount);
		if (storageCount == 0)
			return true;

		set<BaseInventoryStorageComponent> storageCandidates();
		FindStorageComponents(owner, storageCandidates);

		for (int nStorage = 0; nStorage < storageCount; ++nStorage)
		{
			if (!context.StartObject())
				return false;

			string id;
			if (!context.Read(id))
				return false;

			BaseInventoryStorageComponent storage;
			foreach (BaseInventoryStorageComponent candidate : storageCandidates)
			{
				const string componentId = GetComponentIdentifier(owner, candidate);
				if (componentId == id)
				{
					storage = candidate;
					break;
				}
			}

			if (!storage)
			{
				FFRX_Skipped("conteneur absent sur l'entite : " + id);
				FFRX_SkipStorageSlots(context);
				if (!context.EndObject())
					return false;

				continue;
			}

			map<int, IEntity> slottedItems = new map<int, IEntity>();
			array<InventoryItemComponent> itemComponents = {};
			storage.GetOwnedItems(itemComponents, false);
			foreach (InventoryItemComponent item : itemComponents)
			{
				slottedItems.Insert(item.GetParentSlot().GetID(), item.GetOwner());
			}

			int slotCount = 0;
			if (!context.StartMap("slots", slotCount))
				return false;

			for (int i = 0; i < slotCount; ++i)
			{
				string idxStr;
				if (!context.ReadMapKey(i, idxStr))
					return false;

				const int childSlotId = idxStr.ToInt(-1);
				if (childSlotId == -1)
					return false;

				IEntity existing;
				slottedItems.Take(childSlotId, existing);

				if (!context.StartObject(idxStr))
					return false;

				// L'appel imbrique avance deja le curseur sur ses propres chemins d'echec.
				if (!ApplyEntityLoadoutString(existing, context, manager, storage, childSlotId))
					FFRX_Skipped("echec imbrique sur le slot " + childSlotId.ToString());

				if (!context.EndObject())
					return false;
			}

			if (!context.EndMap())
				return false;

			// Retirer ce qui restait dans un slot absent de la tenue enregistree.
			foreach (int idx, IEntity entity : slottedItems)
			{
				if (entity && !manager.TryDeleteItem(entity))
					FFRX_Skipped("reste non supprimable dans le slot " + idx.ToString());
			}

			if (!context.EndObject())
				return false;
		}

		return context.EndArray();
	}

	//------------------------------------------------------------------------------------------------
	// Consomme les donnees d'un objet saute, dans le meme ordre que l'ecriture
	// (donnees custom -> conteneurs). A appeler APRES avoir lu le prefab.
	protected static bool FFRX_SkipEntityLoadoutData(LoadContext context)
	{
		ApplyEntityCustomDataString(null, context);
		return FFRX_SkipEntityStorageData(context);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FFRX_SkipEntityStorageData(LoadContext context)
	{
		int storageCount;
		context.StartArray("storages", storageCount);
		if (storageCount == 0)
			return true;

		for (int nStorage = 0; nStorage < storageCount; ++nStorage)
		{
			if (!context.StartObject())
				return false;

			string id;
			if (!context.Read(id))
				return false;

			if (!FFRX_SkipStorageSlots(context))
				return false;

			if (!context.EndObject())
				return false;
		}

		return context.EndArray();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool FFRX_SkipStorageSlots(LoadContext context)
	{
		int slotCount;
		if (!context.StartMap("slots", slotCount))
			return false;

		for (int i = 0; i < slotCount; ++i)
		{
			string idxStr;
			if (!context.ReadMapKey(i, idxStr))
				return false;

			if (!context.StartObject(idxStr))
				return false;

			ResourceName prefab;
			if (!context.Read(prefab))
				return false;

			if (!FFRX_SkipEntityLoadoutData(context))
				return false;

			if (!context.EndObject())
				return false;
		}

		return context.EndMap();
	}
}
