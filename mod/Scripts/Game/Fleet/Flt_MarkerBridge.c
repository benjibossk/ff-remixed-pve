// ============================================================================
//  Flt_MarkerBridge — pont bidirectionnel entre les marqueurs Anarchy Markers
//  et l'interface web (D:\Serveur\marker-web).
//
//  DÉPENDANCE : ce fichier référence les classes d'Anarchy Markers
//  (SM_MapMarkerStore, SM_MapMarkerData, SM_MarkerNet, SM_EMarkerVisibility).
//  => Ajouter le GUID "69A510CE600D1126" dans les Dependencies du .gproj Fleet,
//     et garder Anarchy Markers chargé, sinon ce fichier ne compile pas.
//
//  Deux boucles (serveur uniquement) :
//    1. PUSH    : SM_MapMarkerStore.GetAll() -> JSON -> POST /api/markers
//    2. PULL    : GET /api/commands -> applique chaque ordre sur le store
//                 (ServerCreate/Move/Update/Remove + SM_MarkerNet.Broadcast*)
//
//  Config : $profile:Fleet/MarkerBridge.json  { baseUrl, token, intervalMs }
// ============================================================================

// ---------------------------------------------------------------------------
//  Config
// ---------------------------------------------------------------------------
class Flt_MB_ConfigData
{
	string baseUrl    = "http://localhost:8090";	// racine du serveur web (sans slash final)
	string token      = "changeme";					// doit correspondre à config.json du serveur web
	int    intervalMs = 2000;						// période push + pull
}

class Flt_MB_Config
{
	const string FILE = "$profile:Fleet/MarkerBridge.json";
	ref Flt_MB_ConfigData data;

	void Flt_MB_Config()
	{
		data = new Flt_MB_ConfigData();
		if (!FileIO.FileExists("$profile:Fleet/"))
			FileIO.MakeDirectory("$profile:Fleet/");
		if (!FileIO.FileExists(FILE))
		{
			Save();	// créer les défauts au premier lancement
			Print("[MB] Config créée : " + FILE, LogLevel.NORMAL);
		}
		else
		{
			Load();
		}
	}

	void Load()
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (ctx.LoadFromFile(FILE) && ctx.ReadValue("", data))
			Print("[MB] Config chargée", LogLevel.NORMAL);
		else
			Print("[MB] Config illisible — défauts utilisés", LogLevel.WARNING);
	}

	void Save()
	{
		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", data);
		ctx.SaveToFile(FILE);
	}
}

// ---------------------------------------------------------------------------
//  Objets de sérialisation SORTANTE (jeu -> web). Champs publics : le
//  SCR_JsonSaveContext sérialise un array<ref X> en tableau JSON d'objets.
// ---------------------------------------------------------------------------
class Flt_MB_MarkerOut
{
	int id, owner, x, y, kind, icon, ident, dim, sym, color, rot, size, vis, ch;
	string text;
	string editor;
}
class Flt_MB_PlayerOut
{
	string alias;
	int x, y;
	string faction;
	int status;
	string uid;		// UID Reforger (clé Discord / grade)
	int rank;		// grade actuel affiché (SCR_ECharacterRank)
	int xp;			// XP courant — le portail compare au seuil du grade (vue "à engueuler")
}
class Flt_MB_DrawingOut
{
	int id, owner, color, w, vis, ch, fill;
	string author;
	ref array<int> pts = {};	// x,z par paires (mètres)
}
class Flt_MB_PresetOut
{
	int kind, ident, dim, sym, color, size, vis;
	string label;
}
// Un grade disponible dans le scénario, par faction. Envoyé au site/Discord pour
// que les menus n'affichent QUE des grades valides (ils sont configurables par map).
class Flt_MB_RankOut
{
	string faction;		// clé de faction (US / RU / FIA ...)
	int    rank;		// SCR_ECharacterRank (valeur enum) — la clé stable
	string name;		// nom lisible
	string nameShort;	// nom court
	string nameUpper;	// nom en majuscules
	string insignia;	// quad de l'insigne (MilitaryIcons.imageset)
}
class Flt_MB_Payload
{
	string mapName = "UNK";
	int    worldSize = 0;	// arête du terrain en mètres (pour l'échelle de la carte web)
	int    worldOffX = 0;	// origine monde de la carte (coin SO), X
	int    worldOffZ = 0;	// origine monde de la carte (coin SO), Z
	ref array<ref Flt_MB_MarkerOut> markers = {};
	ref array<ref Flt_MB_PlayerOut> players = {};
	ref array<ref Flt_MB_DrawingOut> drawings = {};
	ref array<ref Flt_MB_PresetOut> presets = {};
	ref array<ref Flt_MB_RankOut> ranks = {};	// grades dispo par faction (statique par scénario)
}

