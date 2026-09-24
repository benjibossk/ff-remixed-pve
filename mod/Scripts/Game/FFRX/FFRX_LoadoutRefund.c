// FF - REMIXED - PVE
// Action de caisse : "Rendre son equipement" -- le joueur rend tout son materiel et
// recupere sa valeur en ravitaillement dans le depot voisin.
//
// POURQUOI : jusqu'ici l'equipement etait a sens unique. Un joueur qui avait pris une
// dotation lourde pour un assaut annule, ou qui change de role, n'avait aucun moyen de
// restituer son materiel : les supplies etaient perdus pour l'escouade. C'est punitif
// et ca pousse a garder du materiel inutilise "au cas ou", au detriment du groupe.
//
// CE QU'ON GARDE : la TENUE DE BASE -- haut, pantalon, chaussures. Le joueur repart
// habille comme au spawn plutot qu'en sous-vetements : c'est plus digne, et surtout
// ca evite qu'il reste bloque a poil s'il n'a pas de quoi se rehabiller tout de suite.
//
// ------------------------------------------------------------------------------------
// COMMENT ON IDENTIFIE CE QU'ON GARDE
//
// PAR ZONE DE TENUE (BaseLoadoutClothComponent.GetAreaType), jamais par nom de prefab.
// Un pantalon reste un pantalon quel que soit son camo, son theatre ou son mod.
//
// ⚠️ La version precedente gardait le pantalon et, pour le torse, seulement les
// t-shirts d'une LISTE DE PREFABS figee. Cette liste s'est perimee en silence : la
// tenue de spawn reelle est un ensemble militaire (Ubas F3, pantalon F3, rangers
// Haix), dont aucune piece n'y figurait -- et les chaussures n'etaient pas gardees du
// tout. Resultat en jeu : le joueur ressortait EN PANTALON, pieds nus et torse nu.
// Ne pas revenir a une liste de prefabs : elle recassera au prochain uniforme ajoute.
//
// Effet de bord assume : une veste de combat plus chere que la tenue de base est
// CONSERVEE, donc non remboursee (on ne credite que la difference avant/apres). C'est
// le prix a payer pour ne jamais laisser un joueur deshabille.
//
// ------------------------------------------------------------------------------------
// AUTORITE : tout se fait cote serveur. Le remboursement credite le meme depot que
// celui qui facture les dotations (FFRX_LoadoutAction), pour que l'operation soit
// symetrique : ce qui a ete preleve la revient la.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

class FFRX_RefundLoadoutAction : ScriptedUserAction
{
	[Attribute("80", UIWidgets.EditBox, "Rayon (m) de recherche du depot a crediter.")]
	protected float m_fSupplyRadius;

	protected ref array<JWK_LogisticsStorageControllerComponent> m_aScan;

	//------------------------------------------------------------------------------------
	// DIAGNOSTIC : voir FFRX_SaveLoadoutAction -- meme raisonnement, meme verrou statique
	// (CanBeShownScript est appele a chaque frame ou l'on vise la caisse).
	protected static bool s_bRefundShownLogged;

	override bool CanBeShownScript(IEntity user)
	{
		if (!s_bRefundShownLogged)
		{
			s_bRefundShownLogged = true;
			Print("[FFRX][Actions] RefundLoadoutAction PRESENTE sur la caisse visee.", LogLevel.NORMAL);
		}
		return true;
	}
	override bool CanBePerformedScript(IEntity user) { return true; }

	override bool GetActionNameScript(out string outName)
	{
		// DIAGNOSTIC : voir FFRX_SaveLoadoutAction.GetActionNameScript.
		if (!s_bRefundShownLogged)
		{
			s_bRefundShownLogged = true;
			Print("[FFRX][Actions] RefundLoadoutAction PRESENTE sur la caisse visee.", LogLevel.NORMAL);
		}

		outName = "Rendre son equipement (remboursement)";
		return true;
	}

	//------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// PerformAction est diffusee partout : seule l'autorite travaille.
		RplComponent rpl = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		if (rpl && rpl.IsProxy())
			return;

		if (!pUserEntity)
			return;

		// 1) Valeur AVANT : c'est ce que vaut la tenue complete.
		int before = FFRX_LoadoutValue(pUserEntity);
		if (before < 0)
		{
			FFRX_Notify(pUserEntity, "Impossible d'evaluer l'equipement.");
			return;
		}

