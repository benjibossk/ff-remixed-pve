// FF - REMIXED - PVE
// Named "dotations" (loadouts) on the FF arsenal crate, fed by the admin web site.
//
// Flow (server authoritative):
//   FFRX_LoadoutClient.Boot() -> periodic GET /loadouts (reuses $profile:Fleet/GTG.json)
//     -> FFRX_LoadoutSystem.OnFetched -> fills FFRX_LoadoutRegistry (by faction)
//     -> pushes the ordered names of each player's faction to that client (RPC).
//   On the crate: N pre-placed FFRX_LoadoutAction (index 0..K). Each shows/names
//     itself from the CLIENT registry ("Prendre : <nom>"); PerformAction runs on the
//     server, charges the FOB supplies, then applies the loadout JSON to the player.
//
// Ported from the Fleet mod's Flt_Loadout.c (base-game only, no JWK types in the
// capture/apply core). Added here: supply cost on apply (JWK logistics), FF popup.
//
// NOTE: ASCII only in strings/comments (Enforce dedicated build desyncs on UTF-8).

// ============================================================================
//  Data structures (site transport)
//
//  L'ancien arbre maison (FFRX_LoadoutItem / FFRX_LoadoutRoot) a disparu avec la
//  refonte : la tenue est desormais la chaine produite par le serialiseur du jeu,
//  transportee telle quelle dans le champ `data`. On ne la parse plus nous-memes.
// ============================================================================
// ----------------------------------------------------------------------------
//  Reservation d'une dotation a une escouade.
//
//  La categorie 'specialty' saisie sur le site sert de reservation : si elle
//  commence par l'indicatif d'un groupe ("ECHO", "ECHO - Genie", "Echo genie"...),
//  seuls les membres de ce groupe peuvent equiper la dotation. Vide = ouverte.
//
//  On compare uniquement le PREMIER MOT, en majuscules : les noms de groupe RP
//  contiennent des tirets longs et des accents ("ECHO - Genie"), impossibles a
//  comparer de facon fiable en entier depuis un champ saisi a la main.
// ----------------------------------------------------------------------------
class FFRX_LoadoutSquad
{
	//------------------------------------------------------------------------------------------------
	//! Premier mot d'une chaine, en majuscules ("ECHO - Genie" -> "ECHO"). "" si vide.
	static string Callsign(string s)
	{
		s.TrimInPlace();
		if (s == "")
			return "";

		int cut = s.IndexOf(" ");
		if (cut > 0)
			s = s.Substring(0, cut);

		s.ToUpper();
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom RP du groupe d'un joueur, "" s'il n'est dans aucun groupe.
	static string PlayerGroupName(int playerId)
	{
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm)
			return "";

		SCR_AIGroup g = gm.GetPlayerGroup(playerId);
		if (!g)
			return "";

		string rp;
		if (FFRX_GroupsManager.s_mGroupNames.Find(g.GetGroupID(), rp) && rp != "")
			return rp;

		return g.GetCustomName();
	}

	//------------------------------------------------------------------------------------------------
	//! true si le joueur a le droit d'equiper cette dotation.
	//! outRequired = indicatif exige (pour le message), "" si la dotation est ouverte.
	static bool CanTake(FFRX_LoadoutMeta meta, int playerId, out string outRequired)
	{
		outRequired = "";
		if (!meta)
			return false;

		// Une tenue perso appartient a son proprietaire : jamais reservee a une escouade.
		if (meta.owner != "")
			return true;

		string required = Callsign(meta.specialty);
		if (required == "")
			return true;	// categorie libre / non renseignee -> ouverte a tous

		// La categorie n'est une reservation que si elle designe un vrai groupe configure.
		bool isGroup = false;
		foreach (string configured : FFRX_GroupsManager.GetConfiguredGroupNames())
		{
			if (Callsign(configured) == required)
			{
				isGroup = true;
				break;
			}
		}
		if (!isGroup)
			return true;	// simple categorie de rangement ("Infanterie", "Appui"...)

		outRequired = required;
		return (Callsign(PlayerGroupName(playerId)) == required);
	}
}

// A named loadout as stored on the site: { name, faction, data(=CaptureJson), cost }.
// 'cost' is optional (site may not send it) -> 0 means "use the action default".
class FFRX_LoadoutMeta : JsonApiStruct
{
	string name;
	string faction;
	string data;
	int    cost;
	string owner; // "" = faction dotation ; else = a player's UID (personal loadout)
	// Categorie libre saisie par l'admin sur le site. On s'en sert AUSSI comme
	// reservation d'escouade : si elle commence par un indicatif de groupe
	// (ALPHA, ECHO...), seuls les membres de ce groupe peuvent prendre la dotation.
	// Vide = ouverte a tout le monde. Cf. FFRX_LoadoutSquad.
	string specialty;

	// Computed server-side (NOT from JSON) : short content summary for the hover preview.
	string m_sSummary;

	void FFRX_LoadoutMeta() { RegV("name"); RegV("faction"); RegV("data"); RegV("cost"); RegV("owner"); RegV("specialty"); }
}

class FFRX_LoadoutList : JsonApiStruct
{
	ref array<ref FFRX_LoadoutMeta> loadouts;

	void FFRX_LoadoutList() { loadouts = {}; RegV("loadouts"); }
}

// ============================================================================
//  Registry : server = loadouts by faction (stable order) ; client = names of MY faction
// ============================================================================
class FFRX_LoadoutRegistry
{
	protected static ref FFRX_LoadoutRegistry s_Instance;
	protected ref map<string, ref array<ref FFRX_LoadoutMeta>> m_mByFaction = new map<string, ref array<ref FFRX_LoadoutMeta>>();
	// Personal loadouts, keyed by owner UID (server side).
	protected ref map<string, ref array<ref FFRX_LoadoutMeta>> m_mByOwner = new map<string, ref array<ref FFRX_LoadoutMeta>>();

