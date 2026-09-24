// FF - REMIXED - PVE
// Predefined groups / squads from a config file (approach borrowed from GTG
// Groups and Radios, reimplemented in-addon).
//
// On game start (server): load $profile:ffrx-groups.json -> optionally delete
// all existing playable groups -> create the predefined groups. Free group
// creation by players is blocked, so everyone joins a predefined squad.
//
// Fleet integration: the creation logic lives in the static FFRX_GroupsManager
// so Fleet can drive it LIVE from the web (Fleet writes the JSON then calls
// FFRX_GroupsManager.ApplyFromProfile(), or calls CreateGroup(def) per command).
// Fleet already reads live group state for the web livemap/history separately.
//
// JSON shape (place at <profile>/ffrx-groups.json):
// {
//   "options": { "deleteExistingGroupsOnStart": true, "blockFreeCreation": true },
//   "groups": [
//     { "groupName":"AMF - Alpha", "description":"Infanterie", "faction":"FIA",
//       "maxMembers":8, "radioFrequencies":[{ "frequency":39.0 }],
//       "allowedPlayerUIDs":[] }
//   ]
// }

class FFRX_GroupFreq
{
	float frequency;
}

class FFRX_GroupDef
{
	string groupName;
	string description;
	string faction;
	int maxMembers;
	ref array<ref FFRX_GroupFreq> radioFrequencies;
	ref array<string> allowedPlayerUIDs;
	bool etatMajor; // members of this squad can validate procurement requests (D4)
	bool genie;     // members build faster (FFRX_RoleBonus)
	bool medic;     // reserve : meme patron, volet medical a trancher

	void FFRX_GroupDef()
	{
		radioFrequencies = {};
		allowedPlayerUIDs = {};
	}
}

class FFRX_GroupsOptions
{
	bool deleteExistingGroupsOnStart = true;
	bool blockFreeCreation = true;
}

class FFRX_GroupsConfig
{
	ref FFRX_GroupsOptions options;
	ref array<ref FFRX_GroupDef> groups;

	void FFRX_GroupsConfig()
	{
		options = new FFRX_GroupsOptions();
		groups = {};
	}

	static FFRX_GroupsConfig Load(string filePath)
	{
		if (!FileIO.FileExists(filePath)) {
			Print("[FFRX][Groups] No config at " + filePath + " - no predefined groups.");
			return null;
		}

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile(filePath)) {
			Print("[FFRX][Groups] Failed to load JSON: " + filePath);
			return null;
		}

		FFRX_GroupsConfig config = new FFRX_GroupsConfig();
		if (!ctx.ReadValue("", config)) {
			Print("[FFRX][Groups] Failed to parse JSON into config.");
			return null;
		}

		return config;
	}
}

// Shared block flag + reusable creation logic (server-side). Public statics so
// Fleet can drive predefined groups live from the web.
class FFRX_GroupsManager
{
	static const string CONFIG_PATH = "$profile:ffrx-groups.json";
	static bool s_bBlockFreeCreation;
	static bool s_bCreatingPredefined; // true only while WE create our groups
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	static ref array<int> s_aGroupIds;

	static array<int> GroupIds()
	{
		if (!s_aGroupIds)
			s_aGroupIds = new array<int>();

		return s_aGroupIds;
	}
	static ref map<int, string> s_mGroupNames;

	static map<int, string> GroupNames()
	{
		if (!s_mGroupNames)
			s_mGroupNames = new map<int, string>();

		return s_mGroupNames;
	}
	static ref array<int> s_aEtatMajorGroupIds;

	static array<int> EtatMajorIds()
	{
		if (!s_aEtatMajorGroupIds)
			s_aEtatMajorGroupIds = new array<int>();

		return s_aEtatMajorGroupIds;
	}
	static ref array<int> s_aGenieGroupIds;

	static array<int> GenieIds()
	{
		if (!s_aGenieGroupIds)
			s_aGenieGroupIds = new array<int>();

		return s_aGenieGroupIds;
	}
	static ref array<int> s_aMedicGroupIds;

	static array<int> MedicIds()
	{
		if (!s_aMedicGroupIds)
			s_aMedicGroupIds = new array<int>();

		return s_aMedicGroupIds;
	}
	static ref map<int, ref array<string>> s_mGroupWhitelist;

	static map<int, ref array<string>> Whitelist()
	{
		if (!s_mGroupWhitelist)
			s_mGroupWhitelist = new map<int, ref array<string>>();

		return s_mGroupWhitelist;
	}