// ---------------------------------------------------------------------------
//  Parsing ENTRANT (web -> jeu). Format "à plat" servi par /api/commands.
//  JsonApiStruct.ExpandFromRAW(string) parse une chaîne JSON dans les membres.
// ---------------------------------------------------------------------------
class Flt_MB_Cmd : JsonApiStruct
{
	int    cmdId;
	string type;		// "place"|"move"|"edit"|"remove"|"draw_remove"|"setrank"|"objective"|"obj_remove"|"obj_done"
	int    id;
	int    x, y, kind, icon, ident, dim, sym, color, rot, size, vis, ch;
	string text;
	string uid;			// setrank/objective(player) : UID Reforger ciblé
	int    rank;		// setrank : grade voulu (SCR_ECharacterRank)
	string objtarget;	// objective : "player" | "squad" | "faction"
	string objname;		// objective : titre
	string objdesc;		// objective : description
	string faction;		// objective(faction) : clé de faction

	void Flt_MB_Cmd()
	{
		RegV("cmdId"); RegV("type"); RegV("id");
		RegV("x"); RegV("y"); RegV("kind"); RegV("icon"); RegV("ident");
		RegV("dim"); RegV("sym"); RegV("color"); RegV("rot"); RegV("size");
		RegV("vis"); RegV("ch"); RegV("text");
		RegV("uid"); RegV("rank");
		RegV("objtarget"); RegV("objname"); RegV("objdesc"); RegV("faction");
	}
}
class Flt_MB_CmdList : JsonApiStruct
{
	ref array<ref Flt_MB_Cmd> commands;
	void Flt_MB_CmdList() { RegV("commands"); }
}

// ---------------------------------------------------------------------------
//  Callbacks REST
// ---------------------------------------------------------------------------
class Flt_MB_PushCallback : RestCallback
{
	protected static int s_iOkCount;
	void Flt_MB_PushCallback()
	{
		SetOnSuccess(Flt_OnOk);
		SetOnError(Flt_OnErr);	// gère aussi le timeout
	}
	protected void Flt_OnOk(RestCallback cb)  { s_iOkCount++; }
	protected void Flt_OnErr(RestCallback cb) { Print("[MB] push error/timeout", LogLevel.WARNING); }
}

class Flt_MB_PullCallback : RestCallback
{
	protected static int s_iMiss;
	void Flt_MB_PullCallback()
	{
		SetOnSuccess(Flt_OnOk);
		SetOnError(Flt_OnFail);	// gère aussi le timeout
	}
	protected void Flt_OnOk(RestCallback cb)   { Flt_MarkerBridge.GetInstance().ApplyCommandsJson(cb.GetData()); }
	protected void Flt_OnFail(RestCallback cb) { s_iMiss++; /* pas d'ordre / serveur down : silencieux */ }
}

// ---------------------------------------------------------------------------
//  Le pont
// ---------------------------------------------------------------------------
class Flt_MarkerBridge
{
	protected static ref Flt_MarkerBridge s_Instance;
	protected ref Flt_MB_Config m_Config;
	protected bool m_bStarted;
	protected bool m_bLoggedMap;
	protected string m_sMapName = "UNK";
	protected ref array<ref Flt_MB_RankOut> m_aRanksCache;	// grades dispo, construits une seule fois

	// Marqueurs posés depuis le WEB : id -> UID du poseur (Anarchy ne stocke qu'un playerId).
	protected static ref map<int, string> s_mWebOwner = new map<int, string>();
	static void Flt_SetWebOwner(int id, string uid)
	{
		if (uid != "")
			s_mWebOwner.Set(id, uid);
	}
	static string Flt_GetWebOwner(int id)
	{
		string u;
		if (s_mWebOwner.Find(id, u))
			return u;
		return "";
	}

	static Flt_MarkerBridge GetInstance()
	{
		if (!s_Instance)
			s_Instance = new Flt_MarkerBridge();
		return s_Instance;
	}