	static FFRX_LoadoutRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new FFRX_LoadoutRegistry();
		return s_Instance;
	}

	//! Rebuild the registry from the full list returned by the site.
	//! owner=="" -> faction dotation (by faction key) ; owner set -> personal (by UID).
	void SetFromList(array<ref FFRX_LoadoutMeta> all)
	{
		m_mByFaction.Clear();
		m_mByOwner.Clear();
		if (!all)
			return;
		foreach (FFRX_LoadoutMeta m : all)
		{
			if (!m || m.name == "")
				continue;
			m.m_sSummary = FFRX_LoadoutSystem.BuildSummary(m.data); // content preview

			array<ref FFRX_LoadoutMeta> arr;
			if (m.owner != "")
			{
				if (!m_mByOwner.Find(m.owner, arr))
				{
					arr = {};
					m_mByOwner.Set(m.owner, arr);
				}
			}
			else
			{
				if (!m_mByFaction.Find(m.faction, arr))
				{
					arr = {};
					m_mByFaction.Set(m.faction, arr);
				}
			}
			arr.Insert(m);
		}

		FFRX_DumpKeys();
	}

	//! DIAGNOSTIC : ce que le site nous a reellement livre, range par cle.
	//! C'est le point de verite pour deux symptomes :
	//!   - "pas de dotations de base" -> m_mByFaction est vide, ou ses cles ne
	//!     correspondent pas a la faction du joueur (ex. "FIA" stocke vs "FIA_DESERT"
	//!     demande). Une dotation n'entre dans m_mByFaction QUE si owner == "".
	//!   - "la dotation d'un autre joueur" -> comparer les cles owner listees ici avec
	//!     l'uid imprime par BroadcastNames.
	void FFRX_DumpKeys()
	{
		string facs = "";
		foreach (string fk, array<ref FFRX_LoadoutMeta> fa : m_mByFaction)
		{
			if (facs != "") facs = facs + ", ";
			facs = facs + "'" + fk + "'x" + fa.Count().ToString();
		}

		string owners = "";
		foreach (string ok, array<ref FFRX_LoadoutMeta> oa : m_mByOwner)
		{
			if (owners != "") owners = owners + ", ";
			owners = owners + "'" + ok + "'x" + oa.Count().ToString();
		}

		if (facs == "")   facs = "(AUCUNE)";
		if (owners == "") owners = "(aucun)";

		Print("[FFRX][Loadout] cache faction: " + facs, LogLevel.NORMAL);
		Print("[FFRX][Loadout] cache perso  : " + owners, LogLevel.NORMAL);
	}

	//! Per-player ordered view = faction dotations of `faction` THEN this player's personal.
	//! Used by BOTH the client push and the server perform-lookup so the index stays aligned.
	array<ref FFRX_LoadoutMeta> BuildCombined(string uid, string faction)
	{
		array<ref FFRX_LoadoutMeta> combined = {};
		array<ref FFRX_LoadoutMeta> fac;
		if (m_mByFaction.Find(faction, fac) && fac)
		{
			foreach (FFRX_LoadoutMeta m : fac) combined.Insert(m);
		}
		else
		{
			// Repli de theatre : une faction derivee sans dotation propre (ex. FIA_DESERT
			// sur Anizay) sert celles de sa faction de base (FIA). Les pieces de camo sont
			// substituees a l'equipement par FFRX_LoadoutTheatre -- sinon la caisse
			// d'arsenal du theatre serait tout simplement vide. Cf. FFRX_LoadoutTheatre.c.
			string base = FFRX_LoadoutTheatre.BaseFactionKey(faction);
			if (base != "" && m_mByFaction.Find(base, fac) && fac)
				foreach (FFRX_LoadoutMeta m : fac) combined.Insert(m);
		}
		if (uid != "")
		{
			array<ref FFRX_LoadoutMeta> pers;
			if (m_mByOwner.Find(uid, pers) && pers)
				foreach (FFRX_LoadoutMeta m : pers) combined.Insert(m);
		}
		return combined;
	}

	//! Meta at `index` in this player's combined view (server perform-lookup).
	FFRX_LoadoutMeta GetForPlayer(string uid, string faction, int index)
	{
		array<ref FFRX_LoadoutMeta> combined = BuildCombined(uid, faction);
		if (index < 0 || index >= combined.Count())
			return null;
		return combined[index];
	}

	//! How many personal loadouts this player already has.
	//! Premier nom LIBRE pour une nouvelle tenue perso ("Ma tenue 1", "Ma tenue 2"...).
	//!
	//! On ne se sert PAS du nombre de tenues + 1 : le site fait un upsert sur
	//! (proprietaire, faction, nom), donc un nom deja pris ECRASE la tenue existante.
	//! Exemple vecu : 3 tenues, on supprime la 2 sur le site -> le compte retombe a 2
	//! -> la sauvegarde suivante s'appelle "Ma tenue 3" et efface la vraie 3.
	string NextPersonalName(string uid)
	{
		array<ref FFRX_LoadoutMeta> pers;
		m_mByOwner.Find(uid, pers);

		for (int n = 1; n <= 99; n++)
		{
			string candidate = "Ma tenue " + n.ToString();

			bool taken = false;
			if (pers)
			{
				foreach (FFRX_LoadoutMeta m : pers)
				{
					if (m && m.name == candidate)
					{
						taken = true;
						break;
					}
				}
			}

			if (!taken)
				return candidate;
		}

		return "Ma tenue";
	}

	//! "name<TAB>summary" per line for this player's combined view (RPC to the client).
	string ClientPayloadForPlayer(string uid, string faction)
	{
		array<ref FFRX_LoadoutMeta> combined = BuildCombined(uid, faction);
		string csv = "";
		foreach (FFRX_LoadoutMeta m : combined)
		{
			if (!m) continue;
			if (csv != "") csv += "\n";
			csv += m.name + "\t" + m.m_sSummary;
		}
		return csv;
	}

	int Count(string faction)
	{
		array<ref FFRX_LoadoutMeta> arr;
		if (m_mByFaction.Find(faction, arr) && arr)
			return arr.Count();
		return 0;
	}

	FFRX_LoadoutMeta Get(string faction, int index)
	{
		array<ref FFRX_LoadoutMeta> arr;
		if (!m_mByFaction.Find(faction, arr) || !arr)
			return null;
		if (index < 0 || index >= arr.Count())
			return null;
		return arr[index];
	}

	//! Ordered "name<TAB>summary" lines of a faction joined by '\n' (RPC to the client).
	string ClientPayload(string faction)
	{
		array<ref FFRX_LoadoutMeta> arr;
		if (!m_mByFaction.Find(faction, arr) || !arr)
			return "";
		string csv = "";
		foreach (FFRX_LoadoutMeta m : arr)
		{
			if (!m)
				continue;
			if (csv != "")
				csv += "\n";
			csv += m.name + "\t" + m.m_sSummary;
		}
		return csv;
	}

	// ---- Client side : names + content summaries of MY faction (pushed by the server) ----
	protected ref array<string> m_aClientNames = {};
	protected ref array<string> m_aClientDescs = {};

	void SetClientNamesRaw(string raw)
	{
		m_aClientNames.Clear();
		m_aClientDescs.Clear();
		if (raw == "")
			return;
		array<string> lines = {};
		raw.Split("\n", lines, false);
		foreach (string ln : lines)
		{
			if (ln == "")
				continue;
			array<string> parts = {};
			ln.Split("\t", parts, false);
			if (parts.IsEmpty() || parts[0] == "")
				continue;
			m_aClientNames.Insert(parts[0]);
			string d = "";
			if (parts.Count() >= 2)
				d = parts[1];
			m_aClientDescs.Insert(d);
		}
	}

	int ClientCount() { return m_aClientNames.Count(); }

	string ClientName(int index)
	{
		if (index < 0 || index >= m_aClientNames.Count())
			return "";
		return m_aClientNames[index];
	}

	string ClientDesc(int index)
	{
		if (index < 0 || index >= m_aClientDescs.Count())
			return "";
		return m_aClientDescs[index];
	}
}