	// Whitelist gate: a group whose config has a non-empty allowedPlayerUIDs only
	// admits those UIDs (e.g. KILO Commandement). Groups without a list are open.
	// UID == FFRX_LoadoutSystem.UidOfPlayer (the same Bohemia/Discord-linked ID used
	// by the loadout site), so admins fill allowedPlayerUIDs with those UIDs.
	static bool IsPlayerAllowedInGroup(int groupID, int playerID)
	{
		array<string> wl = Whitelist().Get(groupID);
		if (!wl || wl.IsEmpty())
			return true; // open group

		string uid = FFRX_LoadoutSystem.UidOfPlayer(playerID);
		return uid != "" && wl.Contains(uid);
	}

	static void NotifyJoinDenied(int playerID)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerID);
		if (ent)
			FFRX_LoadoutSystem.NotifyPlayer(ent, "Ce groupe est reserve (liste d'acces). Vous n'y etes pas autorise.");
	}

	// Live whitelist edit from the web (Fleet group_whitelist command). csv =
	// semicolon-separated UIDs; "" clears the list (group becomes open). Replaces
	// the whole list. Runtime only (like the other live group edits) -> to persist
	// across a restart, keep the UIDs in ffrx-groups.json too (or let the site
	// re-push the command). Does NOT kick players already in the group.
	static void SetGroupWhitelistCsv(int groupID, string csv)
	{
		array<string> wl = {};
		if (csv != "") {
			array<string> parts = {};
			csv.Split(";", parts, true);
			foreach (string p : parts) {
				if (p != "") wl.Insert(p);
			}
		}

		if (wl.IsEmpty())
			Whitelist().Remove(groupID);
		else
			Whitelist().Set(groupID, wl);

		Print(string.Format("[FFRX][Groups] Whitelist updated for group %1: %2 UID(s).", groupID, wl.Count()));
	}

	// True if the player belongs to an etat-major (command) squad.
	static bool IsEtatMajor(int playerId)
	{
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm) return false;
		SCR_AIGroup g = gm.GetPlayerGroup(playerId);
		if (!g) return false;
		return EtatMajorIds().Contains(g.GetGroupID());
	}

	// Le joueur appartient-il a une escouade portant ce drapeau dans ffrx-groups.json ?
	// Meme patron que IsEtatMajor : l'appartenance est une donnee de CONFIG, pas une
	// constante dans le code -- renommer ou reorganiser les escouades depuis le site ne
	// casse rien, et un serveur peut avoir deux escouades du genie ou aucune.
	static bool IsInFlaggedGroup(int playerId, array<int> flaggedIds)
	{
		if (!flaggedIds || flaggedIds.IsEmpty()) return false;
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm) return false;
		SCR_AIGroup g = gm.GetPlayerGroup(playerId);
		if (!g) return false;
		return flaggedIds.Contains(g.GetGroupID());
	}

	static bool IsGenie(int playerId)
	{
		return IsInFlaggedGroup(playerId, GenieIds());
	}

	static bool IsMedic(int playerId)
	{
		return IsInFlaggedGroup(playerId, MedicIds());
	}

	// All currently-online etat-major players.
	static void GetEtatMajorPlayers(out array<int> outPlayers)
	{
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm) return;

		array<int> all = {};
		GetGame().GetPlayerManager().GetPlayers(all);
		foreach (int pid : all) {
			SCR_AIGroup g = gm.GetPlayerGroup(pid);
			if (g && EtatMajorIds().Contains(g.GetGroupID()))
				outPlayers.Insert(pid);
		}
	}

	// Group names straight from the config file (for the settings spinbox, which
	// is built before the groups are actually created at game start).
	static array<string> GetConfiguredGroupNames()
	{
		array<string> names = {};
		FFRX_GroupsConfig config = FFRX_GroupsConfig.Load(CONFIG_PATH);
		if (config) {
			foreach (FFRX_GroupDef def : config.groups)
				names.Insert(def.groupName);
		}
		return names;
	}

	// The default squad new players are dropped into. Driven by the settings
	// spinbox (1-based); config order == GroupIds() order == spinbox order.
	static SCR_AIGroup GetDefaultGroup()
	{
		if (GroupIds().IsEmpty()) return null;

		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm) return null;

		int idx = 0;
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (cache) idx = cache.m_iFFRXDefaultSquad - 1;
		idx = Math.ClampInt(idx, 0, GroupIds().Count() - 1);

		return gm.FindGroup(GroupIds()[idx]);
	}

	// Load config from the profile and (re)apply: delete existing + create.
	static void ApplyFromProfile()
	{
		FFRX_GroupsConfig config = FFRX_GroupsConfig.Load(CONFIG_PATH);
		if (!config) return;

		Apply(config);
	}

	static void Apply(FFRX_GroupsConfig config)
	{
		if (!config) return;

		s_bBlockFreeCreation = config.options.blockFreeCreation;
		GroupIds().Clear();
		GroupNames().Clear();
		EtatMajorIds().Clear();
		GenieIds().Clear();
		MedicIds().Clear();
		Whitelist().Clear();
		Print(string.Format("[FFRX][Groups] Apply: blockFreeCreation=%1, %2 group(s).",
			s_bBlockFreeCreation, config.groups.Count()));

		if (config.options.deleteExistingGroupsOnStart)
			DeleteAllGroups();

		foreach (FFRX_GroupDef def : config.groups)
			CreateGroup(def);
	}

	static void DeleteAllGroups()
	{
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm) return;

		array<SCR_AIGroup> groups = {};
		gm.GetAllPlayableGroups(groups);
		foreach (SCR_AIGroup g : groups)
			SCR_EntityHelper.DeleteEntityAndChildren(g);

		Print(string.Format("[FFRX][Groups] Deleted %1 existing group(s).", groups.Count()));
	}

	// ---- Live editing API (driven by Fleet from the web) ----

	static SCR_AIGroup FindGroup(int id)
	{
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm) return null;
		return gm.FindGroup(id);
	}

	// Edit fields on an existing group. Sentinels for "leave unchanged":
	// name/desc = "", maxMembers/freqMHz = 0, role/priv/flag = -1.
	static void EditGroup(int id, string name, string desc, int maxMembers, float freqMHz, int role, int priv, int flag)
	{
		SCR_AIGroup g = FindGroup(id);
		if (!g) {
			Print(string.Format("[FFRX][Groups] Edit: group %1 not found.", id));
			return;
		}

		if (name != "") {
			GroupNames().Set(id, name); // keep name protection in sync
			g.SetCustomName(name, 0);
		}
		if (desc != "") g.SetCustomDescription(desc, 0);
		if (maxMembers > 0) g.SetMaxGroupMembers(maxMembers);
		if (freqMHz > 0) g.SetRadioFrequency(freqMHz * 1000);
		if (role >= 0) g.SetGroupRole(role);
		if (priv >= 0) g.SetPrivate(priv == 1);
		if (flag >= 0) g.SetGroupFlag(flag, false);

		Print(string.Format("[FFRX][Groups] Edited group %1.", id));
	}

	static void DeleteGroupById(int id)
	{
		SCR_AIGroup g = FindGroup(id);
		if (g) SCR_EntityHelper.DeleteEntityAndChildren(g);

		GroupNames().Remove(id);
		int idx = GroupIds().Find(id);
		if (idx != -1) GroupIds().Remove(idx);

		Print(string.Format("[FFRX][Groups] Deleted group %1.", id));
	}

	// Create a group live from plain fields.
	static SCR_AIGroup CreateGroupLive(string factionKey, string name, string desc, int maxMembers, float freqMHz)
	{
		FFRX_GroupDef def = new FFRX_GroupDef();
		def.faction = factionKey;
		def.groupName = name;
		def.description = desc;
		def.maxMembers = maxMembers;
		if (freqMHz > 0) {
			FFRX_GroupFreq f = new FFRX_GroupFreq();
			f.frequency = freqMHz;
			def.radioFrequencies.Insert(f);
		}
		return CreateGroup(def);
	}

	// Resolves the faction key a group should be created on.
	//
	// The squads (ALPHA, BRAVO, ...) are the same whatever the theatre, but the
	// player faction is NOT: Everon runs "FIA", Anizay runs "FIA_DESERT". A key
	// hardcoded in the JSON would silently create the groups on the wrong faction
	// -- they would exist but nobody could join them.
	//
	// So: "" or "PLAYER" (recommended in the JSON) = whatever faction plays this
	// map. An explicit key is still honoured if that faction exists; if it does
	// not, we fall back to the player faction rather than dropping the squad.
	static string FFRX_ResolveFactionKey(string requested)
	{
		string playerKey = "";
		JWK_FactionManager jwkFactions = JWK.GetFactions();
		if (jwkFactions) {
			JWK_Faction playerFaction = jwkFactions.GetJWKFactionByRole(JWK_EFactionRole.PLAYER);
			if (playerFaction)
				playerKey = playerFaction.GetKey();
		}

		if (requested == "" || requested == "PLAYER")
			return playerKey;

		FactionManager fm = GetGame().GetFactionManager();
		if (fm && fm.GetFactionByKey(requested))
			return requested;

		if (playerKey != "") {
			Print(string.Format(
				"[FFRX][Groups] Faction '%1' not loaded on this map, using player faction '%2'.",
				requested, playerKey));
			return playerKey;
		}

		return requested; // let the caller report it as unknown
	}

	// Creates one predefined group. Returns it (or null). Callable at runtime.
	static SCR_AIGroup CreateGroup(FFRX_GroupDef def)
	{
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		FactionManager fm = GetGame().GetFactionManager();
		if (!gm || !fm) {
			Print("[FFRX][Groups] Missing GroupsManager or FactionManager.");
			return null;
		}

		Faction faction = fm.GetFactionByKey(FFRX_ResolveFactionKey(def.faction));
		if (!faction) {
			Print(string.Format("[FFRX][Groups] Unknown faction '%1' for group '%2'.", def.faction, def.groupName));
			return null;
		}

		gm.SetNewGroupsAllowed(true); // creating requires this at the moment of call

		s_bCreatingPredefined = true;
		SCR_AIGroup group = gm.CreateNewPlayableGroup(faction);
		s_bCreatingPredefined = false;
		if (!group) {
			Print(string.Format("[FFRX][Groups] Failed to create group '%1'.", def.groupName));
			return null;
		}

		group.SetCustomName(def.groupName, -1);
		group.SetCustomDescription(def.description, -1);
		if (def.maxMembers > 0) group.SetMaxMembers(def.maxMembers);
		group.SetCanDeleteIfNoPlayer(false); // predefined groups persist when empty

		if (def.radioFrequencies && !def.radioFrequencies.IsEmpty()) {
			int freqKHz = def.radioFrequencies[0].frequency * 1000;
			group.SetRadioFrequency(freqKHz);
		}

		GroupIds().Insert(group.GetGroupID());
		GroupNames().Set(group.GetGroupID(), def.groupName);
		if (def.etatMajor) EtatMajorIds().Insert(group.GetGroupID());
		if (def.genie)     GenieIds().Insert(group.GetGroupID());
		if (def.medic)     MedicIds().Insert(group.GetGroupID());
		if (def.allowedPlayerUIDs && !def.allowedPlayerUIDs.IsEmpty()) {
			array<string> wl = {};
			foreach (string uid : def.allowedPlayerUIDs) {
				if (uid != "") wl.Insert(uid);
			}
			if (!wl.IsEmpty()) {
				Whitelist().Set(group.GetGroupID(), wl);
				Print(string.Format("[FFRX][Groups] '%1' is whitelisted (%2 UID(s)).", def.groupName, wl.Count()));
			}
		}

		Print(string.Format("[FFRX][Groups] Created '%1' (faction %2, id %3).",
			def.groupName, def.faction, group.GetGroupID()));

		return group;
	}
}

