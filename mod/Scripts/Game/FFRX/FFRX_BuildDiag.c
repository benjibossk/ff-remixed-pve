// FF - REMIXED - PVE
// Diagnostic "pourquoi je ne peux pas construire" -- commande #pelle, ouverte a TOUS.
//
// Pourquoi une commande et pas des Print :
//   Le gate d'entree de la construction est CLIENT (JWK_MainMenuController.CanShow(), qui
//   decide si l'entree "Construire" est generee dans la roue). Un Print place la partirait
//   dans la console du JOUEUR, pas dans le log du serveur -- inexploitable pour un non-admin
//   qui ne peut pas nous envoyer sa console. Et CanShow() est appele en continu : y logger
//   spammerait autant que la boucle du jammer.
//   On interroge donc l'etat depuis le SERVEUR (qui connait le perso, son gadget en main et
//   sa position) : le joueur tape #pelle, recoit la reponse dans son chat, et la meme ligne
//   part dans le log serveur prefixee [FFRX][BuildDiag].
//
// Ce qu'on verifie, dans l'ordre exact ou FF le verifie :
//   1. gadget en MAIN GAUCHE present ? (GetAttachedGadgetAtLeftHandSlot)
//   2. ce gadget est-il un outil de construction pour FF ? (m_bConstructionTool)
//   3. sinon, le joueur porte-t-il une pelle ailleurs dans son inventaire, et laquelle ?
//   4. est-il dans une zone de construction, de sa faction, qui le contient ?
//
// Rappel etabli en analysant FF : il n'existe AUCUN gate par role, classe ou escouade dans
// la construction. Etre dans le genie ne donne et ne retire rien. Chaines ASCII.

class FFRX_BuildDiagUtil
{
	//------------------------------------------------------------------------------------------------
	//! "{GUID}Prefabs/.../ETool_MPL50.et" -> "ETool_MPL50"
	static string ShortName(ResourceName rn)
	{
		string s = rn;
		if (s == "")
			return "";

		int cut = s.LastIndexOf("/");
		if (cut >= 0)
			s = s.Substring(cut + 1, s.Length() - cut - 1);

		cut = s.LastIndexOf(".");
		if (cut > 0)
			s = s.Substring(0, cut);

		return s;
	}

	//------------------------------------------------------------------------------------------------
	static string EntName(IEntity e)
	{
		if (!e)
			return "";

		EntityPrefabData data = e.GetPrefabData();
		if (!data)
			return "";

		return ShortName(data.GetPrefabName());
	}

	//------------------------------------------------------------------------------------------------
	//! true si FF considere cet objet comme un outil de construction.
	static bool IsTool(IEntity item)
	{
		if (!item || !JWK.GetItemManager())
			return false;

		JWK_ItemAttributes at = JWK.GetItemManager().GetEntityAttributes(item);
		if (!at)
			return false;

		return at.m_bConstructionTool;
	}
}