// ============================================================================
//  Core : capture / apply / receive
// ============================================================================
class FFRX_LoadoutSystem
{
	//========================== CAPTURE ==========================
	// ------------------------------------------------------------------------------------
	//  CAPTURE -- on delegue au SERIALISEUR DE LOADOUT DU JEU DE BASE
	// ------------------------------------------------------------------------------------
	//
	// HISTORIQUE, ET POURQUOI ON A CHANGE. La version precedente parcourait elle-meme les
	// conteneurs et notait chaque objet (prefab, slot, storageIndex, enfants), puis les
	// re-spawnait un par un a la restauration. Ca n'a jamais marche correctement sur les
	// ARMES : une arme naît avec les accessoires de son prefab, et nos re-poses entraient en
	// conflit avec eux --
	//     WEAPON    (W): Weapon attachment already set !!!
	//     INVENTORY (W): VALIDATION FAILED: Stock / PistolGrip / Handguard
	// Cinq correctifs successifs (vidage, comparaison de prefabs, suppression du repli,
	// TrySpawnPrefabToStorage, test d'occupation de slot) ont chacun deplace le symptome
	// sans le regler -- jusqu'a ce que la restauration ne pose carrement plus que le gilet.
	//
	// Le moteur sait faire tout ca : `SCR_PlayerArsenalLoadout` sérialise et restaure un
	// personnage equipe, attachements compris, dans le bon ordre et le bon contexte. C'est
	// exactement ce que WCS_LoadoutEditor utilise. On garde NOTRE source (le site) et on
	// change seulement le format : le contenu devient la chaine du jeu de base.
	//
	// CREDITS : approche relevee dans WCS_LoadoutEditor (Workshop 61D57616CAFBB23D). Le mod
	// n'est pas une dependance et aucun de son code n'est repris.
	//
	// ⚠️ LE FORMAT CHANGE. Les tenues enregistrees avec l'ancien systeme ne sont plus
	// lisibles : FFRX_ApplyJson le detecte et le dit, plutot que d'equiper n'importe quoi.
	static string CaptureJson(IEntity character)
	{
		if (!character)
			return "";

		string factionKey = FactionOfChar(character);
		if (factionKey == "")
			factionKey = SCR_PlayerArsenalLoadout.ARSENALLOADOUT_FACTIONKEY_NONE;

		JsonSaveContext ctx = new JsonSaveContext();

		// La cle de faction est ecrite A LA RACINE, avant le bloc de loadout -- meme ordre
		// que SCR_ArsenalManagerComponent, sinon la relecture du jeu de base ne la trouve pas.
		if (!ctx.WriteValue(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_FACTION_KEY, factionKey))
			return "";

		if (!SCR_PlayerArsenalLoadout.ReadLoadoutString(character, ctx))
		{
			Print("[FFRX][Loadout] capture ECHOUEE (ReadLoadoutString).", LogLevel.WARNING);
			return "";
		}

		return ctx.SaveToString();
	}
	//========================== APPLY ==========================
	// ------------------------------------------------------------------------------------
	//  RESTAURATION -- meme principe : c'est le jeu de base qui rhabille le personnage
	// ------------------------------------------------------------------------------------
	//
	// `ApplyLoadoutString` recree la tenue complete, attachements compris. On ne spawne plus
	// rien nous-memes : plus de conflit avec les accessoires que le prefab d'une arme porte
	// deja, et plus de restauration qui s'arrete au premier refus.
	//
	// THEATRE (substitution de camo desert) : il vivait dans SpawnPlace, qui posait les objets
	// un par un et n'existe plus. On l'applique donc sur la CHAINE, avant de la donner au jeu :
	// elle cite les prefabs en clair, il suffit d'y remplacer les pieces de camo. Cf.
	// FFRX_LoadoutTheatre.RemapJson -- no-op hors desert.
	static bool ApplyJson(IEntity character, string json)
	{
		if (Replication.IsClient() || !character || json == "")
			return false;

		// Armer le theatre d'apres la faction du porteur (FIA_DESERT -> DESERT), substituer,
		// puis desarmer : l'etat est global, il ne doit pas fuir sur l'appel suivant.
		FFRX_LoadoutTheatre.SetCurrent(FFRX_LoadoutTheatre.TheatreOfFaction(FactionOfChar(character)));
		json = FFRX_LoadoutTheatre.RemapJson(json);
		FFRX_LoadoutTheatre.SetCurrent(FFRX_ETheatre.DEFAULT);

		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromString(json))
		{
			Print("[FFRX][Loadout] tenue illisible (JSON invalide).", LogLevel.WARNING);
			return false;
		}

		// Une tenue enregistree avec l'ANCIEN format n'a pas cette cle : on le dit
		// clairement plutot que d'equiper n'importe quoi.
		string factionKey;
		if (!ctx.ReadValue(SCR_PlayerArsenalLoadout.ARSENALLOADOUT_FACTION_KEY, factionKey))
		{
			Print("[FFRX][Loadout] tenue au FORMAT OBSOLETE (enregistree avant la refonte) -> a re-enregistrer.", LogLevel.WARNING);
			return false;
		}

		if (!SCR_PlayerArsenalLoadout.ApplyLoadoutString(character, ctx))
		{
			Print("[FFRX][Loadout] ApplyLoadoutString a echoue.", LogLevel.WARNING);
			return false;
		}

