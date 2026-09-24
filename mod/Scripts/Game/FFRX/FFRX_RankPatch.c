// FF - REMIXED - PVE
// Galons de grade AMF poses AUTOMATIQUEMENT selon le grade reel du soldat.
//
// LE PROBLEME. Les galons sont des items d'arsenal comme les autres : n'importe qui
// pouvait prendre des galons de colonel. Le grade affiche ne voulait rien dire.
//
// LE PRINCIPE. Le grade vient de Flt_RankRegistry (persistant par UID, pilote par le
// site et par #promote/#setrank). On pose le galon correspondant sur la tenue portee,
// et on le repose a chaque changement de tenue.
//
// L'ECHELLE EST ALIGNEE 1:1 SUR LES GALONS AMF. Configs/Ranks/FIAMilitaryRanks.conf
// definit 18 grades : Deserteur (sans galon, c'est l'etat de sanction) + les 17 grades
// ADT pour lesquels AMF fournit un galon. Il n'y a volontairement PAS de "General" :
// AMF n'en fournit pas, donc le grade n'existe pas. Un grade sans galon serait un grade
// qui ne se voit pas.
//
// OU SE POSE LE GALON (releve dans les prefabs AMF) :
//   - t-shirt (Ubas)  -> slot "Sleeve_Left_Small_BottomLeftCorner_Velcro"
//   - veste F3        -> slot "Torso_Rectangle_Velcro"
// Les autres slots velcro sont pris (patronymique, groupe sanguin, insigne d'unite) :
// on n'y touche pas.
//
// BASSE ou HAUTE VISIBILITE : BV si le soldat porte un gilet de combat (LoadoutVestArea),
// HV sinon (t-shirt seul, tenue de ceremonie). Decision Benji.
//
// Serveur uniquement. Chaines ASCII.

//! Table grade -> galon. GENEREE depuis le resourceDatabase.rdb d'AMF-FANTASSIN 1.3.4
//! (68 entrees = 17 grades x 2 supports x 2 visibilites), decodage valide sur deux
//! GUID connus. Si AMF renomme ou deplace ses galons, c'est CE fichier a regenerer.
class FFRX_RankPatchTable
{
	static ResourceName UbasBV(int rank)
	{
		switch (rank) {
			case 0: return "";
			case 1: return "{E4B8FB66B83B029B}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_1CL_BV_Ubas.et";
			case 2: return "{CAD3DADFC47DC424}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Caporal_BV_Ubas.et";
			case 3: return "{A9BA6AC86B772726}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Caporal_Chef_BV_Ubas.et";
			case 4: return "{AC03B69C67C42A96}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Caporal_Chef_1CL_BV_Ubas.et";
			case 5: return "{62D798EEE1EB6601}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Sergent_BV_Ubas.et";
			case 6: return "{F8F3E03216702E97}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Sergent_Chef_BV_Ubas.et";
			case 7: return "{B00BA0F3B5BFB0BE}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Sergent_Chef_BM2_BV_Ubas.et";
			case 8: return "{81F50CEDE0CF7DB2}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Adjudant_BV_Ubas.et";
			case 9: return "{2C2CD9E3C6183161}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Adjudant_Chef_BV_Ubas.et";
			case 10: return "{6834EAB3A53BDDB0}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Major_BV_Ubas.et";
			case 11: return "{09A08487588D64E4}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Aspirant_BV_Ubas.et";
			case 12: return "{9EAB9C46307DEA41}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Sous_Lieutenant_BV_Ubas.et";
			case 13: return "{93BD96B926DFB221}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Lieutenant_BV_Ubas.et";
			case 14: return "{9CAFA9780A5DA0AE}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Capitaine_BV_Ubas.et";
			case 15: return "{3226D2CC43BF9372}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Commandant_BV_Ubas.et";
			case 16: return "{B81F3846556756C1}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Lieutenant_Colonel_BV_Ubas.et";
			case 17: return "{181A6135463085E0}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/BASSE VISIBILITE/ADT_Colonel_BV_Ubas.et";
		}
		return "";
	}