		// 2) Retirer tout sauf pantalon + t-shirt.
		int removed = FFRX_StripKeepingBasics(pUserEntity);
		if (removed == 0)
		{
			FFRX_Notify(pUserEntity, "Rien a rendre.");
			return;
		}

		// 3) Valeur APRES : ce qui reste sur le dos (pantalon + t-shirt) n'est pas
		//    rembourse, puisqu'on le garde. Le credit est la DIFFERENCE, pas la valeur
		//    initiale -- sinon on paierait deux fois ce que le joueur conserve.
		int after = FFRX_LoadoutValue(pUserEntity);
		if (after < 0) after = 0;

		int refund = before - after;
		if (refund < 0) refund = 0;

		// 4) Crediter le depot voisin (jamais la caisse elle-meme).
		JWK_LogisticsStorageControllerComponent store = FFRX_FindSupply(pOwnerEntity.GetOrigin(), pOwnerEntity);
		int credited = 0;
		if (store && refund > 0)
		{
			int cap = store.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES);
			int cur = store.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
			credited = refund;
			// Ne pas depasser la capacite : le surplus est perdu, et on le DIT.
			if (cap > 0 && cur + credited > cap)
				credited = cap - cur;
			if (credited < 0) credited = 0;
			if (credited > 0)
				store.AddResources(JWK_ELogisticsResourceType.SUPPLIES, credited);
		}

		// 5) Remettre a zero ce que le joueur avait "investi" : il ne porte plus rien,
		//    sa prochaine dotation doit etre facturee en entier. Sans ca il serait
		//    credite deux fois (une fois ici, une fois par le rendu de FFRX_LoadoutAction).
		FFRX_ResetSunkCost(pUserEntity);

		string msg;
		if (!store)
			msg = string.Format("Equipement rendu (%1 objets), mais aucun depot ici : %2 ravito perdus.", removed, refund);
		else if (credited < refund)
			msg = string.Format("Equipement rendu (%1 objets) : +%2 ravito (depot plein, %3 perdus).", removed, credited, refund - credited);
		else
			msg = string.Format("Equipement rendu (%1 objets) : +%2 ravito.", removed, credited);

		FFRX_Notify(pUserEntity, msg);
		Print(string.Format("[FFRX][Refund] %1 objets rendus, valeur %2, credite %3.", removed, refund, credited), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------
	//! Valeur en ravitaillement de la tenue portee, -1 si l'arsenal est indisponible.
	protected int FFRX_LoadoutValue(IEntity character)
	{
		GameEntity ge = GameEntity.Cast(character);
		if (!ge) return -1;
		SCR_ArsenalManagerComponent mgr;
		if (!SCR_ArsenalManagerComponent.GetArsenalManager(mgr) || !mgr) return -1;
		return mgr.GetCharacterLoadoutSupplyCost(ge, false);
	}

	//------------------------------------------------------------------------------------
	//! Retire armes et vetements, sauf pantalon et t-shirt. Renvoie le nombre d'objets retires.
	protected int FFRX_StripKeepingBasics(IEntity character)
	{
		SCR_InventoryStorageManagerComponent inv =
			SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inv) return 0;

		int n = 0;
		n += FFRX_StripStorage(EquipedWeaponStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent)), inv, false);
		n += FFRX_StripStorage(EquipedLoadoutStorageComponent.Cast(character.FindComponent(EquipedLoadoutStorageComponent)), inv, true);
		return n;
	}

	//! `checkKeep` : n'appliquer la regle pantalon/t-shirt qu'aux vetements.
	protected int FFRX_StripStorage(BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent inv, bool checkKeep)
	{
		if (!storage || !inv) return 0;

		int removed = 0;
		int count = storage.GetSlotsCount();
		// A REBOURS : retirer un objet reindexe les slots suivants.
		for (int i = count - 1; i >= 0; i--)
		{
			IEntity item = storage.Get(i);
			if (!item) continue;
			if (checkKeep && FFRX_ShouldKeep(item)) continue;

			inv.TryRemoveItemFromStorage(item, storage, null);
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			removed++;
		}
		return removed;
	}

	//! Vrai pour ce qui constitue la TENUE DE BASE : haut, pantalon, chaussures.
	//!
	// ------------------------------------------------------------------------------------
	// POURQUOI CETTE REGLE A CHANGE (2026-09-14)
	//
	// L'ancienne version gardait le pantalon, et le torse UNIQUEMENT s'il s'agissait d'un
	// t-shirt d'une liste figee de prefabs. Elle ne gardait pas les chaussures du tout.
	// Elle datait d'un spawn de type civil ; ce n'est plus le cas.
	//
	// La tenue de spawn reelle (Configs/Factions/Utils/Loadouts/Loadout_FIA_DESERT.conf)
	// est un ensemble militaire complet : haut F3 Ubas, pantalon F3, rangers Haix, beret.
	// Aucune de ces pieces n'etait dans la liste -> le joueur ressortait du remboursement
	// EN PANTALON, pieds nus et torse nu. C'est le defaut remonte en jeu.
	//
	// On garde donc les trois zones vestimentaires de base, PAR ZONE et non par liste de
	// prefabs : ca survit a un changement de camo, de theatre ou de mod, alors qu'une
	// liste figee se perime en silence a chaque nouvelle tenue -- c'est exactement ce qui
	// vient d'arriver.
	//
	// ⚠️ Effet de bord assume : un joueur qui porte une veste de combat plus chere que la
	// tenue de base la CONSERVE, et n'en est donc pas rembourse (on ne credite que la
	// difference avant/apres). C'est le prix a payer pour ne jamais le laisser deshabille.
	// L'alternative -- tout retirer puis rhabiller avec les prefabs exacts du spawn --
	// demanderait de relire la config de loadout FF a l'execution, pour un gain faible.
	protected bool FFRX_ShouldKeep(IEntity item)
	{
		BaseLoadoutClothComponent cloth = BaseLoadoutClothComponent.Cast(item.FindComponent(BaseLoadoutClothComponent));
		if (!cloth) return false;

		LoadoutAreaType area = cloth.GetAreaType();
		if (!area) return false;

		if (area.IsInherited(LoadoutPantsArea))
			return true;    // pantalon

		if (area.IsInherited(LoadoutJacketArea))
			return true;    // haut : t-shirt comme veste de treillis

		if (area.IsInherited(LoadoutBootsArea))
			return true;    // chaussures -- oubliees dans l'ancienne regle

		return false;
	}

	protected string FFRX_PrefabOf(IEntity e)
	{
		if (!e) return "";
		EntityPrefabData pd = e.GetPrefabData();
		if (!pd) return "";
		return pd.GetPrefabName();
	}

	//------------------------------------------------------------------------------------
	//! Depot le plus proche AVEC capacite, jamais la caisse elle-meme.
	protected JWK_LogisticsStorageControllerComponent FFRX_FindSupply(vector pos, IEntity exclude)
	{
		m_aScan = new array<JWK_LogisticsStorageControllerComponent>();
		World world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, m_fSupplyRadius, FFRX_ScanStorage, null, EQueryEntitiesFlags.ALL);

		JWK_LogisticsStorageControllerComponent best;
		float bestD = float.MAX;
		foreach (JWK_LogisticsStorageControllerComponent st : m_aScan)
		{
			if (!st || !st.GetOwner()) continue;
			if (exclude && st.GetOwner() == exclude) continue;
			if (st.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES) <= 0) continue;
			float d = vector.Distance(pos, st.GetOwner().GetOrigin());
			if (d < bestD) { bestD = d; best = st; }
		}
		return best;
	}

	protected bool FFRX_ScanStorage(IEntity e)
	{
		JWK_LogisticsStorageControllerComponent st =
			JWK_CompTU<JWK_LogisticsStorageControllerComponent>.FindIn(e);
		if (st) m_aScan.Insert(st);
		return true;
	}

	//------------------------------------------------------------------------------------
	protected void FFRX_ResetSunkCost(IEntity user)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm) return;
		int pid = pm.GetPlayerIdFromControlledEntity(user);
		if (pid > 0)
			FFRX_LoadoutAction.FFRX_ClearSunkCost(pid);
	}

	protected void FFRX_Notify(IEntity user, string text)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm) return;
		int pid = pm.GetPlayerIdFromControlledEntity(user);
		if (pid <= 0) return;
		PlayerController pc = pm.GetPlayerController(pid);
		if (!pc) return;
		JWK_PlayerControllerComponent jpc = JWK_PlayerControllerComponent.Cast(pc.FindComponent(JWK_PlayerControllerComponent));
		if (jpc) jpc.FFRX_Popup(text);
	}
}