		return true;
	}


	//========================== RECEIVE (site -> game) ==========================
	//! Called by the GET callback. wantName=="*cache*" -> silently refresh the registry.
	static void OnFetched(string json)
	{
		FFRX_LoadoutList list = new FFRX_LoadoutList();
		list.ExpandFromRAW(json);
		if (!list.loadouts)
			list.loadouts = {};

		FFRX_LoadoutRegistry.Get().SetFromList(list.loadouts);
		BroadcastNames();
		Print("[FFRX][Loadout] site -> " + list.loadouts.Count() + " dotation(s) en cache", LogLevel.NORMAL);
	}

	//! Server : push each player the ordered names of THEIR faction (for the crate actions).
	static void BroadcastNames()
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
			string uid = UidOfPlayer(pid);
			string csv = FFRX_LoadoutRegistry.Get().ClientPayloadForPlayer(uid, fac);

			// DIAGNOSTIC : la faction et l'uid REELLEMENT utilises pour construire la liste.
			// A recouper avec les cles imprimees par FFRX_DumpKeys : si la faction demandee
			// ici n'est pas une cle du cache faction, c'est la cause du "pas de dotations
			// de base". Si l'uid est vide, aucune tenue perso ne devrait apparaitre.
			array<ref FFRX_LoadoutMeta> dbg = FFRX_LoadoutRegistry.Get().BuildCombined(uid, fac);
			string noms = "";
			foreach (FFRX_LoadoutMeta dm : dbg)
			{
				if (noms != "") noms = noms + " | ";
				noms = noms + dm.name + " (fac='" + dm.faction + "' owner='" + dm.owner + "')";
			}
			if (noms == "") noms = "(liste vide)";

			Print(string.Format("[FFRX][Loadout] push pid=%1 uid='%2' fac='%3' -> %4 entree(s)",
				pid, uid, fac, dbg.Count()), LogLevel.NORMAL);
			Print("[FFRX][Loadout]   " + noms, LogLevel.NORMAL);

			SCR_PlayerController ctrl = SCR_PlayerController.Cast(pm.GetPlayerController(pid));
			if (ctrl)
				ctrl.FFRX_PushLoadoutNames(csv);
		}
	}

	//! UID Reforger of a player (matches the loadout owner key), "" if unavailable.
	static string UidOfPlayer(int pid)
	{
		if (pid <= 0) return "";
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba) return "";
		return ba.GetPlayerIdentityId(pid);
	}

	//! Server -> owner client feedback in FF's hint style (from a character entity).
	static void NotifyPlayer(IEntity user, string text)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm) return;
		int pid = pm.GetPlayerIdFromControlledEntity(user);
		if (pid <= 0) return;
		PlayerController pc = pm.GetPlayerController(pid);
		if (!pc) return;
		JWK_PlayerControllerComponent jpc = JWK_PlayerControllerComponent.Cast(pc.FindComponent(JWK_PlayerControllerComponent));
		if (jpc)
			jpc.FFRX_Popup(text);
	}

	//! Faction key of a character's affiliated faction, "" if none.
	static string FactionOfChar(IEntity user)
	{
		if (!user) return "";
		FactionAffiliationComponent fc = FactionAffiliationComponent.Cast(user.FindComponent(FactionAffiliationComponent));
		if (!fc) return "";
		Faction f = fc.GetAffiliatedFaction();
		if (!f) return "";
		return f.GetFactionKey();
	}

	//! Cle de faction du joueur, avec DIAGNOSTIC de l'etape qui echoue.
	//!
	//! Le log a montre que le cas dominant est une cle VIDE : le push se produit alors que
	//! le joueur n'a pas (encore) de perso controle -- camera Game Master, ecran de spawn,
	//! ou simplement le rafraichissement periodique du site qui tombe entre deux respawns.
	//! Une cle vide ne correspond a aucune cle du cache, donc AUCUNE dotation de faction
	//! n'est poussee : c'est la cause du "pas de dotations de base".
	//!
	//! On ajoute donc un REPLI : a defaut de perso controle, on sert la faction jouable de
	//! FF (role PLAYER). C'est la bonne reponse pour un serveur PVE ou tous les joueurs
	//! sont du meme cote -- et ca vaut infiniment mieux qu'une liste vide.
	static string FactionKeyOfPlayer(PlayerManager pm, int playerId)
	{
		string reason = "";

		IEntity ent = pm.GetPlayerControlledEntity(playerId);
		if (!ent)
		{
			reason = "pas de perso controle";
		}
		else
		{
			FactionAffiliationComponent fc = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
			if (!fc)
			{
				reason = "perso sans FactionAffiliationComponent";
			}
			else
			{
				Faction f = fc.GetAffiliatedFaction();
				if (!f)
					reason = "aucune faction affiliee";
				else
					return f.GetFactionKey();
			}
		}

		// --- Repli sur la faction jouable FF ---
		string fallback = "";
		JWK_FactionManager fm = JWK.GetFactions();
		if (fm)
		{
			JWK_Faction pf = fm.GetJWKFactionByRole(JWK_EFactionRole.PLAYER);
			if (pf)
				fallback = pf.GetKey();
		}

		Print(string.Format("[FFRX][Loadout] faction pid=%1 indisponible (%2) -> repli '%3'",
			playerId, reason, fallback), LogLevel.WARNING);

		return fallback;
	}

	protected static ResourceName GetPrefabName(IEntity entity)
	{
		if (!entity)
			return "";
		EntityPrefabData pd = entity.GetPrefabData();
		if (!pd)
			return "";
		return pd.GetPrefabName();
	}

	//========================== CONTENT SUMMARY (hover preview) ==========================

	//! Short human-readable content of a loadout (from its data JSON): weapons + mag count.
	// Depuis la refonte, `data` est la chaine du serialiseur du JEU (storages/slots imbriques)
	// et non plus notre ancien JSON a champs `weapons`/`clothing`. On ne parse donc plus la
	// structure : on releve simplement tous les prefabs cites, a n'importe quelle profondeur.
	// C'est exact pour le comptage des chargeurs, et suffisant pour nommer l'arme principale.
	static string BuildSummary(string data)
	{
		if (data == "")
			return "";

		array<string> prefabs = {};
		CollectPrefabs(data, prefabs);

		string wl = "";
		int mags = 0;
		int shown = 0;
		foreach (string p : prefabs)
		{
			if (IsMag(p))
			{
				mags++;
				continue;
			}

			if (shown >= 2 || !IsWeapon(p))
				continue;

			string sn = ShortName(p);
			if (sn == "")
				continue;

			if (wl != "") wl += ", ";
			wl += sn;
			shown++;
		}

		string s = wl;
		if (mags > 0)
		{
			if (s != "") s += " - ";
			s += string.Format("%1 chargeurs", mags);
		}
		return s;
	}

	//! Releve tous les "{GUID}chemin.et" presents dans une chaine JSON, quel que soit l'emboitement.
	//! Public : sert aussi a FFRX_LoadoutTheatre pour la substitution de camo.
	static void CollectPrefabs(string data, array<string> result)
	{
		int pos = 0;
		while (true)
		{
			const int b = data.IndexOfFrom(pos, "{");
			if (b < 0)
				break;

			const int e = data.IndexOfFrom(b, ".et");
			if (e < 0)
				break;

			// Un prefab valide est un seul token JSON : pas de guillemet entre l'accolade et .et
			string token = data.Substring(b, e + 3 - b);
			if (token.Contains("}") && !token.Contains("\""))
				result.Insert(token);

			pos = e + 3;
		}
	}

	//! Arme portable (et non un accessoire : optique, poignee, silencieux...).
	protected static bool IsWeapon(string prefab)
	{
		if (prefab.Contains("/Attachments/") || prefab.Contains("/Magazines/"))
			return false;

		return prefab.Contains("/Weapons/") || prefab.Contains("/Launchers/");
	}

	protected static bool IsMag(string prefab)
	{
		return prefab.Contains("Magazine") || prefab.Contains("/Box_") || prefab.Contains("/Ammo");
	}

	//! "{GUID}Prefabs/.../HK416F-S_AimM5.et" -> "HK416F-S_AimM5"
	protected static string ShortName(string rn)
	{
		string s = rn;
		int b = s.IndexOf("}");
		if (b >= 0 && b + 1 < s.Length()) s = s.Substring(b + 1, s.Length() - b - 1);
		int slash = s.LastIndexOf("/");
		if (slash >= 0 && slash + 1 < s.Length()) s = s.Substring(slash + 1, s.Length() - slash - 1);
		int dot = s.LastIndexOf(".");
		if (dot > 0) s = s.Substring(0, dot);
		return s;
	}
}

// ============================================================================
//  Site client : GET /loadouts (server only), periodic cache refresh
// ============================================================================
class FFRX_LoadoutGetCallback : RestCallback
{
	void FFRX_LoadoutGetCallback()
	{
		SetOnSuccess(FFRX_OnOk);
		SetOnError(FFRX_OnErr);
	}

	protected void FFRX_OnOk(RestCallback cb)
	{
		FFRX_LoadoutSystem.OnFetched(cb.GetData());
		FFRX_LoadoutClient.Get().ReleaseCb(this);
	}
	protected void FFRX_OnErr(RestCallback cb)
	{
		FFRX_LoadoutClient.Get().ReleaseCb(this);
	}
}

// POST callback for publishing a loadout (self-releases from the client's keep-alive list).
class FFRX_LoadoutPostCallback : RestCallback
{
	void FFRX_LoadoutPostCallback()
	{
		SetOnSuccess(FFRX_OnOk);
		SetOnError(FFRX_OnErr);
	}