// ----------------------------------------------------------------------------

modded class SCR_BaseGameMode
{
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer()) return;

		// FF's playable worlds (Arland/Everon) ship without a SCR_LoadoutManager -> spawn one if
		// missing (fixes "Loadout manager is missing in the world!" + base-game loadout features).
		FFRX_WorldFix.EnsureLoadoutManager();

		FFRX_GroupsManager.ApplyFromProfile();

		// Protect predefined names: the base game resets a group's custom name to
		// its callsign when a player leads it. Re-apply our RP name whenever it
		// changes.
		SCR_AIGroup.GetOnCustomNameChanged().Insert(FFRX_OnGroupNameChanged);

		// Amorcage de TOUS nos systemes serveur, centralise dans FFRX_Boot.
		// Le meme appel est refait par FFRX_BootSystem apres chaque hot-reload : sans lui,
		// un Shift+F7 eteint silencieusement tous ces systemes (OnGameModeStart ne rejoue
		// pas, mais la VM et ses CallLater sont detruits). Voir FFRX_Boot.c.
		FFRX_Boot.All(true);
	}

	protected void FFRX_OnGroupNameChanged(SCR_AIGroup group)
	{
		if (!group) return;

		int id = group.GetGroupID();
		if (!FFRX_GroupsManager.GroupNames().Contains(id)) return;

		string saved = FFRX_GroupsManager.GroupNames().Get(id);
		if (group.GetCustomName() != saved)
			group.SetCustomName(saved, 0);
	}
}