	void StartServer()
	{
		if (!Replication.IsServer() || m_bStarted)
			return;

		m_Config = new Flt_MB_Config();

		string wf = GetGame().GetWorldFile();
		if (wf && wf != "")
			m_sMapName = wf;

		int itv = m_Config.data.intervalMs;
		if (itv < 500)
			itv = 500;

		m_bStarted = true;
		GetGame().GetCallqueue().CallLater(Tick, itv, true);
		Print("[MB] Bridge démarré -> " + m_Config.data.baseUrl + " toutes les " + itv + " ms", LogLevel.NORMAL);
	}

	void StopServer()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);
		m_bStarted = false;
	}

	// Une itération : pousser l'état, puis tirer les ordres.
	protected void Tick()
	{
		if (!Replication.IsServer())
			return;
		PushState();
		PullCommands();
	}

	// Libellé lisible d'un symbole militaire APP-6 (pour les boutons presets du web).
	static string Flt_MB_MilLabel(int ident, int dim, int sym)
	{
		string idn = "Mil";
		if (ident == EMilitarySymbolIdentity.BLUFOR)  idn = "Ally";
		else if (ident == EMilitarySymbolIdentity.OPFOR)   idn = "Enemy";
		else if (ident == EMilitarySymbolIdentity.INDFOR)  idn = "Enemy2";
		else if (ident == EMilitarySymbolIdentity.UNKNOWN) idn = "Unknown";

		string sy = "";
		if (sym == EMilitarySymbolIcon.INFANTRY)   sy = " Inf";
		else if (sym == EMilitarySymbolIcon.ARMOR) sy = " Armor";
		else if (dim == EMilitarySymbolDimension.INSTALLATION) sy = " Base";

		return idn + sy;
	}

	// Construit UNE fois la liste des grades disponibles, faction par faction.
	// Statique par scénario -> on met en cache et on réutilise à chaque push.
	protected void BuildRanks()
	{
		if (m_aRanksCache)	// déjà construit
			return;

		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!fm)
			return;	// pas encore prêt : on réessaiera au prochain tick

		array<Faction> factions = {};
		fm.GetFactionsList(factions);
		if (factions.IsEmpty())
			return;	// pas encore peuplé : réessai au prochain tick

		array<ref Flt_MB_RankOut> built = {};
		foreach (Faction f : factions)
		{
			SCR_Faction sf = SCR_Faction.Cast(f);
			if (!sf)
				continue;

			SCR_RankContainer rc = sf.GetRanks();
			if (!rc)
				continue;	// cette faction n'a pas de grades configurés

			string facKey = sf.GetFactionKey();

			array<ref SCR_RankInfo> ranks = rc.GetAllRanks();
			foreach (SCR_RankInfo ri : ranks)
			{
				if (!ri)
					continue;
				Flt_MB_RankOut o = new Flt_MB_RankOut();
				o.faction   = facKey;
				o.rank      = ri.GetRankID();
				o.name      = ri.GetRankName();
				o.nameShort = ri.GetRankNameShort();
				o.nameUpper = ri.GetRankNameUpperCase();
				o.insignia  = ri.GetRankInsignia();
				built.Insert(o);
			}
		}

		if (built.IsEmpty())
			return;	// rien de valide : on garde le cache null pour réessayer

		m_aRanksCache = built;
		Print(string.Format("[MB] Grades publiés : %1 entrées (toutes factions)", built.Count()), LogLevel.NORMAL);
	}

	// -------- JEU -> WEB --------
	protected void PushState()
	{
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		Flt_MB_Payload payload = new Flt_MB_Payload();
		payload.mapName = m_sMapName;

		// Grades dispo (construits une fois, réutilisés) -> le site/Discord n'affiche que du valide
		BuildRanks();
		if (m_aRanksCache)
			payload.ranks = m_aRanksCache;

		// VRAIE taille du terrain via la carte du jeu (SCR_MapEntity.GetMapSizeX/Y, en mètres).
		// C'est ce qu'utilise la carte in-game — carré, aligné aux tuiles. Fallback = boîte des entités.
		int ms = 0;
		SCR_MapEntity mapEnt = SCR_MapEntity.GetMapInstance();
		if (mapEnt)
		{
			int msx = mapEnt.GetMapSizeX();
			int msy = mapEnt.GetMapSizeY();
			ms = msx;
			if (msy > ms)
				ms = msy;

			// Origine monde de la carte (coin SO) — si ≠ 0, l'image doit être placée là, pas à (0,0).
			vector off = mapEnt.Offset();
			payload.worldOffX = off[0];
			payload.worldOffZ = off[2];

			if (!m_bLoggedMap)
			{
				m_bLoggedMap = true;
				vector sz = mapEnt.Size();
				Print(string.Format("[MB] MAP sizeX=%1 sizeY=%2 offset=(%3, %4, %5) size=(%6, %7, %8)",
					msx, msy, off[0], off[1], off[2], sz[0], sz[1], sz[2]), LogLevel.NORMAL);
			}
		}
		if (ms > 0)
		{
			payload.worldSize = ms;
		}
		else
		{
			BaseWorld bw = GetGame().GetWorld();
			if (bw)
			{
				vector mins, maxs;
				bw.GetBoundBox(mins, maxs);
				float sx = maxs[0] - mins[0];
				float sz = maxs[2] - mins[2];
				float edge = sx;
				if (sz > edge)
					edge = sz;
				payload.worldSize = edge;
			}
		}

		// Marqueurs depuis le store autoritaire d'Anarchy Markers
		array<SM_MapMarkerData> all = {};
		SM_MapMarkerStore.GetInstance().GetAll(all);
		foreach (SM_MapMarkerData m : all)
		{
			if (!m)
				continue;
			Flt_MB_MarkerOut o = new Flt_MB_MarkerOut();
			o.id = m.m_iId;         o.owner = m.m_iOwnerId;
			o.x = m.m_iPosX;        o.y = m.m_iPosY;
			o.kind = m.m_iKind;     o.icon = m.m_iIconEntry;
			o.ident = m.m_iIdentity; o.dim = m.m_iDimension; o.sym = m.m_iSymbolFlags;
			o.color = m.m_iColor;   o.rot = m.m_iRotation;   o.size = m.m_iSize;
			o.vis = m.m_iVisibility; o.ch = m.m_iChannel;
			o.text = m.m_sText;     o.editor = m.m_sLastEditor;
			payload.markers.Insert(o);
		}

		// Dessins (crayon) depuis SM_MapDrawingStore
		array<SM_MapDrawingData> draws = {};
		SM_MapDrawingStore.GetInstance().GetAll(draws);
		foreach (SM_MapDrawingData d : draws)
		{
			if (!d)
				continue;
			Flt_MB_DrawingOut od = new Flt_MB_DrawingOut();
			od.id = d.m_iId;       od.owner = d.m_iOwnerId;
			od.color = d.m_iColor; od.w = d.m_iWidthIdx;
			od.vis = d.m_iVisibility; od.ch = d.m_iChannel;
			od.fill = d.m_iFill;   od.author = d.m_sOwnerName;
			od.pts.Copy(d.m_aPoints);	// x,z par paires
			payload.drawings.Insert(od);
		}

		// Presets militaires intégrés d'Anarchy (pour proposer les mêmes en jeu et sur le web)
		array<ref SM_MapMarkerData> mil = SM_MapMarkerPresets.GetInstance().GetMilitary();
		if (mil)
		{
			foreach (SM_MapMarkerData pr : mil)
			{
				if (!pr)
					continue;
				Flt_MB_PresetOut op = new Flt_MB_PresetOut();
				op.kind = pr.m_iKind; op.ident = pr.m_iIdentity; op.dim = pr.m_iDimension;
				op.sym = pr.m_iSymbolFlags; op.color = pr.m_iColor;
				op.size = pr.m_iSize; op.vis = pr.m_iVisibility;
				op.label = Flt_MB_MilLabel(pr.m_iIdentity, pr.m_iDimension, pr.m_iSymbolFlags);
				payload.presets.Insert(op);
			}
		}

		// Joueurs (positions, pour la carte)
		PlayerManager pm = GetGame().GetPlayerManager();
		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		BackendApi ba = GetGame().GetBackendApi();
		if (pm)
		{
			array<int> ids = {};
			pm.GetPlayers(ids);
			foreach (int pid : ids)
			{
				IEntity ent = pm.GetPlayerControlledEntity(pid);
				if (!ent)
					continue;
				Flt_MB_PlayerOut p = new Flt_MB_PlayerOut();
				p.alias = pm.GetPlayerName(pid);
				vector pos = ent.GetOrigin();
				p.x = pos[0];
				p.y = pos[2];	// Z monde = axe "nord" ; on garde x/y = (X, Z)
				if (fm)
				{
					Faction f = fm.GetPlayerFaction(pid);
					if (f)
						p.faction = f.GetFactionKey();
				}

				// UID (clé Discord) + grade affiché + XP courant (pour la vue "à engueuler")
				if (ba)
					p.uid = ba.GetPlayerIdentityId(pid);
				p.rank = (int)SCR_CharacterRankComponent.GetCharacterRank(ent);
				PlayerController pc = pm.GetPlayerController(pid);
				if (pc)
				{
					SCR_PlayerXPHandlerComponent xph = SCR_PlayerXPHandlerComponent.Cast(pc.FindComponent(SCR_PlayerXPHandlerComponent));
					if (xph)
						p.xp = xph.GetPlayerXP();
				}

				payload.players.Insert(p);
			}
		}

		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		if (!ctx.WriteValue("", payload))
			return;
		string json = ctx.ExportToString();
		if (json == "")
			return;

		RestContext rc = api.GetContext(m_Config.data.baseUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Content-Type,application/json,X-Auth-Token,%1", m_Config.data.token));
		rc.POST(new Flt_MB_PushCallback(), "/api/markers", json);
	}

	// -------- WEB -> JEU --------
	protected void PullCommands()
	{
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;
		RestContext rc = api.GetContext(m_Config.data.baseUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("X-Auth-Token,%1", m_Config.data.token));
		rc.GET(new Flt_MB_PullCallback(), "/api/commands");
	}

	// Appelé par le callback GET : parse le JSON et applique les ordres.
	void ApplyCommandsJson(string json)
	{
		if (!Replication.IsServer() || json == "")
			return;

		Flt_MB_CmdList list = new Flt_MB_CmdList();
		list.ExpandFromRAW(json);
		if (!list.commands || list.commands.IsEmpty())
			return;

		foreach (Flt_MB_Cmd c : list.commands)
		{
			if (!c)
				continue;
			ApplyOne(c);
		}
	}

	protected void ApplyOne(Flt_MB_Cmd c)
	{
		SM_MapMarkerStore store = SM_MapMarkerStore.GetInstance();

		if (c.type == "place")
		{
			SM_MapMarkerData d = new SM_MapMarkerData();
			d.m_iPosX = c.x;   d.m_iPosY = c.y;
			d.m_iKind = c.kind; d.m_iIconEntry = c.icon;
			d.m_iIdentity = c.ident; d.m_iDimension = c.dim; d.m_iSymbolFlags = c.sym;
			d.m_iColor = c.color; d.m_iRotation = c.rot;
			d.m_iSize = c.size;
			if (d.m_iSize <= 0) d.m_iSize = 200;
			d.m_iVisibility = c.vis; d.m_iChannel = c.ch;
			d.m_sText = c.text;

			// owner -1 = marqueur serveur ; ServerCreate assigne l'id et repart de packed
			SM_MapMarkerData created = store.ServerCreate(-1, d.PackInts(), c.text);
			if (created)
			{
				created.m_sLastEditor = "Web";
				Flt_SetWebOwner(created.m_iId, c.uid);	// attribue le marqueur à l'utilisateur web (UID)
				SM_MarkerNet.BroadcastUpsert(created);
				Print(string.Format("[MB] WEB PLACE #%1 (%2,%3) vis=%4", created.m_iId, c.x, c.y, c.vis), LogLevel.NORMAL);
			}
		}
		else if (c.type == "move")
		{
			if (store.ServerMove(c.id, c.x, c.y))
			{
				SM_MapMarkerData m = store.FindById(c.id);
				if (m)
					SM_MarkerNet.BroadcastUpsert(m);
				Print(string.Format("[MB] WEB MOVE #%1 -> (%2,%3)", c.id, c.x, c.y), LogLevel.NORMAL);
			}
		}
		else if (c.type == "edit")
		{
			SM_MapMarkerData m = store.FindById(c.id);
			if (!m)
				return;
			// on repart de l'existant et on écrase les champs fournis par le web
			m.m_iPosX = c.x; m.m_iPosY = c.y;
			m.m_iKind = c.kind; m.m_iIconEntry = c.icon;
			m.m_iIdentity = c.ident; m.m_iDimension = c.dim; m.m_iSymbolFlags = c.sym;
			m.m_iColor = c.color; m.m_iRotation = c.rot;
			if (c.size > 0) m.m_iSize = c.size;
			m.m_iVisibility = c.vis; m.m_iChannel = c.ch;
			if (store.ServerUpdate(c.id, m.PackInts(), c.text))
			{
				SM_MapMarkerData upd = store.FindById(c.id);
				if (upd)
				{
					upd.m_sLastEditor = "Web";
					SM_MarkerNet.BroadcastUpsertOrRemove(upd);
				}
				Print(string.Format("[MB] WEB EDIT #%1", c.id), LogLevel.NORMAL);
			}
		}
		else if (c.type == "remove")
		{
			if (store.ServerRemove(c.id))
			{
				SM_MarkerNet.BroadcastRemove(c.id);
				Print(string.Format("[MB] WEB REMOVE #%1", c.id), LogLevel.NORMAL);
			}
		}
		else if (c.type == "draw_remove")
		{
			// Suppression d'un dessin (crayon) depuis le web : retire du store + broadcast clients.
			if (SM_MapDrawingStore.GetInstance().ServerRemove(c.id))
			{
				SM_DrawingNet.BroadcastRemove(c.id);
				Print(string.Format("[MB] WEB DRAW_REMOVE #%1", c.id), LogLevel.NORMAL);
			}
		}
		else if (c.type == "setrank")
		{
			// Grade fixe piloté par le portail/Discord : on enregistre (persistant)
			// puis on applique tout de suite si le joueur est en ligne.
			Flt_RankRegistry.GetInstance().SetAssignedRank(c.uid, c.rank);
			Flt_RankApply.ApplyToUID(c.uid, c.rank);
			Print(string.Format("[MB] WEB SETRANK %1 -> %2", c.uid, c.rank), LogLevel.NORMAL);
		}
		else if (c.type == "objective")
		{
			// Objectif (tâche J) assigné depuis le site : x=X est, y=Z nord (mètres monde).
			vector pos = Vector(c.x, 0, c.y);
			Flt_Objectives.Create(c.objtarget, c.uid, c.id, c.faction, c.objname, c.objdesc, pos);
			Print(string.Format("[MB] WEB OBJECTIF '%1' -> %2", c.objname, c.objtarget), LogLevel.NORMAL);
		}
		else if (c.type == "obj_remove")
		{
			// Suppression d'un objectif depuis le web (par son id).
			Flt_Objectives.SetStateById(c.id, SCR_ETaskState.CANCELLED);
			Print(string.Format("[MB] WEB OBJ_REMOVE #%1", c.id), LogLevel.NORMAL);
		}
		else if (c.type == "say")
		{
			// Message ecrit depuis le site (page /journal) -> popup pour tous les joueurs.
			// Passe par la file /commands existante : aucun port RCON a ouvrir.
			Flt_EventLog.Flt_Broadcast(c.text);
		}
		else if (c.type == "obj_done")
		{
			// Objectif marqué comme accompli depuis le web (par son id).
			Flt_Objectives.SetStateById(c.id, SCR_ETaskState.COMPLETED);
			Print(string.Format("[MB] WEB OBJ_DONE #%1", c.id), LogLevel.NORMAL);
		}
	}
}

// ---------------------------------------------------------------------------
//  Boot : démarre le pont côté serveur avec un léger délai (comme la
//  persistance d'Anarchy Markers) pour laisser le store se peupler.
// ---------------------------------------------------------------------------
modded class SCR_BaseGameMode
{
	override void OnGameModeStart()
	{
		super.OnGameModeStart();
		// DÉSACTIVÉ : marker-web (8090) est un site séparé qu'on n'utilise plus.
		// Décision : TOUT passe par GTG (8080). Le bidirectionnel (marqueurs + grades)
		// sera ajouté au pipe GTG (voir Flt_GTGPositions), pas ici. Code conservé pour référence.
		//if (Replication.IsServer())
		//	GetGame().GetCallqueue().CallLater(Flt_MB_Boot, 2000, false);
	}

	protected void Flt_MB_Boot()
	{
		Flt_MarkerBridge.GetInstance().StartServer();
	}

	override void OnGameEnd()
	{
		Flt_MarkerBridge.GetInstance().StopServer();
		super.OnGameEnd();
	}
}