	protected void FFRX_OnOk(RestCallback cb)
	{
		// Rafraichir SEULEMENT ici : le POST est asynchrone. Relire la liste juste apres
		// l'avoir envoye (ce qu'on faisait) partait souvent avant que le site ait enregistre
		// -> la tenue n'apparaissait pas sur la caisse, et le compteur "Ma tenue N" restait
		// en retard (d'ou un nom deja pris, donc un ecrasement possible cote site).
		FFRX_LoadoutClient.Get().ReleasePostCb(this);
		FFRX_LoadoutClient.Get().Fetch();
	}
	protected void FFRX_OnErr(RestCallback cb)
	{
		Print("[FFRX][Loadout] publish error", LogLevel.WARNING);
		FFRX_LoadoutClient.Get().ReleasePostCb(this);
	}
}

class FFRX_LoadoutClient
{
	protected static ref FFRX_LoadoutClient s_Instance;

	protected string m_sUrl;
	protected string m_sApiKey;
	protected bool   m_bReady;
	protected ref array<ref FFRX_LoadoutGetCallback> m_aCb = {};
	protected ref array<ref FFRX_LoadoutPostCallback> m_aPostCb = {};

	static FFRX_LoadoutClient Get()
	{
		if (!s_Instance)
			s_Instance = new FFRX_LoadoutClient();
		return s_Instance;
	}

	static void Boot()
	{
		if (!Replication.IsServer()) return;
		Get().Start();
	}

	protected void Start()
	{
		if (m_bReady) return;

		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile("$profile:Fleet/GTG.json"))
		{
			Print("[FFRX][Loadout] $profile:Fleet/GTG.json missing -> loadouts disabled", LogLevel.WARNING);
			return;
		}

		string posUrl = "";
		string explicitUrl = "";
		ctx.ReadValue("url", posUrl);
		ctx.ReadValue("apiKey", m_sApiKey);
		ctx.ReadValue("loadoutsUrl", explicitUrl);

		if (explicitUrl != "")
			m_sUrl = explicitUrl;
		else
			m_sUrl = DeriveUrl(posUrl, "loadouts");

		if (m_sUrl == "" || m_sApiKey == "")
		{
			Print("[FFRX][Loadout] url/apiKey missing in GTG.json -> loadouts disabled", LogLevel.WARNING);
			return;
		}

		m_bReady = true;
		// Refresh the cache every 30s (first tick fires immediately after the delay).
		GetGame().GetCallqueue().CallLater(Fetch, 30000, true);
		GetGame().GetCallqueue().CallLater(Fetch, 4000, false); // one early fetch at boot
		Print("[FFRX][Loadout] client started -> " + m_sUrl, LogLevel.NORMAL);
	}

	protected string DeriveUrl(string base, string suffix)
	{
		int idx = base.IndexOf("positions");
		if (idx < 0) return "";
		return base.Substring(0, idx) + suffix;
	}

	//! GET /loadouts -> refresh the server registry + push names to clients.
	void Fetch()
	{
		if (!m_bReady) return;
		RestApi api = GetGame().GetRestApi();
		if (!api) return;
		RestContext rc = api.GetContext(m_sUrl);
		if (!rc) return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_sApiKey));
		FFRX_LoadoutGetCallback cb = new FFRX_LoadoutGetCallback();
		m_aCb.Insert(cb); // keep a ref during the async call
		rc.GET(cb, "");
	}

	void ReleaseCb(FFRX_LoadoutGetCallback cb)
	{
		if (cb)
			m_aCb.RemoveItem(cb);
	}

	//! POST a loadout to the site. owner="" = faction dotation ; owner=<UID> = personal.
	//! data = CaptureJson output (embedded as an escaped JSON string).
	void Publish(string name, string faction, string data, string owner)
	{
		if (!m_bReady) return;
		RestApi api = GetGame().GetRestApi();
		if (!api) return;
		RestContext rc = api.GetContext(m_sUrl);
		if (!rc) return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_sApiKey));
		// ⚠️ PAS de string.Format ici : il TRONQUE sa sortie a ~8 Ko sans rien dire. Depuis la
		// refonte, `data` est la chaine du serialiseur du jeu (bien plus volumineuse que
		// l'ancien format maison), et JsonEsc l'allonge encore en echappant chaque guillemet.
		// Une tenue complete depasse le seuil -> le corps partait coupe en plein milieu et le
		// site repondait 400. Concatenation incrementale : pas de limite, et pas de
		// "Formula too complex" (c'est une longue chaine de '+' d'un seul tenant qui le declenche).
		string body = "{\"name\":\"";
		body += JsonEsc(name);
		body += "\",\"faction\":\"";
		body += JsonEsc(faction);
		body += "\",\"data\":\"";
		body += JsonEsc(data);
		body += "\",\"owner\":\"";
		body += JsonEsc(owner);
		body += "\"}";
		FFRX_LoadoutPostCallback cb = new FFRX_LoadoutPostCallback();
		m_aPostCb.Insert(cb);
		rc.POST(cb, "", body);
		Print(string.Format("[FFRX][Loadout] publish '%1' owner=%2 (%3 car.)", name, owner, data.Length()), LogLevel.NORMAL);
	}

	void ReleasePostCb(FFRX_LoadoutPostCallback cb)
	{
		if (cb)
			m_aPostCb.RemoveItem(cb);
	}

	//! Minimal JSON string escaping (backslash + quote). Prefab paths use '/', no controls.
	static string JsonEsc(string s)
	{
		string e = s;
		e.Replace("\\", "\\\\");
		e.Replace("\"", "\\\"");
		return e;
	}
}

// ============================================================================
//  Crate action : "Prendre : <nom>" (one per index), charges FOB supplies
// ============================================================================
//! Etiquette lisible d'une entite porteuse d'action (prefab + position), pour les logs.
string FFRX_ActionOwnerLabel(IEntity ent)
{
	if (!ent)
		return "(entite nulle)";

	string prefab = "(sans prefab)";
	if (ent.GetPrefabData())
		prefab = ent.GetPrefabData().GetPrefabName();

	return prefab + " @ " + ent.GetOrigin().ToString();
}

//! DIAGNOSTIC PERSISTANCE : dit si la caisse posee a de quoi etre sauvegardee.
//!
//! Pourquoi ces deux composants precisement :
//!   - RplComponent : sans identite de replication, l'entite n'existe pas pour le reseau,
//!     et la persistance EPF n'a rien a quoi accrocher son enregistrement. C'est le
//!     suspect n1 : le prefab herite de CompositionBase (via FFRX_Preview_ArsenalBox),
//!     qui NE FOURNIT PAS de RplComponent racine -- contrairement a
//!     BuildableComposition_Base, dont heritent les arsenals constructibles du jeu.
//!   - EPF_PersistenceComponent : c'est lui qui inscrit l'entite au registre de sauvegarde.
//!
//! Si RPL=NON, la caisse ne sera jamais sauvegardee, quoi qu'on fasse cote script : la
//! correction est dans le prefab (changer de parent), pas dans le code.
void FFRX_LogCrateHealth(IEntity crate)
{
	if (!crate)
	{
		Print("[FFRX][Caisse] entite proprietaire introuvable.", LogLevel.WARNING);
		return;
	}

	bool hasRpl = RplComponent.Cast(crate.FindComponent(RplComponent)) != null;
	bool hasEpf = crate.FindComponent(EPF_PersistenceComponent) != null;

	string verdict = "OK";
	if (!hasRpl)
		verdict = "PAS DE REPLICATION -> ne sera JAMAIS persistee (parent de prefab a corriger)";
	else if (!hasEpf)
		verdict = "pas de composant de persistance";

	Print(string.Format("[FFRX][Caisse] %1 | RPL=%2 EPF=%3 | %4",
		FFRX_ActionOwnerLabel(crate), hasRpl, hasEpf, verdict), LogLevel.NORMAL);
}