	static ResourceName UbasHV(int rank)
	{
		switch (rank) {
			case 0: return "";
			case 1: return "{90A1B9FBC811123C}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_1CL_HV_grade_Ubas.et";
			case 2: return "{CF56B906B9619EFB}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Caporal_HV_grade_Ubas.et";
			case 3: return "{1C1A46A4114506DA}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Caporal_Chef_HV_grade_Ubas.et";
			case 4: return "{917F0C29A701AAFC}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Caporal_Chef_1CL_HV_grade_Ubas.et";
			case 5: return "{0C3EF586C2047FFA}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Sergent_HV_grade_Ubas.et";
			case 6: return "{385DFE2670F1F29B}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Sergent_Chef_HV_grade_Ubas.et";
			case 7: return "{BD738DBCD1E35544}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Sergent_Chef_BM2_HV_grade_Ubas.et";
			case 8: return "{16F6C3B4050498E1}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Adjudant_HV_grade_Ubas.et";
			case 9: return "{40C300EBC4EB5B0B}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Adjudant_Chef_HV_grade_Ubas.et";
			case 10: return "{16AB9867C48CD3F4}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Major_HV_grade_Ubas.et";
			case 11: return "{F145B8D07DE82D38}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Aspirant_HV_grade_Ubas.et";
			case 12: return "{3C81D580475A65C0}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Sous_Lieutenant_HV_grade_Uba.et";
			case 13: return "{B7A4AD98C386CD44}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Lieutenant_HV_grade_Ubas.et";
			case 14: return "{681F35245072AC78}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Capitaine_HV_grade_Ubas.et";
			case 15: return "{64110AE5307C0778}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Commandant_HV_grade_Ubas.et";
			case 16: return "{06A7C1677068E93D}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Lieutenant_Colonel_HV_grade_Ubas.et";
			case 17: return "{41DC70132DB84BFF}Prefabs/Items/Equipment/Patches/F3 Ubas/Grades/ADT/HAUTE VISIBILITE/ADT_Colonel_HV_grade_Ubas.et";
		}
		return "";
	}

	static ResourceName VestBV(int rank)
	{
		switch (rank) {
			case 0: return "";
			case 1: return "{1F6F9A419884021D}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_1CL_BV_Vest.et";
			case 2: return "{C8339E27A3D1C639}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Caporal_BV_Vest.et";
			case 3: return "{0AE5CDB81119D844}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Caporal_Chef_BV_Vest.et";
			case 4: return "{BC6F9943CBBF10A7}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Caporal_Chef_1CL_BV_Vest.et";
			case 5: return "{6037DC168647641C}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Sergent_BV_Vest.et";
			case 6: return "{5BAC47426C1ED1F5}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Sergent_Chef_BV_Vest.et";
			case 7: return "{16AA66063C9619B7}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Sergent_Chef_BM2_BV_Vest.et";
			case 8: return "{1B3F5DC4DE98BF72}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Adjudant_BV_Vest.et";
			case 9: return "{7175F503CFA9C521}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Adjudant_Chef_BV_Vest.et";
			case 10: return "{E76E007AABE833A0}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Major_BV_Vest.et";
			case 11: return "{936AD5AE66DAA624}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Aspirant_BV_Vest.et";
			case 12: return "{8E946F92A3A5A3DF}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Sous_Lieutenant_BV_Vest.et";
			case 13: return "{CB5F9603B9ADCB1D}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Lieutenant_BV_Vest.et";
			case 14: return "{2ED178BD3E9C97F2}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Capitaine_BV_Vest.et";
			case 15: return "{D7AF15B0BA47945D}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Commandant_BV_Vest.et";
			case 16: return "{01AA29E16AC8E555}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Lieutenant_Colonel_BV_Vest.et";
			case 17: return "{368CD636F9D7728D}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/BASSE VISIBILITE/ADT_Colonel_BV_Vest.et";
		}
		return "";
	}

