// ============================================================================
//  Flt_Loadout — système léger de loadout (capture / application).
//
//  Inspiré du mécanisme de DynamicTacticalOperations, MAIS autonome : aucune
//  dépendance à DTO, aucun mod du game mode/inventaire du base game.
//
//  Un loadout = arbre récursif { prefab, slot, storageIndex, children } — juste
//  des GUID de prefabs + placement. Portable/JSON -> stockable sur le site.
//   - CAPTURE : parcourt l'inventaire d'un perso -> arbre.
//   - APPLY   : vide l'équipement + spawn+place chaque item récursivement.
//
//  Serveur uniquement (spawn/inventaire autoritaires).
// ============================================================================

class Flt_LoadoutItem : JsonApiStruct
{
	string prefab;
	int    slot;			// index de slot dans le storage parent
	int    storageIndex;	// quel sous-storage du parent (0 = top-level)
	ref array<ref Flt_LoadoutItem> children;

	void Flt_LoadoutItem()
	{
		children = {};
		RegV("prefab"); RegV("slot"); RegV("storageIndex"); RegV("children");
	}
}

class Flt_LoadoutRoot : JsonApiStruct
{
	ref array<ref Flt_LoadoutItem> clothing;
	ref array<ref Flt_LoadoutItem> weapons;

	void Flt_LoadoutRoot()
	{
		clothing = {};
		weapons = {};
		RegV("clothing"); RegV("weapons");
	}
}

// --- Transport site : un loadout nommé { name, faction, data(=JSON CaptureJson) } ----------
class Flt_LoadoutMeta : JsonApiStruct
{
	string name;
	string faction;
	string data;	// le loadout complet (JSON de CaptureJson), transporté comme chaîne

	void Flt_LoadoutMeta() { RegV("name"); RegV("faction"); RegV("data"); }
}

class Flt_LoadoutList : JsonApiStruct
{
	ref array<ref Flt_LoadoutMeta> loadouts;

	void Flt_LoadoutList() { loadouts = {}; RegV("loadouts"); }
}

// --- Registre SERVEUR : loadouts du site rangés par faction (ordre stable) -----------------
// Rempli par Flt_LoadoutSystem.OnFetched à chaque GET /loadouts. L'action d'arsenal et
// l'application serveur lisent ici en synchrone (index = position dans la liste de la faction).
class Flt_LoadoutRegistry
{
	protected static ref Flt_LoadoutRegistry s_Instance;
	protected ref map<string, ref array<ref Flt_LoadoutMeta>> m_mByFaction = new map<string, ref array<ref Flt_LoadoutMeta>>();

	static Flt_LoadoutRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new Flt_LoadoutRegistry();
		return s_Instance;
	}

	//! Reconstruit le registre depuis la liste complète renvoyée par le site.
	void SetFromList(array<ref Flt_LoadoutMeta> all)
	{
		m_mByFaction.Clear();
		if (!all)
			return;
		foreach (Flt_LoadoutMeta m : all)
		{
			if (!m || m.name == "")
				continue;
			string fac = m.faction;
			array<ref Flt_LoadoutMeta> arr;
			if (!m_mByFaction.Find(fac, arr))
			{
				arr = {};
				m_mByFaction.Set(fac, arr);
			}
			arr.Insert(m);
		}
	}

	//! Nombre de loadouts pour une faction.
	int Count(string faction)
	{
		array<ref Flt_LoadoutMeta> arr;
		if (m_mByFaction.Find(faction, arr) && arr)
			return arr.Count();
		return 0;
	}

	//! Loadout à l'index donné pour une faction, ou null.
	Flt_LoadoutMeta Get(string faction, int index)
	{
		array<ref Flt_LoadoutMeta> arr;
		if (!m_mByFaction.Find(faction, arr) || !arr)
			return null;
		if (index < 0 || index >= arr.Count())
			return null;
		return arr[index];
	}

	//! Nom du loadout à l'index (pour affichage), "" si absent.
	string GetName(string faction, int index)
	{
		Flt_LoadoutMeta m = Get(faction, index);
		if (!m)
			return "";
		return m.name;
	}

	//! Noms d'une faction joints par '\n' (transport RPC vers le client).
	string NamesCsv(string faction)
	{
		array<ref Flt_LoadoutMeta> arr;
		if (!m_mByFaction.Find(faction, arr) || !arr)
			return "";
		string csv = "";
		foreach (Flt_LoadoutMeta m : arr)
		{
			if (!m)
				continue;
			if (csv != "")
				csv += "\n";
			csv += m.name;
		}
		return csv;
	}

	// ---- Côté CLIENT : noms de MA faction (poussés par RPC du serveur) --------------------
	// Le client n'a ni le JSON ni la clé API ; il reçoit juste la liste ordonnée des noms de
	// sa faction pour afficher/nommer les actions d'arsenal. L'index reste aligné avec le
	// serveur (même source = liste du site).
	protected ref array<string> m_aClientNames = {};

	//! Remplace la liste cliente à partir d'un CSV séparé par '\n'.
	void SetClientNamesRaw(string raw)
	{
		m_aClientNames.Clear();
		if (raw == "")
			return;
		array<string> parts = {};
		raw.Split("\n", parts, false);
		foreach (string p : parts)
		{
			if (p != "")
				m_aClientNames.Insert(p);
		}
	}

	int ClientCount() { return m_aClientNames.Count(); }

	string ClientName(int index)
	{
		if (index < 0 || index >= m_aClientNames.Count())
			return "";
		return m_aClientNames[index];
	}
}