class FFRX_LoadoutAction : ScriptedUserAction
{
	// DIAGNOSTIC : quels index de dotation existent REELLEMENT sur la caisse visee.
	// On attend 10 lignes (idx=0..9). S'il n'y en a qu'une, le prefab pose dans le monde
	// ne porte qu'une seule action -- donc il n'a pas ete rebake.
	// Un index par ligne, une seule fois chacun (voir le verrou dans SaveLoadoutAction).
	// Cle de deduplication du log : "index@entite porteuse", PAS l'index seul.
	//
	// Avec l'index seul, deux caisses posees au meme endroit (ou une caisse construite
	// par-dessus celle d'origine de la FOB) produisaient DEUX entrees identiques dans le
	// menu mais UNE SEULE ligne de log -- le doublon etait donc invisible ici, alors que
	// c'est precisement ce qu'on cherche a diagnostiquer. Cf. le doublon du 2026-09-17.
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<string> s_aIdxLogged;

	protected static array<string> IdxLogged()
	{
		if (!s_aIdxLogged)
			s_aIdxLogged = new array<string>();

		return s_aIdxLogged;
	}

	//! Compteurs du recapitulatif (cf. GetActionNameScript). Statiques : les actions sont
	//! des instances independantes, aucune ne connait les autres -- c'est le seul moyen
	//! d'obtenir un total a l'echelle de la caisse.
	protected static int s_iMaxSlot;      // plus grand index rencontre + 1 = emplacements declares
	protected static int s_iNamedSlots;   // emplacements qui resolvent un nom (donc affiches)


	[Attribute("0", UIWidgets.EditBox, "Index de la dotation (0 = 1ere) pour la faction du joueur.")]
	protected int m_iIndex;

	[Attribute("40", UIWidgets.EditBox, "Cout en ravitaillement par defaut (si le site n'en fournit pas).")]
	protected int m_iDefaultCost;

	[Attribute("80", UIWidgets.EditBox, "Rayon (m) de recherche du stock de ravitaillement autour de la caisse.")]
	protected float m_fSupplyRadius;

	// Sphere-query accumulator (server-sequential -> instance member is safe).
	protected ref array<JWK_LogisticsStorageControllerComponent> m_aScan;

	// Server: supply each player has already "sunk" into their current arsenal kit.
	// Re-equipping hands the old kit back -> we refund that first, so a swap only
	// charges the difference (swapping two same-cost dotations is free).
	protected static ref map<int, int> s_mSunkCost;

	protected static map<int, int> SunkCost()
	{
		if (!s_mSunkCost)
			s_mSunkCost = new map<int, int>();

		return s_mSunkCost;
	}

	//! Oublie ce qu'un joueur avait investi dans sa tenue.
	//! Appele par FFRX_RefundLoadoutAction : apres un remboursement le joueur ne porte
	//! plus rien, donc sa prochaine dotation doit etre facturee ENTIEREMENT. Sans ca il
	//! serait credite deux fois -- une fois par le remboursement, une fois par le
	//! "rendu" automatique calcule ici lors du prochain equipement.
	static void FFRX_ClearSunkCost(int playerId)
	{
		if (playerId > 0)
			SunkCost().Remove(playerId);
	}

	// Server: real supply cost measured per dotation name (first equip measures it via
	// the arsenal API, then it's known upfront for the affordability pre-check).
	protected static ref map<string, int> s_mLoadoutCost;

	protected static map<string, int> LoadoutCost()
	{
		if (!s_mLoadoutCost)
			s_mLoadoutCost = new map<string, int>();

		return s_mLoadoutCost;
	}

	//------------------------------------------------------------------------------------------------
	// The action is gated by the CLIENT registry (the list pushed to this player) so the
	// menu only shows loadouts that exist. BUT on a dedicated server the authority has an
	// EMPTY client registry (ClientCount()==0 -> it was never pushed to the server), and
	// the engine re-validates CanBe*Script on the authority before running the broadcast
	// PerformAction. Gating on ClientCount() there made the server REFUSE every take ->
	// "Prendre" worked in Workbench (client == server) but did nothing on the dedicated
	// server. On the authority we return true and let PerformAction validate (meta != null).
	override bool CanBeShownScript(IEntity user)
	{
		if (Replication.IsServer())
			return true;
		return FFRX_LoadoutRegistry.Get().ClientCount() > m_iIndex;
	}

	override bool CanBePerformedScript(IEntity user)
	{
		if (Replication.IsServer())
			return true;
		return FFRX_LoadoutRegistry.Get().ClientCount() > m_iIndex;
	}