modded class SCR_GroupsManagerComponent
{
	// FF disables the group menu on FreedomFighters_Base.et (m_bAllowGroupMenu 0),
	// which stops the P menu from opening. Force it back on so players can open
	// the menu and JOIN the predefined squads.
	override bool IsGroupMenuAllowed()
	{
		return true;
	}

	// Block players from creating their own groups (they join predefined squads).
	override bool CanCreateNewGroup(notnull Faction newGroupFaction)
	{
		if (FFRX_GroupsManager.s_bBlockFreeCreation)
			return false;

		return super.CanCreateNewGroup(newGroupFaction);
	}

	// Catch-all: block EVERY group creation except our own predefined creation,
	// so no throwaway per-player group (nor an FF-config predefined group) can be
	// made. Our creation sets s_bCreatingPredefined around each call.
	override SCR_AIGroup CreateNewPlayableGroup(Faction faction, SCR_EGroupRole groupRole = SCR_EGroupRole.NONE)
	{
		if (FFRX_GroupsManager.s_bBlockFreeCreation && !FFRX_GroupsManager.s_bCreatingPredefined) {
			// Return the DEFAULT predefined squad instead of null. null would NPE FF's
			// respawn SetupPlayerGroup (which calls this for a group-less player); the
			// default squad makes that player join it. Replaces the old modded
			// JWK_RespawnSystemComponent.SetupPlayerGroup (dropped to avoid the EPF poison).
			SCR_AIGroup def = FFRX_GroupsManager.GetDefaultGroup();
			if (def) {
				Print("[FFRX][Groups] Non-predefined creation -> default squad.");
				return def;
			}
			Print("[FFRX][Groups] Blocked a non-predefined group creation (no default yet).");
			return null;
		}

		return super.CreateNewPlayableGroup(faction, groupRole);
	}

	// Whitelist gate (e.g. KILO Commandement): a group with allowedPlayerUIDs only
	// admits listed UIDs. Gate BOTH manager join paths BEFORE the old-group removal.
	// On deny, return previousGroupID so the caller's "if (result != m_iGroupID)"
	// is false -> the player is NOT removed and stays cleanly in their current group.
	override int MovePlayerToGroup(int playerID, int previousGroupID, int newGroupID)
	{
		if (!FFRX_GroupsManager.IsPlayerAllowedInGroup(newGroupID, playerID)) {
			FFRX_GroupsManager.NotifyJoinDenied(playerID);
			return previousGroupID;
		}
		int result = super.MovePlayerToGroup(playerID, previousGroupID, newGroupID);
		FFRX_GrantVehicleKeysIfEtatMajor(playerID, result);
		return result;
	}

	override int AddPlayerToGroup(int groupID, int playerID)
	{
		if (!FFRX_GroupsManager.IsPlayerAllowedInGroup(groupID, playerID)) {
			FFRX_GroupsManager.NotifyJoinDenied(playerID);
			return -1;
		}
		int result = super.AddPlayerToGroup(groupID, playerID);
		if (result != -1)
			FFRX_GrantVehicleKeysIfEtatMajor(playerID, groupID);
		return result;
	}

	// If the player just landed in an etat-major squad, give them the keys to every
	// already-locked procured vehicle (dynamic pass-key). Server only.
	protected void FFRX_GrantVehicleKeysIfEtatMajor(int playerID, int groupID)
	{
		if (!Replication.IsServer()) return;
		if (groupID < 0) return;
		if (!FFRX_GroupsManager.EtatMajorIds().Contains(groupID)) return;
		FFRX_VehicleLock.GrantEtatMajorAccess(playerID);
	}
}

// Stop the base game from auto-creating a fresh group for a player on spawn /
// faction change: SCR_GroupsManagerComponent.OnPlayerFactionChanged returns
// early when the faction is "predefined groups only", so players stay free to
// join one of our predefined squads instead of getting a throwaway group.
modded class SCR_Faction
{
	override bool GetCanCreateOnlyPredefinedGroups()
	{
		if (FFRX_GroupsManager.s_bBlockFreeCreation)
			return true;

		return super.GetCanCreateOnlyPredefinedGroups();
	}
}

// NOTE: the modded JWK_RespawnSystemComponent (SetupPlayerGroup default-squad
// assignment AND the auto-deploy CreateCharacter) lives in FFRX_AutoSpawn.c --
// Enfusion allows only ONE modded block per class per addon, so both overrides
// are merged there.