class Flt_LoadoutSystem
{
	//========================== CAPTURE ==========================

	//! Capture le loadout d'un perso -> JSON.
	static string CaptureJson(IEntity character)
	{
		if (!character)
			return "";
		Flt_LoadoutRoot root = new Flt_LoadoutRoot();

		EquipedLoadoutStorageComponent clothing = EquipedLoadoutStorageComponent.Cast(character.FindComponent(EquipedLoadoutStorageComponent));
		EquipedWeaponStorageComponent weapons = EquipedWeaponStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));

		CaptureStorage(clothing, root.clothing);
		CaptureStorage(weapons, root.weapons);

		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", root);
		return ctx.ExportToString();
	}

	protected static void CaptureStorage(BaseInventoryStorageComponent storage, array<ref Flt_LoadoutItem> outItems)
	{
		if (!storage)
			return;
		int n = storage.GetSlotsCount();
		for (int i = 0; i < n; i++)
		{
			IEntity item = storage.Get(i);
			if (!item)
				continue;
			ResourceName pf = GetPrefabName(item);
			if (pf == "")
				continue;
			Flt_LoadoutItem li = new Flt_LoadoutItem();
			li.prefab = pf;
			li.slot = i;
			li.storageIndex = 0;
			CaptureChildren(item, li.children);
			outItems.Insert(li);
		}
	}

	protected static void CaptureChildren(IEntity item, array<ref Flt_LoadoutItem> outChildren)
	{
		array<Managed> comps = {};
		item.FindComponents(BaseInventoryStorageComponent, comps);
		for (int si = 0; si < comps.Count(); si++)
		{
			BaseInventoryStorageComponent sub = BaseInventoryStorageComponent.Cast(comps[si]);
			if (!sub)
				continue;
			int n = sub.GetSlotsCount();
			for (int i = 0; i < n; i++)
			{
				IEntity subItem = sub.Get(i);
				if (!subItem)
					continue;
				ResourceName pf = GetPrefabName(subItem);
				if (pf == "")
					continue;
				Flt_LoadoutItem li = new Flt_LoadoutItem();
				li.prefab = pf;
				li.slot = i;
				li.storageIndex = si;
				CaptureChildren(subItem, li.children);
				outChildren.Insert(li);
			}
		}
	}

	//========================== APPLY ==========================

	//! Applique un loadout (JSON) à un perso : vide l'équipement puis spawn+place.
	static bool ApplyJson(IEntity character, string json)
	{
		if (Replication.IsClient() || !character || json == "")
			return false;

		Flt_LoadoutRoot root = new Flt_LoadoutRoot();
		root.ExpandFromRAW(json);
		if ((!root.clothing || root.clothing.IsEmpty()) && (!root.weapons || root.weapons.IsEmpty()))
			return false;

		SCR_InventoryStorageManagerComponent inv = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inv)
			return false;

		EquipedLoadoutStorageComponent clothing = EquipedLoadoutStorageComponent.Cast(character.FindComponent(EquipedLoadoutStorageComponent));
		EquipedWeaponStorageComponent weapons = EquipedWeaponStorageComponent.Cast(character.FindComponent(EquipedWeaponStorageComponent));

		ClearStorage(clothing, inv);
		ClearStorage(weapons, inv);

		if (clothing && root.clothing)
		{
			foreach (Flt_LoadoutItem li : root.clothing)
				SpawnPlace(li, clothing, character, inv);
		}
		if (weapons && root.weapons)
		{
			foreach (Flt_LoadoutItem li : root.weapons)
				SpawnPlace(li, weapons, character, inv);
		}
		return true;
	}

	protected static void ClearStorage(BaseInventoryStorageComponent storage, SCR_InventoryStorageManagerComponent inv)
	{
		if (!storage || !inv)
			return;
		int n = storage.GetSlotsCount();
		for (int i = n - 1; i >= 0; i--)
		{
			IEntity item = storage.Get(i);
			if (!item)
				continue;
			inv.TryRemoveItemFromStorage(item, storage, null);
			SCR_EntityHelper.DeleteEntityAndChildren(item);
		}
	}

	protected static void SpawnPlace(Flt_LoadoutItem li, BaseInventoryStorageComponent parent, IEntity near, SCR_InventoryStorageManagerComponent inv)
	{
		if (!li || li.prefab == "" || !parent || !near || !inv)
			return;

		Resource res = Resource.Load(li.prefab);
		if (!res || !res.IsValid())
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		near.GetTransform(params.Transform);

		IEntity newItem = GetGame().SpawnEntityPrefab(res, near.GetWorld(), params);
		if (!newItem)
			return;

		if (!inv.TryInsertItemInStorage(newItem, parent, li.slot, null))
		{
			if (!inv.TryInsertItemInStorage(newItem, parent, -1, null))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(newItem);
				return;
			}
		}

		// enfants -> sous-storages (on vide d'abord pour éviter le remplissage par défaut)
		if (li.children && li.children.Count() > 0)
		{
			array<Managed> comps = {};
			newItem.FindComponents(BaseInventoryStorageComponent, comps);
			foreach (Managed c : comps)
			{
				BaseInventoryStorageComponent s = BaseInventoryStorageComponent.Cast(c);
				if (s)
					ClearStorage(s, inv);
			}
			foreach (Flt_LoadoutItem child : li.children)
			{
				if (!child || child.storageIndex < 0 || child.storageIndex >= comps.Count())
					continue;
				BaseInventoryStorageComponent targetSub = BaseInventoryStorageComponent.Cast(comps[child.storageIndex]);
				if (targetSub)
					SpawnPlace(child, targetSub, near, inv);
			}
		}
	}

	//========================== RÉCEPTION SITE ==========================

	//! Appelé par le callback GET quand le site renvoie la liste des loadouts.
	//! wantName == ""  -> on liste seulement (notif au joueur) ;
	//! wantName != ""  -> on applique ce loadout au joueur playerId.
	static void OnFetched(int playerId, string wantName, string json)
	{
		Flt_LoadoutList list = new Flt_LoadoutList();
		list.ExpandFromRAW(json);
		if (!list.loadouts)
			list.loadouts = {};

		// Toujours rafraîchir le registre serveur (l'action d'arsenal lit ici).
		Flt_LoadoutRegistry.Get().SetFromList(list.loadouts);
		// Pousse les noms aux clients (pour afficher/nommer les actions d'arsenal).
		Flt_BroadcastNames();

		// "*cache*" = fetch périodique silencieux (juste peupler le registre).
		if (wantName == "*cache*")
			return;

		if (wantName == "")
		{
			string names = "";
			foreach (Flt_LoadoutMeta m : list.loadouts)
			{
				if (!m)
					continue;
				if (names != "")
					names += ", ";
				names += m.name;
			}
			if (names == "")
				names = "(aucun)";
			Print("[LOADOUT] Dispo (" + list.loadouts.Count() + ") : " + names, LogLevel.NORMAL);
			return;
		}

		// application
		foreach (Flt_LoadoutMeta m : list.loadouts)
		{
			if (!m || m.name != wantName)
				continue;
			IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!ent)
			{
				Print("[LOADOUT] apply : pas de perso pour player " + playerId, LogLevel.WARNING);
				return;
			}
			bool ok = ApplyJson(ent, m.data);
			Print("[LOADOUT] apply '" + wantName + "' -> " + ok, LogLevel.NORMAL);
			return;
		}
		Print("[LOADOUT] apply : nom introuvable '" + wantName + "'", LogLevel.WARNING);
	}

	//! Serveur : envoie à chaque joueur la liste (ordonnée) des noms de loadouts de SA faction,
	//! via RPC owner (modded SCR_PlayerController.Flt_PushLoadoutNames).
	static void Flt_BroadcastNames()
	{
		if (Replication.IsClient())
			return;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		array<int> ids = {};
		pm.GetPlayers(ids);
		foreach (int pid : ids)
		{
			string fac = FactionKeyOfPlayer(pm, pid);
			string csv = Flt_LoadoutRegistry.Get().NamesCsv(fac);
			SCR_PlayerController ctrl = SCR_PlayerController.Cast(pm.GetPlayerController(pid));
			if (ctrl)
				ctrl.Flt_PushLoadoutNames(csv);
		}
	}

	//! Clé de faction d'un joueur (via son perso contrôlé), "" si inconnue/non spawné.
	static string FactionKeyOfPlayer(PlayerManager pm, int playerId)
	{
		IEntity ent = pm.GetPlayerControlledEntity(playerId);
		if (!ent)
			return "";
		FactionAffiliationComponent fc = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
		if (!fc)
			return "";
		Faction f = fc.GetAffiliatedFaction();
		if (!f)
			return "";
		return f.GetFactionKey();
	}

	//========================== helper ==========================
	protected static ResourceName GetPrefabName(IEntity entity)
	{
		if (!entity)
			return "";
		EntityPrefabData pd = entity.GetPrefabData();
		if (!pd)
			return "";
		return pd.GetPrefabName();
	}
}