	override bool GetActionNameScript(out string outName)
	{
		FFRX_LoadoutRegistry reg = FFRX_LoadoutRegistry.Get();
		string n = reg.ClientName(m_iIndex);

		// DIAGNOSTIC : c'est ICI que le moteur passe pour afficher une entree du menu
		// (CanBeShownScript ne produisait aucun log -> mauvais point d'accroche).
		// Une ligne par index, une seule fois : on voit d'un coup quels index existent
		// sur la caisse et quel nom chacun resout. Deux index qui affichent le MEME nom
		// = la caisse porte deux jeux d'actions (prefab imbrique ou deux caisses posees).
		string ownerLabel = FFRX_ActionOwnerLabel(GetOwner());
		string logKey = m_iIndex.ToString() + "@" + ownerLabel;

		if (!IdxLogged().Contains(logKey))
		{
			// Meme index deja vu sur une AUTRE entite = doublon dans le menu d'action.
			bool doublon = false;
			foreach (string seen : IdxLogged())
			{
				if (seen.IndexOf(m_iIndex.ToString() + "@") == 0)
				{
					doublon = true;
					break;
				}
			}

			IdxLogged().Insert(logKey);

			if (doublon)
			{
				// Le menu affichera l'entree DEUX fois. On ne compte pas l'emplacement une
				// seconde fois : le RECAP doit rester le nombre d'emplacements du prefab.
				Print("[FFRX][Actions] DOUBLON : l'index " + m_iIndex.ToString()
					+ " est aussi porte par " + ownerLabel
					+ " -- deux entites offrent la meme dotation (deux caisses posees l'une sur l'autre ?)", LogLevel.WARNING);
			}
			else
			{
				if (n != "")
					s_iNamedSlots = s_iNamedSlots + 1;
				if (m_iIndex + 1 > s_iMaxSlot)
					s_iMaxSlot = m_iIndex + 1;
			}

			Print(string.Format("[FFRX][Actions] dotation idx=%1 -> nom='%2' (ClientCount=%3) sur %4",
				m_iIndex, n, reg.ClientCount(), ownerLabel), LogLevel.NORMAL);

			// RECAPITULATIF, reimprime a chaque nouvel emplacement decouvert : la DERNIERE
			// ligne de ce type dans le log donne le compte complet.
			//
			// A quoi ca sert : distinguer "le prefab declare trop d'emplacements" (slots
			// eleve, beaucoup de vides) de "le site envoie trop de dotations"
			// (ClientCount eleve). Les deux gonflent le menu d'action, mais la correction
			// n'est pas au meme endroit -- prefab dans le premier cas, donnees dans le second.
			int empty = s_iMaxSlot - s_iNamedSlots;
			Print(string.Format("[FFRX][Actions] RECAP caisse : %1 emplacements declares, %2 nommes, %3 vides (ClientCount=%4)",
				s_iMaxSlot, s_iNamedSlots, empty, reg.ClientCount()), LogLevel.NORMAL);
		}

		if (n == "")
			return false;
		outName = "Prendre : " + n;
		// Content preview shown as the action's context sub-line (the menu splits on
		// "%CTX_HACK%") -> visible when the action is highlighted, without performing it.
		string d = reg.ClientDesc(m_iIndex);
		if (d != "")
			outName = outName + "%CTX_HACK%" + d;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// PerformAction is broadcast everywhere -> only the authority does the real work.
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		RplComponent rpl = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		Print(string.Format("[FFRX][Loadout] PerformAction ENTER idx=%1 proxy=%2", m_iIndex, rpl && rpl.IsProxy()));
		if (rpl && rpl.IsProxy())
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		int pid = 0;
		if (pm) pid = pm.GetPlayerIdFromControlledEntity(pUserEntity);
		string uid = FFRX_LoadoutSystem.UidOfPlayer(pid);

		string fac = FFRX_UserFaction(pUserEntity);
		// Combined view = faction dotations + this player's personal loadouts (index-aligned
		// with what was pushed to the client).
		FFRX_LoadoutMeta meta = FFRX_LoadoutRegistry.Get().GetForPlayer(uid, fac, m_iIndex);
		Print(string.Format("[FFRX][Loadout] PerformAction idx=%1 pid=%2 uid='%3' fac='%4' meta=%5", m_iIndex, pid, uid, fac, meta != null));
		if (!meta)
		{
			FFRX_Notify(pUserEntity, "Aucune dotation disponible ici.");
			return;
		}

		// Reservation d'escouade : validee ICI, sur l'autorite. Un gate dans
		// CanBePerformedScript ne servirait a rien sur le dedie (le serveur revalide
		// l'action avec un etat client vide) -- cf. FFRX_LoadoutAction.CanBeShownScript.
		string requiredSquad;
		if (!FFRX_LoadoutSquad.CanTake(meta, pid, requiredSquad))
		{
			// Le message doit GUIDER, pas seulement refuser. La version precedente, quand
			// le joueur avait deja une escouade, se contentait de constater ("tu es en X")
			// sans dire quoi faire -- c'est exactement le cas le plus frequent, et le plus
			// frustrant. On nomme toujours l'escouade requise ET le geste pour y aller.
			string mine = FFRX_LoadoutSquad.PlayerGroupName(pid);
			string msg;
			if (mine == "")
				msg = string.Format("Dotation '%1' reservee a l'escouade %2. Ouvre le menu des groupes (touche P) et rejoins %2 pour pouvoir l'equiper.",
					meta.name, requiredSquad);
			else
				msg = string.Format("Dotation '%1' reservee a l'escouade %2, or tu es en %3. Change d'escouade (touche P) ou prends une dotation non reservee.",
					meta.name, requiredSquad, mine);

			Print(string.Format("[FFRX][Loadout] REFUS escouade : pid=%1 groupe='%2' exige='%3' dotation='%4'",
				pid, mine, requiredSquad, meta.name), LogLevel.NORMAL);
			FFRX_Notify(pUserEntity, msg);
			return;
		}

		// Refund what this player already sank into their current kit -> net charge.
		int prev = 0;
		if (pid > 0) SunkCost().Find(pid, prev);

		// Supply pool: nearest FOB/depot, NEVER the crate's own storage.
		JWK_LogisticsStorageControllerComponent store = FFRX_FindSupply(pOwnerEntity.GetOrigin(), pOwnerEntity);

		// Real cost = sum of the kit's supply value (arsenal API). We only know it once
		// the kit is worn, so it's measured on the first equip and cached per dotation.
		// When the price is already known, pre-check affordability BEFORE applying.
		int known = -1;
		LoadoutCost().Find(meta.name, known);
		if (known >= 0)
		{
			int knownNet = known - prev;
			if (knownNet > 0)
			{
				if (!store)
				{
					FFRX_Notify(pUserEntity, "Pas de stock de ravitaillement ici.");
					return;
				}
				int have0 = store.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
				if (have0 < knownNet)
				{
					FFRX_Notify(pUserEntity, string.Format("Ravitaillement insuffisant (%1 requis, %2 dispo).", knownNet, have0));
					return;
				}
			}
		}

		bool ok = FFRX_LoadoutSystem.ApplyJson(pUserEntity, meta.data);
		if (!ok)
		{
			FFRX_Notify(pUserEntity, "Echec de l'equipement.");
			return;
		}

		// Measure the REAL supply cost of the kit now that it is worn, and cache it.
		int cost = FFRX_ComputeCost(pUserEntity);
		if (cost < 0)
		{
			cost = meta.cost;
			if (cost <= 0) cost = m_iDefaultCost; // arsenal manager unavailable -> fallback
		}
		LoadoutCost().Set(meta.name, cost);

		int net = cost - prev; // >0 pay the difference ; <0 change back ; 0 free swap

		if (store)
		{
			if (net > 0)
			{
				int have = store.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
				int take = net;
				if (take > have) take = have; // first-ever equip with a low depot -> take what's there
				store.TakeResources(JWK_ELogisticsResourceType.SUPPLIES, take);
			}
			else if (net < 0)
			{
				store.AddResources(JWK_ELogisticsResourceType.SUPPLIES, -net);
			}
		}
		if (pid > 0) SunkCost().Set(pid, cost);

		// ⚠️ DEUX Print, pas un seul avec 8 arguments.
		// La version d'origine passait 8 parametres a string.Format : les deux derniers
		// (%7 store, %8 owner) ressortaient VIDES dans le log du dedie. Or FFRX_Name ne
		// peut pas rendre "" -- elle rend "none" ou un nom de prefab. Le log mentait donc,
		// et on a cru un moment qu'aucun depot n'etait trouve alors qu'on n'en savait rien.
		// (string.Format a deja montre d'autres limites, cf. memoire
		// enfusion-stringformat-8kb-truncation.) On scinde : un diagnostic auquel on ne
		// peut pas se fier est pire que pas de diagnostic.
		Print(string.Format("[FFRX][Loadout] apply '%1' fac=%2 pid=%3 cost=%4 prev=%5 net=%6",
			meta.name, fac, pid, cost, prev, net), LogLevel.NORMAL);
		Print("[FFRX][Loadout]   depot=" + FFRX_Name(store) + " | caisse=" + FFRX_EntName(pOwnerEntity)
			+ " | rayon=" + m_fSupplyRadius.ToString(), LogLevel.NORMAL);

		if (net > 0)
			FFRX_Notify(pUserEntity, string.Format("Dotation '%1' equipee (-%2 ravito).", meta.name, net));
		else if (net < 0)
			FFRX_Notify(pUserEntity, string.Format("Dotation '%1' equipee (+%2 ravito rendu).", meta.name, -net));
		else
			FFRX_Notify(pUserEntity, string.Format("Dotation '%1' equipee (echange gratuit).", meta.name));
	}