	static ResourceName VestHV(int rank)
	{
		switch (rank) {
			case 0: return "";
			case 1: return "{8B3AB1B732904D76}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_1CL_HV_Vest.et";
			case 2: return "{974267CEC5FC1E8E}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Caporal_HV_Vest.et";
			case 3: return "{2D61D2BFDD1A9A1F}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Caporal_Chef_HV_Vest.et";
			case 4: return "{1432E9B0541D3902}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Caporal_Chef_1CL_HV_Vest.et";
			case 5: return "{3F4625FFE06ABCAB}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Sergent_HV_Vest.et";
			case 6: return "{7C285845A01D93AE}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Sergent_Chef_HV_Vest.et";
			case 7: return "{BEF716F5A3343012}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Sergent_Chef_BM2_HV_Vest.et";
			case 8: return "{F6B2B0454AF0AD85}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Adjudant_HV_Vest.et";
			case 9: return "{8B3AB1B732904D77}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Adjudant_Chef_HV_Vest.et";
			case 10: return "{AE44CFE20C1DA32B}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Major_HV_Vest.et";
			case 11: return "{7EE7382FF2B2B4D3}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Aspirant_HV_Vest.et";
			case 12: return "{A27E5B8CBE25AE39}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Sous_Lieutenant_HV_Vest.et";
			case 13: return "{BDA9EFF221FF9F69}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Lieutenant_HV_Vest.et";
			case 14: return "{E61452D970FA52DF}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Capitaine_HV_Vest.et";
			case 15: return "{9014AC5F7C9D5AE2}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Commandant_HV_Vest.et";
			case 16: return "{88C153D5ADA247E9}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Lieutenant_Colonel_HV_Vest.et";
			case 17: return "{DAF411FE7084182F}Prefabs/Items/Equipment/Patches/F3 Veste/Grades/ADT/HAUTE VISIBILITE/ADT_Colonel_HV_Vest.et";
		}
		return "";
	}
}

// ---------------------------------------------------------------------------
class FFRX_RankPatch
{
	//! Slots velcro qui portent le galon (releves dans les prefabs AMF).
	static const string SLOT_UBAS = "Sleeve_Left_Small_BottomLeftCorner_Velcro";
	static const string SLOT_VEST = "Torso_Rectangle_Velcro";

	//------------------------------------------------------------------------------------------------
	//! Branche la pose au spawn. Les changements de tenue en cours de partie sont
	//! traites par le hook OnItemAdded de FFRX_JammerBatteryDrop.
	static void Boot()
	{
		if (!Replication.IsServer())
			return;

		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gm)
			return;