// --- Action d'arsenal : "Prendre <nom du loadout>" (une par index) --------------------------
//   Posée sur les caisses via ActionsManagerComponent.additionalActions (override prefab).
//   CLIENT : s'affiche/se nomme depuis le registre client (noms poussés par RPC).
//   SERVEUR : PerformAction applique le JSON du loadout (registre serveur) au joueur.
class Flt_LoadoutAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.EditBox, "Fleet : index du loadout (0 = 1er) pour la faction du joueur.")]
	protected int m_iFltIndex;

	//! Clé de faction du joueur qui utilise l'action.
	protected string Flt_UserFaction(IEntity user)
	{
		if (!user)
			return "";
		FactionAffiliationComponent fc = FactionAffiliationComponent.Cast(user.FindComponent(FactionAffiliationComponent));
		if (!fc)
			return "";
		Faction f = fc.GetAffiliatedFaction();
		if (!f)
			return "";
		return f.GetFactionKey();
	}

	//------------------------------------------------------------------------------------------------
	// CLIENT : n'affiche l'action que si un loadout existe à cet index (registre client).
	override bool CanBeShownScript(IEntity user)
	{
		return Flt_LoadoutRegistry.Get().ClientCount() > m_iFltIndex;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		return Flt_LoadoutRegistry.Get().ClientCount() > m_iFltIndex;
	}

	//------------------------------------------------------------------------------------------------
	// CLIENT : libellé = "Prendre <nom>".
	override bool GetActionNameScript(out string outName)
	{
		string n = Flt_LoadoutRegistry.Get().ClientName(m_iFltIndex);
		if (n == "")
			return false;
		outName = "Prendre : " + n;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// PerformAction est diffusé partout : on n'applique que sur l'autorité (serveur/host).
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		RplComponent rpl = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		if (rpl && rpl.IsProxy())
			return;

		string fac = Flt_UserFaction(pUserEntity);
		Flt_LoadoutMeta meta = Flt_LoadoutRegistry.Get().Get(fac, m_iFltIndex);
		if (!meta)
		{
			Print("[LOADOUT] arsenal : aucun loadout index " + m_iFltIndex + " pour faction '" + fac + "'", LogLevel.WARNING);
			return;
		}
		bool ok = Flt_LoadoutSystem.ApplyJson(pUserEntity, meta.data);
		Print("[LOADOUT] arsenal '" + meta.name + "' (" + fac + ") -> " + ok, LogLevel.NORMAL);
	}
}