	// Real supply cost of the kit currently worn by the character (arsenal API), -1 if unavailable.
	protected int FFRX_ComputeCost(IEntity character)
	{
		GameEntity ge = GameEntity.Cast(character);
		if (!ge) return -1;
		SCR_ArsenalManagerComponent mgr;
		if (!SCR_ArsenalManagerComponent.GetArsenalManager(mgr) || !mgr) return -1;
		return mgr.GetCharacterLoadoutSupplyCost(ge, false);
	}

	//------------------------------------------------------------------------------------------------
	protected string FFRX_UserFaction(IEntity user)
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

	// Nearest FF logistics storage WITH supply capacity around the crate.
	// `exclude` = the crate entity itself, so we never drain/affect the arsenal's own storage.
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
			if (exclude && st.GetOwner() == exclude) continue; // never the crate itself
			if (st.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES) <= 0) continue;
			float d = vector.Distance(pos, st.GetOwner().GetOrigin());
			if (d < bestD)
			{
				bestD = d;
				best = st;
			}
		}
		return best;
	}

	// Short label of a storage's owner entity (logging).
	protected string FFRX_Name(JWK_LogisticsStorageControllerComponent st)
	{
		if (!st || !st.GetOwner()) return "none";
		return FFRX_EntName(st.GetOwner());
	}

	protected string FFRX_EntName(IEntity e)
	{
		if (!e) return "null";
		EntityPrefabData pd = e.GetPrefabData();
		if (pd) return pd.GetPrefabName();
		return e.GetName();
	}

	protected bool FFRX_ScanStorage(IEntity e)
	{
		JWK_LogisticsStorageControllerComponent st = JWK_CompTU<JWK_LogisticsStorageControllerComponent>.FindIn(e);
		if (st) m_aScan.Insert(st);
		return true;
	}

	// Server -> owner client feedback, in FF's hint/feedback style (reuses FFRX_Popup).
	protected void FFRX_Notify(IEntity user, string text)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm) return;
		int pid = pm.GetPlayerIdFromControlledEntity(user);
		if (pid <= 0) return;
		PlayerController pc = pm.GetPlayerController(pid);
		if (!pc) return;
		JWK_PlayerControllerComponent jpc = JWK_PlayerControllerComponent.Cast(pc.FindComponent(JWK_PlayerControllerComponent));
		if (jpc)
			jpc.FFRX_Popup(text);
	}
}

// ============================================================================
//  Crate action : "Enregistrer ma tenue" -> save the player's current kit as a
//  personal loadout on the site (owner = their UID). Renamed later on the website.
// ============================================================================
class FFRX_SaveLoadoutAction : ScriptedUserAction
{
	// DIAGNOSTIC. `Init` n'est PAS appele sur une ScriptedUserAction (verifie : aucun log).
	// On se branche donc sur CanBeShownScript, par lequel le moteur passe a chaque
	// construction du menu contextuel -- mais il y passe a CHAQUE frame ou l'on vise la
	// caisse, d'ou le verrou statique : une seule ligne par session, sinon le log est
	// noye et illisible.
	//
	// Si cette ligne n'apparait JAMAIS alors que tu vises la caisse, l'action n'existe pas
	// sur l'entite posee (prefab non rebake) -- ce n'est pas un probleme de condition,
	// puisqu'on rend true sans condition juste en dessous.
	protected static bool s_bSaveShownLogged;

	override bool CanBeShownScript(IEntity user)
	{
		if (!s_bSaveShownLogged)
		{
			s_bSaveShownLogged = true;
			Print("[FFRX][Actions] SaveLoadoutAction PRESENTE sur la caisse visee.", LogLevel.NORMAL);
			FFRX_LogCrateHealth(GetOwner());
		}
		return true;
	}
	override bool CanBePerformedScript(IEntity user) { return true; }

	override bool GetActionNameScript(out string outName)
	{
		// DIAGNOSTIC : si cette ligne sort, l'action EXISTE bien sur la caisse et le
		// moteur lui demande son nom -- le probleme serait alors l'affichage (le menu
		// d'action defile : des entrees peuvent etre hors de la fenetre visible).
		// Si elle ne sort JAMAIS alors que les dotations s'affichent, l'action n'est pas
		// sur l'entite posee : le prefab du monde ne porte pas la meme liste d'actions.
		if (!s_bSaveShownLogged)
		{
			s_bSaveShownLogged = true;
			Print("[FFRX][Actions] SaveLoadoutAction PRESENTE sur " + FFRX_ActionOwnerLabel(GetOwner()), LogLevel.NORMAL);
			FFRX_LogCrateHealth(GetOwner());
		}

		outName = "Enregistrer ma tenue";
		return true;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		RplComponent rpl = RplComponent.Cast(pOwnerEntity.FindComponent(RplComponent));
		Print(string.Format("[FFRX][Loadout] SAVE PerformAction ENTER proxy=%1", rpl && rpl.IsProxy()));
		if (rpl && rpl.IsProxy())
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm) return;
		int pid = pm.GetPlayerIdFromControlledEntity(pUserEntity);
		string uid = FFRX_LoadoutSystem.UidOfPlayer(pid);
		Print(string.Format("[FFRX][Loadout] SAVE pid=%1 uid='%2'", pid, uid));
		if (uid == "")
		{
			FFRX_LoadoutSystem.NotifyPlayer(pUserEntity, "Impossible d'enregistrer (UID indisponible).");
			return;
		}

		string data = FFRX_LoadoutSystem.CaptureJson(pUserEntity);
		if (data == "")
		{
			FFRX_LoadoutSystem.NotifyPlayer(pUserEntity, "Rien a enregistrer.");
			return;
		}

		string fac = FFRX_LoadoutSystem.FactionOfChar(pUserEntity);
		string name = FFRX_LoadoutRegistry.Get().NextPersonalName(uid);

		// Le rafraichissement est declenche par le callback de succes du POST, pas ici :
		// sinon on relit la liste avant que le site ait enregistre. Cf. FFRX_LoadoutPostCallback.
		FFRX_LoadoutClient.Get().Publish(name, fac, data, uid);

		FFRX_LoadoutSystem.NotifyPlayer(pUserEntity, string.Format("Tenue enregistree : %1 (renommable sur le site).", name));
	}
}

// ============================================================================
//  RPC : server pushes this player the ordered names of their faction's loadouts.
// ============================================================================
modded class SCR_PlayerController
{
	void FFRX_PushLoadoutNames(string csv)
	{
		PlayerController local = GetGame().GetPlayerController();
		if (local && local.GetPlayerId() == GetPlayerId())
		{
			FFRX_LoadoutRegistry.Get().SetClientNamesRaw(csv); // local player (listen-host)
			Print(string.Format("[FFRX][Loadout] push (local) pid=%1 -> ClientCount=%2", GetPlayerId(), FFRX_LoadoutRegistry.Get().ClientCount()));
		}
		else
		{
			Print(string.Format("[FFRX][Loadout] push (rpc) to pid=%1 csvLen=%2", GetPlayerId(), csv.Length()));
			Rpc(FFRX_RpcRecvLoadoutNames, csv);                // remote client
		}
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcRecvLoadoutNames(string csv)
	{
		FFRX_LoadoutRegistry.Get().SetClientNamesRaw(csv);
		Print(string.Format("[FFRX][Loadout] CLIENT recv names -> ClientCount=%1", FFRX_LoadoutRegistry.Get().ClientCount()));
	}
}