		gm.GetOnPlayerSpawned().Insert(OnPlayerSpawned);
		Print("[FFRX][Galon] Pose automatique des galons activee.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! La tenue de spawn n'est pas encore entierement posee quand l'event part : on
	//! laisse le loadout se terminer avant d'aller chercher le haut porte.
	protected static void OnPlayerSpawned(int playerId, IEntity player)
	{
		GetGame().GetCallqueue().CallLater(Apply, 1500, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Pose (ou remplace) le galon du joueur sur la tenue qu'il porte.
	static void Apply(int playerId)
	{
		if (!Replication.IsServer())
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		IEntity player = pm.GetPlayerControlledEntity(playerId);
		if (!player)
			return;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(player.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return;

		IEntity jacket = loadout.GetClothFromArea(LoadoutJacketArea);
		if (!jacket)
			return;   // torse nu : rien a decorer

		int rank = RankOf(playerId);

		// Gilet de combat -> galon discret ; sinon galon voyant.
		bool hasVest = loadout.GetClothFromArea(LoadoutVestArea) != null;

		// On ne sait pas a priori si le haut est un Ubas ou une veste F3 : on identifie
		// par le slot qu'il expose, plutot que par un nom de prefab (qui varierait avec
		// chaque camo).
		if (TrySlot(player, jacket, SLOT_UBAS, UbasPatch(rank, hasVest)))
			return;

		TrySlot(player, jacket, SLOT_VEST, VestPatch(rank, hasVest));
	}

	//------------------------------------------------------------------------------------------------
	protected static ResourceName UbasPatch(int rank, bool hasVest)
	{
		if (hasVest)
			return FFRX_RankPatchTable.UbasBV(rank);
		return FFRX_RankPatchTable.UbasHV(rank);
	}

	protected static ResourceName VestPatch(int rank, bool hasVest)
	{
		if (hasVest)
			return FFRX_RankPatchTable.VestBV(rank);
		return FFRX_RankPatchTable.VestHV(rank);
	}

	//------------------------------------------------------------------------------------------------
	//! Grade assigne du joueur, 0 (Deserteur) si aucun.
	protected static int RankOf(int playerId)
	{
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba)
			return 0;

		string uid = ba.GetPlayerIdentityId(playerId);
		if (uid == "")
			return 0;

		int r = Flt_RankRegistry.GetInstance().GetAssignedRank(uid);
		if (r < 0)
			return 0;
		return r;
	}

	//------------------------------------------------------------------------------------------------
	//! Remplace le contenu d'un slot nomme du vetement. \return false si le slot n'existe pas.
	//!
	//! ATTENTION : ces slots ont un Prefab par defaut qui SE RE-REMPLIT quand on les vide.
	//! C'est ce mecanisme qui avait produit la boucle infinie du jammer. On retire donc
	//! l'ancien galon et on pose le nouveau dans la FOULEE, sans laisser le slot vide.
	//------------------------------------------------------------------------------------------------
	//! Pose un galon PRECIS sur un vetement, pour un porteur qui n'est pas un joueur.
	//!
	//! `Apply()` ci-dessus part d'un `playerId` et lit le grade dans le registre Fleet ; un PNJ
	//! n'a ni l'un ni l'autre. On expose donc le geste seul. Aujourd'hui utilise par le PNJ
	//! guide (FFRX_GuideNPC.c), qui porte un galon de colonel fixe.
	//!
	//! On tente les DEUX slots : on ne sait pas a priori si le vetement est un Ubas ou une
	//! veste F3, et c'est le slot expose qui tranche -- meme raisonnement que dans `Apply()`.
	static bool FFRX_PinOn(IEntity wearer, IEntity cloth, ResourceName patch)
	{
		if (!wearer || !cloth || patch == "")
			return false;

		if (TrySlot(wearer, cloth, SLOT_UBAS, patch))
			return true;

		return TrySlot(wearer, cloth, SLOT_VEST, patch);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TrySlot(IEntity player, IEntity cloth, string slotName, ResourceName patch)
	{
		SCR_EquipmentStorageComponent equip = SCR_EquipmentStorageComponent.Cast(cloth.FindComponent(SCR_EquipmentStorageComponent));
		if (!equip)
			return false;

		int slotIndex = -1;
		int count = equip.GetSlotsCount();
		for (int i = 0; i < count; i++)
		{
			InventoryStorageSlot slot = equip.GetSlot(i);
			// GetSourceName() = le nom declare du slot dans le prefab. InventoryStorageSlot
			// n'a pas de GetName() : il herite d'EntitySlotInfo, qui expose GetSourceName().
			if (slot && slot.GetSourceName() == slotName)
			{
				slotIndex = i;
				break;
			}
		}

		if (slotIndex < 0)
			return false;   // pas ce type de vetement

		// Grade sans galon (Deserteur) : on laisse la tenue telle quelle.
		if (patch == "")
			return true;

		SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inv)
			return true;

		InventoryStorageSlot target = equip.GetSlot(slotIndex);
		IEntity current = target.GetAttachedEntity();

		// Deja le bon galon -> ne rien faire (evite de re-spawner a chaque changement).
		if (current)
		{
			EntityPrefabData pd = current.GetPrefabData();
			if (pd && pd.GetPrefabName() == patch)
				return true;

			inv.TryRemoveItemFromStorage(current, equip);
			SCR_EntityHelper.DeleteEntityAndChildren(current);
		}

		Resource res = Resource.Load(patch);
		if (!res || !res.IsValid())
			return true;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = player.GetOrigin();

		IEntity item = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!item)
			return true;

		if (!inv.TryInsertItemInStorage(item, equip, slotIndex))
			SCR_EntityHelper.DeleteEntityAndChildren(item);

		return true;
	}
}