// --- Callback GET des loadouts (site -> jeu) : délègue à Flt_LoadoutSystem.OnFetched --------
// Se retire de la liste de Flt_GTGPositions à la fin (évite le GC pendant l'async).
class Flt_GTG_LoadoutGetCallback : RestCallback
{
	protected int    m_iPlayerId;
	protected string m_sWantName;	// "" = liste seulement

	void Flt_GTG_LoadoutGetCallback(int playerId, string wantName)
	{
		m_iPlayerId = playerId;
		m_sWantName = wantName;
		// Nouvelle API (pas d'override -> pas de spam "OnSuccess not set"). cb.GetData() = corps.
		SetOnSuccess(Flt_OnOk);
		SetOnError(Flt_OnErr);	// gère aussi le timeout (pas de SetOnTimeout dans l'API)
	}

	protected void Flt_OnOk(RestCallback cb)
	{
		Flt_LoadoutSystem.OnFetched(m_iPlayerId, m_sWantName, cb.GetData());
		Flt_GTGPositions.GetInstance().Flt_ReleaseLoadoutCb(this);
	}
	protected void Flt_OnErr(RestCallback cb)
	{
		Print("[LOADOUT] fetch error", LogLevel.WARNING);
		Flt_GTGPositions.GetInstance().Flt_ReleaseLoadoutCb(this);
	}
}

