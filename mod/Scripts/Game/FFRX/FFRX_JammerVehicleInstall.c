// FF - REMIXED - PVE
// Brique D -- installer SON sac jammer sur un vehicule, et le reprendre.
//
// Le vehicule porte deja un slot nomme (RegisteringComponentSlotInfo "Jammer", MergePhysics 1),
// celui que FFRX_JammerSpawnGate remplit sur les vehicules ennemis. Ici on ne cree rien : on
// deplace le sac que le joueur porte SUR SON DOS vers ce slot, et inversement.
//
// Pourquoi deplacer l'ENTITE et pas spawner un prefab neuf : l'autonomie vit dans le
// SAL_DroneJammerComponent du sac (m_fFFRXBattery, gere par FFRX_JammerSituational). En
// transportant l'entite telle quelle, la charge, la cellule de rechange rangee dedans et
// l'usure suivent le sac. Un spawn neuf redonnerait 100 % a chaque pose -- batterie infinie.
//
// Une fois attache, le sac est un porteur "vehicule" pour FFRX_JammerSituational : il passe
// en mode automatique (allumage situationnel a l'approche d'un drone hostile), exactement
// comme les jammers montes sur les vehicules ennemis. Le systeme de batterie continue de
// tourner : recharge par glisser-deposer (FFRX_JammerBatteryDrop) ou par l'action
// d'inventaire (FFRX_JammerReplaceBattery), les deux ne testent que la presence du
// SAL_DroneJammerComponent sur le conteneur.
//
// Serveur uniquement pour la partie qui deplace vraiment le sac. Chaines ASCII.

class FFRX_JammerVehicleUtil
{
	//------------------------------------------------------------------------------------------------
	//! Le slot jammer d'un vehicule, ou null s'il n'en a pas.
	static EntitySlotInfo FindSlot(IEntity vehicle, string slotName)
	{
		if (!vehicle)
			return null;

		SlotManagerComponent slotMgr = SlotManagerComponent.Cast(vehicle.FindComponent(SlotManagerComponent));
		if (!slotMgr)
			return null;

		return slotMgr.GetSlotByName(slotName);
	}

	//------------------------------------------------------------------------------------------------
	//! Le sac a dos porte par un perso s'il s'agit bien d'un jammer, sinon null.
	static IEntity WornJammerBag(IEntity user)
	{
		if (!user)
			return null;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(user.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return null;

		IEntity backpack = loadout.GetClothFromArea(LoadoutBackpackArea);
		if (!backpack)
			return null;

		if (!SAL_DroneJammerComponent.Cast(backpack.FindComponent(SAL_DroneJammerComponent)))
			return null;

		return backpack;
	}

	//------------------------------------------------------------------------------------------------
	//! Retour serveur -> client proprietaire, dans le style des hints FF.
	static void Notify(IEntity user, string text)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		int pid = pm.GetPlayerIdFromControlledEntity(user);
		if (pid <= 0)
			return;

		PlayerController pc = pm.GetPlayerController(pid);
		if (!pc)
			return;

		JWK_PlayerControllerComponent jpc = JWK_PlayerControllerComponent.Cast(pc.FindComponent(JWK_PlayerControllerComponent));
		if (jpc)
			jpc.FFRX_Popup(text);
	}

	//------------------------------------------------------------------------------------------------
	//! true si on execute sur l'autorite (PerformAction est diffuse partout).
	static bool IsAuthority(IEntity owner)
	{
		RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
		return !(rpl && rpl.IsProxy());
	}

	//------------------------------------------------------------------------------------------------
	//! Pose le sac sur le HAUT-ARRIERE de la carrosserie, calcule depuis la boite englobante.
	//!
	//! Le slot vit sur Vehicle_Base, donc sa transformation est forcement generique : une
	//! valeur fixe mettrait le sac dans le chassis d'un char et au-dessus du toit d'une jeep.
	//! On la recalcule donc par vehicule avant d'attacher. Les 6 vehicules ennemis qui ont
	//! leur PROPRE slot Jammer place a la main gardent le leur : ce calcul ne s'applique
	//! qu'au slot generique, reconnaissable a son offset non personnalise.
	static void PlaceOnHull(IEntity vehicle, EntitySlotInfo slot)
	{
		if (!vehicle || !slot)
			return;

		vector mins, maxs;
		vehicle.GetBounds(mins, maxs);

		// Centre en X, sur le toit (Y max), au tiers arriere (Z min cote arriere du modele).
		vector pos;
		pos[0] = (mins[0] + maxs[0]) * 0.5;
		pos[1] = maxs[1];
		pos[2] = mins[2] + (maxs[2] - mins[2]) * 0.25;

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = pos;
		slot.OverrideTransformLS(mat);
	}
}

// ---------------------------------------------------------------------------
//  Poser son sac jammer dans le slot du vehicule.
// ---------------------------------------------------------------------------
class FFRX_JammerInstallAction : ScriptedUserAction
{
	[Attribute("Jammer", desc: "Nom EXACT du slot jammer du vehicule (SlotManagerComponent).")]
	protected string m_sSlotName;

