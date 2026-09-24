// FF - REMIXED - PVE : recharger un jammer en GLISSANT une batterie dessus.
//
// Le mod RealisticCombatDrones (comme notre FFRX_JammerReplaceBattery) ne propose que des
// actions d'inventaire. Ici on ajoute le geste naturel : on prend une batterie FPV/DJI et on
// la depose sur le sac jammer -> elle est consommee et le jammer repart.
//
// Comment : le sac a un petit stockage (voir JammerBag.et). Quand le moteur y depose un item,
// SCR_InventoryStorageManagerComponent.OnItemAdded se declenche ; si l'item porte un
// SAL_BatteryComponent et que le conteneur est un jammer, on recharge et on consomme la cellule.
// L'action d'inventaire existante est conservee : les deux gestes marchent.
//
// Autorite serveur : la consommation et la recharge ne se font que cote serveur, sinon un
// client pourrait recharger dans le vide (l'entite y est detruite localement puis resynchronisee).

modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemAdded(storageOwner, item);

		if (!storageOwner || !item)
			return;

		// --- Galon de grade : le joueur vient de changer de haut ou de gilet ----------
		// Ce hook vit ICI et pas dans FFRX_RankPatch.c parce qu'Enforce n'admet qu'UN
		// `modded class SCR_InventoryStorageManagerComponent` par addon.
		//
		// PAS DE RECURSION : on ne reagit qu'aux vetements deposes dans le stockage
		// D'EQUIPEMENT DU PERSO (EquipedLoadoutStorageComponent). Le galon, lui, est
		// insere dans le SCR_EquipmentStorageComponent DU VETEMENT -- un autre stockage,
		// qui ne repasse donc pas par ce test. C'est exactement le garde qui manquait
		// a la batterie du jammer.
		FFRX_OnPossibleClothChange(storageOwner, item);

		if (!Replication.IsServer())
			return;

		// Is the dropped item a drone battery?
		SAL_BatteryComponent battery = SAL_BatteryComponent.Cast(item.FindComponent(SAL_BatteryComponent));
		if (!battery)
			return;

		// Is the container a jammer?
		IEntity bag = storageOwner.GetOwner();
		if (!bag)
			return;

		// --- BALISE GPS : meme geste, autre appareil (cf. FFRX_Beacon.c) -------------
		// Glisser une cellule sur la balise la recharge, exactement comme le jammer. Ce
		// branchement vit ICI parce que ce fichier porte le seul point d'ecoute des depots
		// d'inventaire du mod -- pas par contrainte du langage (plusieurs `modded class`
		// d'une meme classe coexistent, mesure du 2026-09-21), mais pour garder UN SEUL
		// endroit qui reagit a OnItemAdded : deux abonnements separes rendraient l'ordre
		// des reactions imprevisible.
		//
		// Balise deja pleine -> on ne consomme RIEN : la cellule reste rangee et sert de
		// rechange. Meme regle que le jammer ci-dessous, et meme garde anti-boucle.
		FFRX_BeaconComponent beacon = FFRX_Beacons.Find(bag);
		if (beacon)
		{
			if (beacon.FFRX_BatteryPct() >= 100)
				return;

			if (!Replication.IsServer())
				return;

			beacon.FFRX_Recharge(battery.m_fBatteryStorage);
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			Print("[FFRX][Balise] Rechargee par une cellule glissee dessus.", LogLevel.NORMAL);
			return;
		}

		SAL_DroneJammerComponent jammer = SAL_DroneJammerComponent.Cast(bag.FindComponent(SAL_DroneJammerComponent));
		if (!jammer)
			return;

		// Jammer deja plein -> on ne consomme RIEN. La cellule reste dans le sac et sert
		// de rechange, ce qui est le comportement attendu quand on range une batterie.
		//
		// C'est aussi LE garde anti-boucle qui manquait. Celui par EntityID (plus bas) ne
		// protege que d'une re-notification sur la MEME entite ; il est impuissant si une
		// entite NEUVE reapparait a chaque passage. C'est exactement ce qui se produisait :
		// le slot Battery de JammerBag.et porte un `Prefab` par defaut (la cellule de
		// rechange qui accompagne le sac), le moteur en reposait donc une neuve des qu'on
		// vidait le slot -> consommation en boucle a ~58/s, 32 600 lignes = 76 % du log du
		// dedie du 2026-09-10.
		//
		// Le sac demarre plein (FFRX_JammerSituational : m_fFFRXBattery = FFRX_BATTERY_MAX),
		// donc la cellule de rechange n'est jamais mangee a l'apparition. Et si le composant
		// jammer n'est pas encore initialise, FFRX_BatteryPct() renvoie 100 ("assume full") :
		// le garde tient quel que soit l'ordre d'init.
		if (jammer.FFRX_BatteryPct() >= 100)
			return;

		// ⚠️ GARDE ANTI-BOUCLE (indispensable).
		//
		// Constate sur le dedie : 1 231 386 recharges pour une seule batterie, un log de
		// 237 Mo. OnItemAdded se redeclenche sur la MEME cellule -- suppression differee
		// qui n'aboutit pas, item remis en stockage, ou re-notification a la replication.
		// Chaque passage rechargeait le jammer et reprogrammait une suppression, en boucle.
		//
		// On marque donc la cellule comme consommee AVANT de la traiter, et on refuse tout
		// second passage. On indexe par EntityID et non par IEntity : garder une reference
		// sur une entite qu'on s'apprete a detruire l'empecherait d'etre liberee.
		if (!s_mFFRXConsumed)
			s_mFFRXConsumed = new map<EntityID, bool>();

		EntityID id = item.GetID();
		if (s_mFFRXConsumed.Contains(id))
			return;
		s_mFFRXConsumed.Set(id, true);

		// Sortir la cellule du stockage AVANT de la detruire : sans ca le moteur garde une
		// entree pointant sur une entite morte, et peut re-notifier l'ajout.
		TryRemoveItemFromStorage(item, storageOwner);

		FFRX_ConsumeBatteryInto(jammer, item, battery.m_fBatteryStorage);
	}

	//! Cellules deja consommees. Statique : la garde doit survivre au composant, chaque
	//! personnage ayant le sien.
	protected static ref map<EntityID, bool> s_mFFRXConsumed;

	//------------------------------------------------------------------------------------------------
	//! Deferred: deleting the entity inside the inventory callback would pull the rug from under
	//! the engine's own insertion bookkeeping. One frame later is safe.
	protected void FFRX_ConsumeBatteryInto(SAL_DroneJammerComponent jammer, IEntity battery, float capacity)
	{
		if (!jammer || !battery)
			return;

		jammer.FFRX_Recharge(capacity);
		GetGame().GetCallqueue().CallLater(FFRX_DeleteBattery, 1, false, battery);

		Print("[FFRX][Jammer] Batterie glissee sur le jammer -> recharge.");
	}

	//------------------------------------------------------------------------------------------------
	//! Repose le galon quand le joueur enfile un haut ou un gilet.
	protected void FFRX_OnPossibleClothChange(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		if (!Replication.IsServer())
			return;

		// Seulement l'equipement PORTE : un vetement range dans un sac ne compte pas.
		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(storageOwner);
		if (!loadout)
			return;

		if (!BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent)))
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		int pid = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(owner);
		if (pid <= 0)
			return;   // IA : pas de galon pilote par le registre

		// Differe d'une frame : le moteur est encore en train de finir l'habillage, et
		// poser un item dans un sous-stockage pendant son propre callback d'insertion
		// lui tire le tapis sous les pieds (meme raison que la suppression differee
		// de la batterie ci-dessus).
		GetGame().GetCallqueue().CallLater(FFRX_ApplyRankPatch, 1, false, pid);
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_ApplyRankPatch(int playerId)
	{
		FFRX_RankPatch.Apply(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_DeleteBattery(IEntity battery)
	{
		if (battery)
			SCR_EntityHelper.DeleteEntityAndChildren(battery);
	}
}