// --- Commande : #loadout <save|load|publish|list|apply> ----------------------
//   save/load        : test LOCAL (capture en mémoire, réapplique) — pas de site.
//   publish <nom>    : capture le loadout courant -> envoie au SITE sous ce nom (admin).
//   list             : demande au site la liste des loadouts (log console).
//   apply <nom>      : récupère ce loadout depuis le site et l'applique au joueur.
[BaseContainerProps()]
class Flt_LoadoutCommand : ScrServerCommand
{
	protected static ref map<int, string> s_mSaved = new map<int, string>();	// playerId -> JSON (test local)

	override string GetKeyword() { return "loadout"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.NONE; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	//! Clé de faction du perso (US/USSR/…), "" si inconnue.
	protected string FactionKey(IEntity ent)
	{
		FactionAffiliationComponent fc = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
		if (!fc)
			return "";
		Faction fac = fc.GetAffiliatedFaction();
		if (!fac)
			return "";
		return fac.GetFactionKey();
	}

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Pas de perso.", EServerCmdResultType.ERR);

		string sub = "";
		if (argv.Count() >= 2)
			sub = argv[1];

		if (sub == "save")
		{
			string json = Flt_LoadoutSystem.CaptureJson(ent);
			s_mSaved.Set(playerId, json);
			Print("[LOADOUT] Capturé (" + json.Length() + " car.) : " + json, LogLevel.NORMAL);
			return ScrServerCmdResult("Loadout capturé.", EServerCmdResultType.OK);
		}
		else if (sub == "load")
		{
			string json;
			if (!s_mSaved.Find(playerId, json))
				return ScrServerCmdResult("Aucun loadout sauvé (fais #loadout save).", EServerCmdResultType.ERR);
			bool ok = Flt_LoadoutSystem.ApplyJson(ent, json);
			if (ok)
				return ScrServerCmdResult("Loadout appliqué.", EServerCmdResultType.OK);
			return ScrServerCmdResult("Échec application.", EServerCmdResultType.ERR);
		}
		else if (sub == "publish")
		{
			if (argv.Count() < 3)
				return ScrServerCmdResult("Usage: #loadout publish <nom>", EServerCmdResultType.PARAMETERS);
			string name = argv[2];
			string json = Flt_LoadoutSystem.CaptureJson(ent);
			if (json == "")
				return ScrServerCmdResult("Capture vide.", EServerCmdResultType.ERR);
			Flt_GTGPositions.GetInstance().Flt_PublishLoadout(name, FactionKey(ent), json);
			return ScrServerCmdResult("Loadout '" + name + "' envoyé au site.", EServerCmdResultType.OK);
		}
		else if (sub == "list")
		{
			Flt_GTGPositions.GetInstance().Flt_FetchLoadouts(playerId, "");
			return ScrServerCmdResult("Liste demandée (voir console).", EServerCmdResultType.OK);
		}
		else if (sub == "apply")
		{
			if (argv.Count() < 3)
				return ScrServerCmdResult("Usage: #loadout apply <nom>", EServerCmdResultType.PARAMETERS);
			Flt_GTGPositions.GetInstance().Flt_FetchLoadouts(playerId, argv[2]);
			return ScrServerCmdResult("Loadout '" + argv[2] + "' demandé au site…", EServerCmdResultType.OK);
		}
		return ScrServerCmdResult("Usage: #loadout save|load|publish <nom>|list|apply <nom>", EServerCmdResultType.PARAMETERS);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("Joueur uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}

// --- RPC : le serveur pousse à CE joueur la liste ordonnée des noms de loadouts de sa faction
// (pour afficher/nommer les actions sur les caisses d'arsenal). '\n' = séparateur.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	void Flt_PushLoadoutNames(string csv)
	{
		PlayerController local = GetGame().GetPlayerController();
		if (local && local.GetPlayerId() == GetPlayerId())
			Flt_LoadoutRegistry.Get().SetClientNamesRaw(csv);	// joueur local (listen-host)
		else
			Rpc(Flt_RpcRecvLoadoutNames, csv);					// client distant
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void Flt_RpcRecvLoadoutNames(string csv)
	{
		Flt_LoadoutRegistry.Get().SetClientNamesRaw(csv);
	}
}