	//------------------------------------------------------------------------------------------------
	//! Le slot Jammer est desormais sur Vehicle_Base, donc TOUS les vehicules l'ont. Sans
	//! filtre, chaque joueur verrait l'entree sur chaque vehicule. On ne la montre donc
	//! qu'a quelqu'un qui porte reellement un sac brouilleur, et si le slot est libre.
	//! Le port du sac est un etat d'inventaire replique : lisible client ET serveur.
	override bool CanBeShownScript(IEntity user)
	{
		EntitySlotInfo slot = FFRX_JammerVehicleUtil.FindSlot(GetOwner(), m_sSlotName);
		if (!slot || slot.GetAttachedEntity())
			return false;

		return FFRX_JammerVehicleUtil.WornJammerBag(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Revalide quand meme : l'autorite ne fait jamais confiance a un etat lu cote client.
	override bool CanBePerformedScript(IEntity user)
	{
		return FFRX_JammerVehicleUtil.WornJammerBag(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Installer le brouilleur sur le vehicule";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!FFRX_JammerVehicleUtil.IsAuthority(pOwnerEntity))
			return;

		EntitySlotInfo slot = FFRX_JammerVehicleUtil.FindSlot(pOwnerEntity, m_sSlotName);
		if (!slot)
			return;

		if (slot.GetAttachedEntity())
		{
			FFRX_JammerVehicleUtil.Notify(pUserEntity, "Ce vehicule porte deja un brouilleur.");
			return;
		}

		IEntity bag = FFRX_JammerVehicleUtil.WornJammerBag(pUserEntity);
		if (!bag)
		{
			FFRX_JammerVehicleUtil.Notify(pUserEntity, "Il faut porter un sac brouilleur sur le dos.");
			return;
		}

		// Sortir le sac de l'equipement AVANT de l'attacher : tant qu'il appartient au
		// stockage du perso, le moteur continue de le suivre comme un vetement porte.
		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(pUserEntity.FindComponent(EquipedLoadoutStorageComponent));
		SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!loadout || !inv || !inv.TryRemoveItemFromStorage(bag, loadout))
		{
			FFRX_JammerVehicleUtil.Notify(pUserEntity, "Impossible de retirer le sac brouilleur.");
			return;
		}

		// Le slot generique de Vehicle_Base n'a pas de position utilisable : on la calcule
		// depuis la carrosserie de CE vehicule avant d'attacher.
		FFRX_JammerVehicleUtil.PlaceOnHull(pOwnerEntity, slot);

		// AttachEntity recale le sac sur la transformation locale du slot. MergePhysics
		// (regle sur le prefab du vehicule) fusionne son collider dans celui du vehicule :
		// sans ca le sac simule comme un corps dynamique libre -- c'est ce qui avait fait
		// tomber le serveur a 1 FPS, cf. FFRX_JammerVehicleGate.c.
		slot.AttachEntity(bag);

		FFRX_JammerVehicleUtil.Notify(pUserEntity, "Brouilleur installe sur le vehicule.");
		Print(string.Format("[FFRX][Jammer] Sac joueur installe sur vehicule (slot '%1').", m_sSlotName), LogLevel.NORMAL);
	}
}

// ---------------------------------------------------------------------------
//  Reprendre le sac jammer du vehicule sur son dos.
// ---------------------------------------------------------------------------
class FFRX_JammerUninstallAction : ScriptedUserAction
{
	[Attribute("Jammer", desc: "Nom EXACT du slot jammer du vehicule (SlotManagerComponent).")]
	protected string m_sSlotName;

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		EntitySlotInfo slot = FFRX_JammerVehicleUtil.FindSlot(GetOwner(), m_sSlotName);
		if (!slot)
			return false;

		return slot.GetAttachedEntity() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Il faut le dos libre : on ne peut pas porter deux sacs.
	override bool CanBePerformedScript(IEntity user)
	{
		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(user.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return false;

		return loadout.GetClothFromArea(LoadoutBackpackArea) == null;
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Reprendre le brouilleur";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!FFRX_JammerVehicleUtil.IsAuthority(pOwnerEntity))
			return;

		EntitySlotInfo slot = FFRX_JammerVehicleUtil.FindSlot(pOwnerEntity, m_sSlotName);
		if (!slot)
			return;

		IEntity bag = slot.GetAttachedEntity();
		if (!bag)
			return;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(pUserEntity.FindComponent(EquipedLoadoutStorageComponent));
		SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!loadout || !inv)
			return;

		if (loadout.GetClothFromArea(LoadoutBackpackArea))
		{
			FFRX_JammerVehicleUtil.Notify(pUserEntity, "Tu portes deja un sac : libere ton dos d'abord.");
			return;
		}

		// Detacher AVANT d'inserer : le moteur refuse de ranger une entite encore slottee.
		slot.DetachEntity();

		if (!inv.TryInsertItemInStorage(bag, loadout))
		{
			// Echec de l'equipement : on remet le sac dans le slot plutot que de le laisser
			// tomber dans le vide -- il ne doit jamais disparaitre avec sa charge.
			slot.AttachEntity(bag);
			FFRX_JammerVehicleUtil.Notify(pUserEntity, "Impossible d'equiper le sac brouilleur.");
			return;
		}

		FFRX_JammerVehicleUtil.Notify(pUserEntity, "Brouilleur repris.");
		Print(string.Format("[FFRX][Jammer] Sac repris du vehicule (slot '%1').", m_sSlotName), LogLevel.NORMAL);
	}
}