// ---------------------------------------------------------------------------
//  #pelle -- pourquoi je ne peux pas construire ?
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_BuildDiagCommand : ScrServerCommand
{
	override string GetKeyword() { return "pelle"; }
	override bool IsServerSide() { return true; }
	//~ Ouvert a TOUT LE MONDE : c'est justement le joueur non-admin qui est bloque et qui
	//~ doit pouvoir savoir pourquoi, sans avoir a passer par un admin.
	override int RequiredChatPermission() { return EPlayerRole.NONE; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Diag(playerId); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return ScrServerCmdResult("Commande joueur uniquement (#pelle en jeu).", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Diag(int playerId)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return ScrServerCmdResult("PlayerManager indisponible.", EServerCmdResultType.ERR);

		IEntity player = pm.GetPlayerControlledEntity(playerId);
		if (!player)
			return ScrServerCmdResult("Perso introuvable, reessaye une fois en jeu.", EServerCmdResultType.ERR);

		string msg = Build(player, playerId);

		// Identite vue par le SERVEUR. A comparer avec la ligne [FFRX][MenuDiag] du client :
		// si les deux ne disent pas la meme chose (groupe, admin), on tient la divergence.
		Print("[FFRX][BuildDiag] " + pm.GetPlayerName(playerId) + " [" + Who(playerId) + "] : " + msg, LogLevel.NORMAL);
		return ScrServerCmdResult(msg, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	protected string Who(int playerId)
	{
		string groupName = "(aucun groupe)";
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm)
		{
			groupName = "(GroupsManager NULL)";
		}
		else
		{
			SCR_AIGroup g = gm.GetPlayerGroup(playerId);
			if (g)
			{
				groupName = g.GetCustomName();
				if (groupName == "")
					groupName = "(groupe sans nom, id " + g.GetGroupID().ToString() + ")";
			}
		}

		return "playerId=" + playerId.ToString()
			+ " groupe='" + groupName + "'"
			+ " admin=" + SCR_Global.IsAdmin(playerId).ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Concatenation avec '+' : string.Format tronque sa sortie a ~8 Ko.
	protected string Build(IEntity player, int playerId)
	{
		string msg = "";

		// --- 1. gadget en main gauche ---------------------------------------------------
		IEntity gadget = LeftHandGadget(player);
		if (!gadget)
		{
			msg = "AUCUN outil en main gauche. La pelle doit etre SORTIE (touche gadget), pas rangee dans le sac.";
			msg = msg + " " + CarriedToolReport(player);
			return msg;
		}

		string gname = FFRX_BuildDiagUtil.EntName(gadget);

		// --- 2. cet outil compte-t-il pour FF ? ------------------------------------------
		if (!FFRX_BuildDiagUtil.IsTool(gadget))
		{
			msg = "En main gauche : '" + gname + "' -- ce n'est PAS un outil de construction pour FF,";
			msg = msg + " donc l'entree Construire n'apparait pas dans la roue.";
			msg = msg + " Il faut une pelle FF (ETool_ALICE / ETool_MPL50), achetable a la boutique.";
			return msg;
		}

		msg = "Outil OK : '" + gname + "'. ";

		// --- 3. zone de construction ------------------------------------------------------
		msg = msg + AreaReport(player);

		// --- 4. rappel : l'escouade n'entre pas en jeu ------------------------------------
		msg = msg + " (Rappel : aucune escouade ne donne ni ne retire le droit de construire.)";
		return msg;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity LeftHandGadget(IEntity player)
	{
		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(player.FindComponent(SCR_CharacterControllerComponent));
		if (!cc)
			return null;

		return cc.GetAttachedGadgetAtLeftHandSlot();
	}

	//------------------------------------------------------------------------------------------------
	//! Cherche une pelle dans tout l'inventaire et dit si c'est la bonne. C'est LE cas
	//! frequent : le joueur "a une pelle" mais c'est une vanilla, inerte pour FF.
	protected string CarriedToolReport(IEntity player)
	{
		SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(player.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inv)
			return "";

		array<IEntity> items = {};
		inv.GetItems(items);

		string good = "";
		string bad = "";
		foreach (IEntity it : items)
		{
			string n = FFRX_BuildDiagUtil.EntName(it);
			if (n == "")
				continue;

			bool looksLikeTool = n.Contains("ETool") || n.Contains("Shovel");
			if (!looksLikeTool && !FFRX_BuildDiagUtil.IsTool(it))
				continue;

			if (FFRX_BuildDiagUtil.IsTool(it))
			{
				if (good != "") good = good + ", ";
				good = good + n;
			}
			else
			{
				if (bad != "") bad = bad + ", ";
				bad = bad + n;
			}
		}

		if (good != "")
			return "Tu as bien un outil valide dans ton inventaire (" + good + ") : sors-le en main gauche.";

		if (bad != "")
			return "Tu portes une pelle (" + bad + ") mais elle n'est PAS reconnue par FF comme outil de construction.";

		return "Aucune pelle trouvee dans ton inventaire.";
	}

	//------------------------------------------------------------------------------------------------
	protected string AreaReport(IEntity player)
	{
		if (!JWK.GetConstruction())
			return "Gestionnaire de construction indisponible.";

		vector pos = player.GetOrigin();
		JWK_BuildAreaControllerComponent area = JWK.GetConstruction().GetEffectiveAreaController(pos);
		if (!area)
			return "Tu n'es dans AUCUNE zone de construction : seuls les objets qui n'en exigent pas sont posables ici. Rapproche-toi d'une FOB ou d'une base tenue.";

		if (!area.IsBuildingAllowed())
			return "Zone de construction presente mais elle n'appartient PAS a ta faction : construction refusee.";

		if (!area.Contains(pos))
			return "Zone de construction trouvee mais tu es en dehors de ses limites.";

		return "Zone OK (type " + area.GetAreaType().ToString() + "). " + ItemsReport(pos, area);
	}

	//------------------------------------------------------------------------------------------------
	//! Recense les objets constructibles ICI. C'est l'etape qui manquait : outil OK + zone OK
	//! et pourtant rien ne s'ouvre = la liste du menu est VIDE, parce que chaque item est
	//! refuse par CheckCanBuild (type de zone incompatible, item desactive, limite atteinte).
	//! Le detail item par item part dans le LOG ; le chat ne recoit que le compte.
	protected string ItemsReport(vector pos, JWK_BuildAreaControllerComponent area)
	{
		if (!JWK.GetConstruction())
			return "";

		array<JWK_BaseConstructionItemConfig> all = JWK.GetConstruction().GetAllConstructionItemConfigs();
		if (!all || all.IsEmpty())
			return "AUCUN objet de construction n'est enregistre du tout (catalogue vide).";

		int okCount = 0;
		int koCount = 0;
		string detail = "";

		foreach (JWK_BaseConstructionItemConfig cfg : all)
		{
			JWK_BuildItemConfig buildItem = JWK_BuildItemConfig.Cast(cfg);
			if (!buildItem)
				continue;   // placeables : autre chemin (S_CanPlace), hors de ce recensement

			JWK_EFeedback reason;
			bool can = JWK.GetConstruction().CheckCanBuild(buildItem, area, pos, true, reason);
			if (can)
			{
				okCount++;
				continue;
			}

			koCount++;
			detail = detail + "\n  REFUSE  " + cfg.m_sName
				+ "  raison=" + SCR_Enum.GetEnumName(JWK_EFeedback, reason)
				+ "  areaTypes=" + buildItem.m_iBuildAreaTypes.ToString()
				+ "  requiresArea=" + buildItem.m_bRequiresBuildArea.ToString();
		}

		Print("[FFRX][BuildDiag] Zone type=" + area.GetAreaType().ToString()
			+ " limite=" + area.GetBuildItemsLimit().ToString()
			+ " -- constructibles " + okCount.ToString() + ", refuses " + koCount.ToString()
			+ detail, LogLevel.NORMAL);

		if (okCount > 0)
			return okCount.ToString() + " objet(s) constructible(s) ici (" + koCount.ToString() + " refuses). Detail dans le log serveur.";

		return "AUCUN objet constructible ici : le menu s'ouvre vide, d'ou l'impression qu'il ne s'ouvre pas. Les "
			+ koCount.ToString() + " refus sont detailles dans le log serveur.";
	}
}
