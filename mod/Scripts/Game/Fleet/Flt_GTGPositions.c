// ============================================================================
//  Flt_GTGPositions — Fleet remplace le mod "GTG LiveMap & Position Log".
//  Envoie les positions des joueurs au backend GTG (POST /api/v1/positions),
//  au FORMAT EXACT attendu par leur handler Go :
//    corps = { "<playerGUID>": "<chaîne JSON échappée>", ... }   (map[string]string)
//    header = Authorization: Bearer <clé API du serveur>
//    JSON interne = { playerGuid, playerName, factionName, factionColor:{r,g,b},
//                     positions:[{timestamp, position:{absolute,relative}, rotation, inVehicle}] }
//    inVehicle = 0/1.  Coordonnées absolues en mètres, relatives normalisées [0,1].
//
//  Config : $profile:Fleet/GTG.json  { "url", "apiKey", "intervalSec" }
//  Serveur uniquement. Aucune dépendance à leur mod.
// ============================================================================

class Flt_GTG_ConfigData
{
	// Vides par défaut : à renseigner dans $profile:Fleet/GTG.json (rien de sensible dans le script).
	string url         = "";	// ex. http://IP:8080/api/v1/positions
	string markersUrl  = "";	// ex. http://IP:8080/api/v1/markers  (auto-déduit de 'url' si vide)
	string drawingsUrl = "";	// ex. http://IP:8080/api/v1/drawings (auto-déduit de 'url' si vide)
	string commandsUrl = "";	// ex. http://IP:8080/api/v1/commands (auto-déduit ; web->jeu)
	string squadsUrl   = "";	// ex. http://IP:8080/api/v1/squads   (auto-déduit ; escouades)
	string ranksUrl    = "";	// ex. http://IP:8080/api/v1/ranks    (auto-déduit ; grades dispo)
	string coverageUrl = "";	// ex. http://IP:8080/api/v1/coverage (auto-déduit ; cercles portée radio)
	string loadoutsUrl = "";	// ex. http://IP:8080/api/v1/loadouts (auto-déduit ; loadouts nommés admin)
	string objectivesUrl = "";	// ex. http://IP:8080/api/v1/objectives (auto-déduit ; objectifs actifs)
	string leaveUrl    = "";	// ex. http://IP:8080/api/v1/leave (auto-déduit ; départ volontaire d'un joueur)
	string apiKey      = "";	// clé API du serveur GTG
	int    intervalSec = 5;		// période d'envoi (s)
}

class Flt_GTG_Config
{
	const string FILE = "$profile:Fleet/GTG.json";
	ref Flt_GTG_ConfigData data;

	void Flt_GTG_Config()
	{
		data = new Flt_GTG_ConfigData();
		if (!FileIO.FileExists("$profile:Fleet/"))
			FileIO.MakeDirectory("$profile:Fleet/");
		if (!FileIO.FileExists(FILE))
		{
			Save();
			Print("[GTGPOS] Config créée : " + FILE, LogLevel.NORMAL);
		}
		else
		{
			SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
			if (ctx.LoadFromFile(FILE) && ctx.ReadValue("", data))
				Print("[GTGPOS] Config chargée", LogLevel.NORMAL);
			else
				Print("[GTGPOS] Config illisible — défauts", LogLevel.WARNING);
		}
	}

	void Save()
	{
		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", data);
		ctx.SaveToFile(FILE);
	}
}

// NOUVELLE API RestCallback : on ENREGISTRE les handlers via SetOnSuccess/SetOnError/SetOnTimeout
// dans le constructeur (au lieu d'OVERRIDER OnSuccess/OnError/OnTimeout, l'ancienne API dépréciée
// qui faisait spammer le moteur "RestCallback: Function was not set for event: 'OnSuccess'").
// Handler = void H(RestCallback cb) ; le corps de la réponse se lit via cb.GetData().
class Flt_GTG_Callback : RestCallback
{
	protected static int s_iOkCount;
	protected string m_Label;
	void Flt_GTG_Callback(string label = "POST")
	{
		m_Label = label;
		SetOnSuccess(Flt_OnOk);
		SetOnError(Flt_OnErr);	// gère aussi le timeout (pas de SetOnTimeout dans l'API)
	}
	protected void Flt_OnOk(RestCallback cb)  { s_iOkCount++; }
	protected void Flt_OnErr(RestCallback cb) { Print("[GTGPOS] " + m_Label + " HTTP error/timeout", LogLevel.WARNING); }
}

// Callback du pull (GTG -> jeu). On délègue à la logique déjà testée de Flt_MarkerBridge :
// ApplyCommandsJson parse {commands:[...]} et applique place/move/edit/remove/setrank.
class Flt_GTG_PullCallback : RestCallback
{
	protected static int s_iPullMiss;
	void Flt_GTG_PullCallback()
	{
		SetOnSuccess(Flt_OnOk);
		SetOnError(Flt_OnFail);	// gère aussi le timeout
	}
	protected void Flt_OnOk(RestCallback cb)   { Flt_MarkerBridge.GetInstance().ApplyCommandsJson(cb.GetData()); }
	protected void Flt_OnFail(RestCallback cb) { s_iPullMiss++; /* pas d'ordre / serveur down : silencieux */ }
}

// Callback de l'envoi des grades : au 1er succès, on arrête d'envoyer (1 fois par reboot).
class Flt_GTG_RanksCallback : RestCallback
{
	protected static int s_iRankMiss;
	void Flt_GTG_RanksCallback()
	{
		SetOnSuccess(Flt_OnOk);
		SetOnError(Flt_OnErr);	// gère aussi le timeout
	}
	protected void Flt_OnOk(RestCallback cb)  { Flt_GTGPositions.GetInstance().OnRanksSent(); }
	protected void Flt_OnErr(RestCallback cb) { s_iRankMiss++; /* HTTP error/timeout : retry au prochain tick */ }
}

class Flt_GTGPositions
{
	protected static ref Flt_GTGPositions s_Instance;
	protected ref Flt_GTG_Config m_Config;
	protected ref Flt_GTG_Callback m_Cb;			// positions
	protected ref Flt_GTG_Callback m_CbMarkers;
	protected ref Flt_GTG_Callback m_CbDrawings;
	protected ref Flt_GTG_Callback m_CbSquads;
	protected ref Flt_GTG_Callback m_CbCoverage;
	protected ref Flt_GTG_Callback m_CbLink;
	protected ref Flt_GTG_PullCallback m_PullCb;
	protected ref Flt_GTG_RanksCallback m_RanksCb;
	protected ref Flt_GTG_Callback m_CbObjectives;						// push objectifs actifs (POST)
	protected string m_ObjectivesUrl;
	protected ref Flt_GTG_Callback m_CbLeave;							// départ volontaire joueur (POST)
	protected string m_LeaveUrl;
	protected string m_EventsUrl;		// journal admin (POST /events/log)
	protected ref Flt_GTG_Callback m_CbEvents;
	protected ref Flt_GTG_Callback m_CbLoadoutPost;						// publish loadout (POST)
	protected ref array<ref Flt_GTG_LoadoutGetCallback> m_aLoadoutGetCb = {};	// GET en cours (évite le GC async)
	protected string m_LoadoutsUrl;		// endpoint loadouts nommés (admin)
	protected int m_iLoadoutFetchCtr = -1;	// compteur pour le refresh périodique du cache loadouts
	protected bool m_bRanksSent;	// grades envoyés+reçus une fois (par reboot) -> stop
	protected bool m_bStarted;
	protected vector m_vBoundMin;
	protected vector m_vBoundMax;
	protected string m_MarkersUrl;		// endpoint marqueurs (déduit de l'URL positions)
	protected string m_DrawingsUrl;		// endpoint dessins
	protected string m_CommandsUrl;		// endpoint ordres web->jeu (GET, vidé à chaque appel)
	protected string m_SquadsUrl;		// endpoint escouades (groupes joueurs)
	protected string m_RanksUrl;		// endpoint grades dispo (statique)
	protected string m_RanksBody;		// corps JSON des grades, construit une fois
	protected string m_CoverageUrl;		// endpoint cercles de portée radio
	protected string m_sLastCoverageBody;	// dernier corps coverage envoyé (anti-spam : on ne renvoie que si changé)
	protected ref map<int, int> m_mMarkerSeen = new map<int, int>();	// markerId -> heure réelle (Unix) de 1re apparition
	protected ref map<string, vector> m_mLastPos = new map<string, vector>();	// uid -> dernière position AVEC radio (brouillard de guerre)
	protected ref map<string, int> m_mLastSeen = new map<string, int>();		// uid -> Unix de cette dernière position

	static Flt_GTGPositions GetInstance()
	{
		if (!s_Instance)
			s_Instance = new Flt_GTGPositions();
		return s_Instance;
	}

	void StartServer()
	{
		if (!Replication.IsServer() || m_bStarted)
			return;

		m_Config = new Flt_GTG_Config();
		m_Cb = new Flt_GTG_Callback("positions");
		m_CbMarkers = new Flt_GTG_Callback("markers");
		m_CbDrawings = new Flt_GTG_Callback("drawings");
		m_CbSquads = new Flt_GTG_Callback("squads");
		m_CbCoverage = new Flt_GTG_Callback("coverage");
		m_CbLink = new Flt_GTG_Callback("link");
		m_PullCb = new Flt_GTG_PullCallback();
		m_RanksCb = new Flt_GTG_RanksCallback();
		m_CbLoadoutPost = new Flt_GTG_Callback("loadout");
		m_CbObjectives = new Flt_GTG_Callback("objectives");
		m_CbLeave = new Flt_GTG_Callback("leave");
		m_CbEvents = new Flt_GTG_Callback("events");

		// Rien envoyé tant que l'URL + la clé ne sont pas renseignées dans le GTG.json (évite tout spam).
		if (m_Config.data.url == "" || m_Config.data.apiKey == "")
		{
			Print("[GTGPOS] Non configuré : renseigne 'url' et 'apiKey' dans $profile:Fleet/GTG.json — envoi désactivé.", LogLevel.WARNING);
			return;
		}

		BaseWorld bw = GetGame().GetWorld();
		if (bw)
			bw.GetBoundBox(m_vBoundMin, m_vBoundMax);

		// Endpoints : explicites si fournis, sinon déduits de 'url' via DeriveUrl (robuste :
		// IndexOf+Substring — le Replace EN PLACE s'était déjà avéré peu fiable en jeu).
		m_MarkersUrl = m_Config.data.markersUrl;
		if (m_MarkersUrl == "")
			m_MarkersUrl = DeriveUrl("markers");
		m_DrawingsUrl = m_Config.data.drawingsUrl;
		if (m_DrawingsUrl == "")
			m_DrawingsUrl = DeriveUrl("drawings");
		m_CommandsUrl = m_Config.data.commandsUrl;
		if (m_CommandsUrl == "")
			m_CommandsUrl = DeriveUrl("commands");
		m_SquadsUrl = m_Config.data.squadsUrl;
		if (m_SquadsUrl == "")
			m_SquadsUrl = DeriveUrl("squads");
		m_RanksUrl = m_Config.data.ranksUrl;
		if (m_RanksUrl == "")
			m_RanksUrl = DeriveUrl("ranks");
		m_CoverageUrl = m_Config.data.coverageUrl;
		if (m_CoverageUrl == "")
			m_CoverageUrl = DeriveUrl("coverage");
		m_LoadoutsUrl = m_Config.data.loadoutsUrl;
		if (m_LoadoutsUrl == "")
			m_LoadoutsUrl = DeriveUrl("loadouts");
		m_ObjectivesUrl = m_Config.data.objectivesUrl;
		if (m_ObjectivesUrl == "")
			m_ObjectivesUrl = DeriveUrl("objectives");
		m_LeaveUrl = m_Config.data.leaveUrl;
		if (m_LeaveUrl == "")
			m_LeaveUrl = DeriveUrl("leave");

		// Journal admin : POST /api/v1/events/log (connexions, deconnexions...).
		m_EventsUrl = DeriveUrl("events/log");

		int itv = m_Config.data.intervalSec;
		if (itv < 1)
			itv = 1;

		m_bStarted = true;
		GetGame().GetCallqueue().CallLater(Tick, itv * 1000, true);
		Print("[GTGPOS] Démarré -> " + m_Config.data.url + " toutes les " + itv + "s", LogLevel.NORMAL);
		// URLs résolues (diagnostic) : on doit voir /positions /markers /drawings /commands /squads /ranks
		Print("[GTGPOS] URLs: markers=" + m_MarkersUrl + " drawings=" + m_DrawingsUrl + " commands=" + m_CommandsUrl + " squads=" + m_SquadsUrl + " ranks=" + m_RanksUrl, LogLevel.NORMAL);
	}

	void StopServer()
	{
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);
		m_bStarted = false;
	}

	// Déduit une URL d'endpoint depuis 'url' (.../positions -> .../<suffix>).
	// IndexOf+Substring = fiable (le Replace en place échouait en jeu -> tout partait sur /positions).
	protected string DeriveUrl(string suffix)
	{
		string u = m_Config.data.url;
		int idx = u.IndexOf("positions");
		if (idx < 0)
			return "";	// url non standard : renseigne l'URL explicite dans GTG.json
		string base = u.Substring(0, idx);
		return base + suffix;
	}

	// Meilleur statut de couverture radio du perso (0 NONE / 1 RECEIVE / 2 SEND / 3 BOTH_WAYS).
	// Prend en compte les relais/HQ via le SCR_RadioCoverageSystem (calcul déjà fait par le jeu).
	protected int Flt_RadioStatus(IEntity ent)
	{
		SCR_GadgetManagerComponent gm = SCR_GadgetManagerComponent.GetGadgetManager(ent);
		if (!gm)
			return 0;

		int best = 0;
		best = Flt_MaxCoverage(gm.GetGadgetsByType(EGadgetType.RADIO), best);
		best = Flt_MaxCoverage(gm.GetGadgetsByType(EGadgetType.RADIO_BACKPACK), best);
		return best;
	}

	protected int Flt_MaxCoverage(array<SCR_GadgetComponent> radios, int best)
	{
		if (!radios)
			return best;
		foreach (SCR_GadgetComponent g : radios)
		{
			if (!g)
				continue;
			IEntity re = g.GetOwner();
			if (!re)
				continue;
			SCR_CoverageRadioComponent cov = SCR_CoverageRadioComponent.Cast(re.FindComponent(SCR_CoverageRadioComponent));
			if (!cov)
				continue;
			int st = cov.GetCoverageByEncryption(cov.GetEncryptionKey());
			if (st > best)
				best = st;
		}
		return best;
	}

	// Coordonnées relatives normalisées [0,1] (comme le mod GTG).
	protected vector RelCoords(vector p)
	{
		vector size = m_vBoundMax - m_vBoundMin;
		float rx, ry, rz;
		if (size[0] != 0) rx = (p[0] - m_vBoundMin[0]) / size[0];
		if (size[1] != 0) ry = (p[1] - m_vBoundMin[1]) / size[1];
		if (size[2] != 0) rz = (p[2] - m_vBoundMin[2]) / size[2];
		return Vector(rx, ry, rz);
	}

	protected string JsonEsc(string s)
	{
		string r = s;
		r.Replace("\\", "\\\\");
		r.Replace("\"", "\\\"");
		r.Replace("\n", " ");
		r.Replace("\r", " ");
		return r;
	}

	// Marqueurs Anarchy (SM_MapMarkerStore) -> GTG /api/v1/markers.
	protected void SendMarkers()
	{
		if (m_MarkersUrl == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		array<SM_MapMarkerData> all = {};
		SM_MapMarkerStore.GetInstance().GetAll(all);

		BackendApi ba = GetGame().GetBackendApi();
		int now = System.GetUnixTime();
		string arr = "[";
		int n = 0;
		foreach (SM_MapMarkerData m : all)
		{
			if (!m)
				continue;

			// Date de pose : heure RÉELLE (Unix) de la 1re fois qu'on voit ce marqueur.
			int created;
			if (!m_mMarkerSeen.Find(m.m_iId, created))
			{
				created = now;
				m_mMarkerSeen.Set(m.m_iId, now);
			}

			if (n > 0)
				arr += ",";
			// string.Format plafonne à %9 -> on construit en fragments puis on assemble.
			// icon/ident/dim/sym = apparence exacte (icône + APP-6). author = nom du poseur
			// (m_sLastEditor, stocké par Anarchy car le playerId n'est pas stable). created = date.
			string mkA = string.Format("\"id\":%1,\"x\":%2,\"z\":%3,\"kind\":%4,\"icon\":%5,\"ident\":%6,\"dim\":%7,\"sym\":%8",
				m.m_iId, m.m_iPosX, m.m_iPosY, m.m_iKind, m.m_iIconEntry, m.m_iIdentity, m.m_iDimension, m.m_iSymbolFlags);
			string mkB = string.Format("\"color\":%1,\"vis\":%2,\"rot\":%3,\"size\":%4,\"text\":\"%5\"",
				m.m_iColor, m.m_iVisibility, m.m_iRotation, m.m_iSize, JsonEsc(m.m_sText));
			// ownerUid : web-owner si posé depuis le site, sinon playerId -> UID (posé en jeu)
			string ownerUid = Flt_MarkerBridge.Flt_GetWebOwner(m.m_iId);
			if (ownerUid == "" && ba && m.m_iOwnerId > 0)
				ownerUid = ba.GetPlayerIdentityId(m.m_iOwnerId);

			string mkC = string.Format("\"author\":\"%1\",\"created\":%2,\"ownerUid\":\"%3\"",
				JsonEsc(m.m_sLastEditor), created, JsonEsc(ownerUid));
			arr += "{" + mkA + "," + mkB + "," + mkC + "}";
			n++;
		}
		arr += "]";
		string body = "{\"markers\":" + arr + "}";

		RestContext rc = api.GetContext(m_MarkersUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbMarkers, "", body);
	}

	// Dessins (crayon) Anarchy (SM_MapDrawingStore) -> GTG /api/v1/drawings.
	// pts = paires x,z en mètres monde (comme les marqueurs).
	protected void SendDrawings()
	{
		if (m_DrawingsUrl == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		array<SM_MapDrawingData> draws = {};
		SM_MapDrawingStore.GetInstance().GetAll(draws);

		string arr = "[";
		int n = 0;
		foreach (SM_MapDrawingData d : draws)
		{
			if (!d)
				continue;

			// tableau de points "à plat" : [x0,z0,x1,z1,...]
			string pts = "[";
			int pc = 0;
			foreach (int v : d.m_aPoints)
			{
				if (pc > 0)
					pts += ",";
				pts += v.ToString();
				pc++;
			}
			pts += "]";

			if (n > 0)
				arr += ",";
			string dj = string.Format("\"id\":%1,\"owner\":%2,\"color\":%3,\"w\":%4,\"vis\":%5,\"ch\":%6,\"fill\":%7,\"author\":\"%8\"",
				d.m_iId, d.m_iOwnerId, d.m_iColor, d.m_iWidthIdx, d.m_iVisibility, d.m_iChannel, d.m_iFill, JsonEsc(d.m_sOwnerName));
			arr += "{" + dj + ",\"pts\":" + pts + "}";
			n++;
		}
		arr += "]";
		string body = "{\"drawings\":" + arr + "}";

		RestContext rc = api.GetContext(m_DrawingsUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbDrawings, "", body);
	}

	// Ordres web -> jeu : GET GTG /commands (Bearer). GTG renvoie {commands:[...]} et VIDE
	// sa file. Le callback applique via Flt_MarkerBridge.ApplyCommandsJson (logique testée).
	protected void PullCommands()
	{
		if (m_CommandsUrl == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;
		RestContext rc = api.GetContext(m_CommandsUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.GET(m_PullCb, "");
	}

	// Escouades (groupes joueurs) -> GTG /api/v1/squads.
	// Membres identifiés par UID (même clé que les positions) -> le dashboard peut
	// imbriquer les joueurs sous leur escouade. On ne pousse que les groupes ayant >=1 joueur.
	protected void SendSquads()
	{
		if (m_SquadsUrl == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm)
			return;

		BackendApi ba = GetGame().GetBackendApi();
		PlayerManager pm = GetGame().GetPlayerManager();

		array<SCR_AIGroup> groups = {};
		gm.GetAllPlayableGroups(groups);

		string arr = "[";
		int n = 0;
		foreach (SCR_AIGroup g : groups)
		{
			if (!g || g.GetPlayerCount() < 1)
				continue;

			string faction = "";
			Faction f = g.GetFaction();
			if (f)
				faction = f.GetFactionKey();

			// Nom COMPLET traduit ("Atlas White 1"), pas juste le n° d'escouade.
			string company, platoon, squad, character, format;
			g.GetCallsigns(company, platoon, squad, character, format);
			string name = SCR_GroupHelperUI.GetTranslatedGroupName(g);
			if (name == "")
				name = squad;	// repli extrême

			int leaderId = g.GetLeaderID();
			string leaderUid = "";
			string leaderName = "";
			if (leaderId > 0)
			{
				if (ba)
					leaderUid = ba.GetPlayerIdentityId(leaderId);
				if (pm)
					leaderName = pm.GetPlayerName(leaderId);
			}

			array<int> pids = g.GetPlayerIDs();
			string mem = "[";
			int mc = 0;
			foreach (int pid : pids)
			{
				string u = "";
				if (ba)
					u = ba.GetPlayerIdentityId(pid);
				if (u == "")
					u = pid.ToString();	// repli local
				if (mc > 0)
					mem += ",";
				mem += "\"" + JsonEsc(u) + "\"";
				mc++;
			}
			mem += "]";

			int squadColor = Flt_SquadColors.Resolve(g, name);	// forcée > table nom > palette auto
			string role = ResolveRankName(g.GetGroupRoleName());	// spécialité traduite (Assaut, Reco...)
			string desc = g.GetCustomDescription();				// description libre du groupe

			if (n > 0)
				arr += ",";
			// string.Format plafonne à %9 -> 2 fragments.
			string sjA = string.Format("\"id\":%1,\"faction\":\"%2\",\"name\":\"%3\",\"callsign\":\"%4\",\"leader\":\"%5\",\"leaderName\":\"%6\"",
				g.GetGroupID(), JsonEsc(faction), JsonEsc(name), JsonEsc(squad), JsonEsc(leaderUid), JsonEsc(leaderName));
			string sjB = string.Format("\"count\":%1,\"color\":%2,\"role\":\"%3\",\"desc\":\"%4\"",
				g.GetPlayerCount(), squadColor, JsonEsc(role), JsonEsc(desc));
			arr += "{" + sjA + "," + sjB + ",\"members\":" + mem + "}";
			n++;
		}
		arr += "]";
		string body = "{\"squads\":" + arr + "}";

		RestContext rc = api.GetContext(m_SquadsUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbSquads, "", body);
	}

	// Résout un nom de grade : traduit la clé de loc, sinon renvoie le brut (sans le '#').
	protected string ResolveRankName(string raw)
	{
		if (raw == "")
			return "";
		string t = WidgetManager.Translate(raw);
		if (t != "" && !t.StartsWith("#"))
			return t;
		if (raw.StartsWith("#"))
			return raw.Substring(1, raw.Length() - 1);
		return raw;
	}

	// Appelé par le callback quand GTG a bien reçu la liste des grades -> on arrête d'envoyer.
	void OnRanksSent()
	{
		m_bRanksSent = true;
		Print("[GTGPOS] Grades reçus par le site (envoi arrêté)", LogLevel.NORMAL);
	}

	// Liste des grades DISPO par faction -> GTG /api/v1/ranks. Statique par scénario :
	// construit une fois (cache), envoyé UNE fois par reboot (re-essai jusqu'à réception).
	protected void SendRanks()
	{
		if (m_RanksUrl == "" || m_bRanksSent)
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		// Construction unique.
		if (m_RanksBody == "")
		{
			SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			if (!fm)
				return;	// pas prêt : retry au prochain tick

			array<Faction> factions = {};
			fm.GetFactionsList(factions);
			if (factions.IsEmpty())
				return;

			// Format attendu par GTG : { "ranks": { "US":[{"i":0,"name":"..."}], "USSR":[...] } }
			string obj = "{";
			int fcount = 0;
			int total = 0;
			foreach (Faction f : factions)
			{
				SCR_Faction sf = SCR_Faction.Cast(f);
				if (!sf)
					continue;
				SCR_RankContainer rcont = sf.GetRanks();
				if (!rcont)
					continue;	// faction sans grades configurés

				array<ref SCR_RankInfo> ranks = rcont.GetAllRanks();
				string rarr = "[";
				int rc2 = 0;
				foreach (SCR_RankInfo ri : ranks)
				{
					if (!ri)
						continue;
					string nm = ResolveRankName(ri.GetRankName());
					if (rc2 > 0)
						rarr += ",";
					rarr += string.Format("{\"i\":%1,\"name\":\"%2\"}", (int)ri.GetRankID(), JsonEsc(nm));
					rc2++;
					total++;
				}
				rarr += "]";
				if (rc2 == 0)
					continue;

				if (fcount > 0)
					obj += ",";
				obj += "\"" + JsonEsc(sf.GetFactionKey()) + "\":" + rarr;
				fcount++;
			}
			obj += "}";
			if (fcount == 0)
				return;	// rien de valide : on réessaiera

			// On joint la TABLE DES PALIERS d'XP. Le site l'affichait jusqu'ici en dur,
			// donc toute modification des seuils en jeu le rendait faux sans prevenir.
			// Desormais la source de verite est unique : Flt_XpProgression.GradeFloor().
			m_RanksBody = "{\"ranks\":" + obj + ",\"ladder\":" + Flt_BuildXpLadderJson() + "}";
			Print(string.Format("[GTGPOS] Grades publiés : %1 factions, %2 grades", fcount, total), LogLevel.NORMAL);
		}

		RestContext rc = api.GetContext(m_RanksUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_RanksCb, "", m_RanksBody);	// callback dédié -> stop au 1er succès
	}

	// Cercle de portée radio par faction : centre = HQ, rayon = SOMME des portées radio des
	// bases de la faction (HQ + relais). -> grandit à chaque relais/base radio construit.
	protected void SendCoverage()
	{
		if (m_CoverageUrl == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		SCR_MilitaryBaseSystem bs = SCR_MilitaryBaseSystem.GetInstance();
		if (!bs)
			return;	// pas un mode campagne / pas prêt

		array<SCR_MilitaryBaseComponent> bases = {};
		bs.GetBases(bases);

		map<string, vector> hqPos = new map<string, vector>();	// faction -> position HQ
		map<string, float> sumRange = new map<string, float>();	// faction -> somme des portées
		map<string, int> facColor = new map<string, int>();		// faction -> couleur ARGB (pour le web même hors-ligne)

		foreach (SCR_MilitaryBaseComponent b : bases)
		{
			SCR_CampaignMilitaryBaseComponent cb = SCR_CampaignMilitaryBaseComponent.Cast(b);
			if (!cb)
				continue;
			Faction f = cb.GetFaction();
			if (!f)
				continue;
			string fk = f.GetFactionKey();
			facColor.Set(fk, f.GetFactionColor().PackToInt());	// couleur ARGB de la faction

			// une base ne compte que si elle a une radio de couverture active
			SCR_CoverageRadioComponent radio = SCR_CoverageRadioComponent.Cast(cb.GetOwner().FindComponent(SCR_CoverageRadioComponent));
			if (!radio)
				continue;
			BaseTransceiver tr = radio.GetTransceiver(0);
			if (!tr)
				continue;
			float range = tr.GetRange();
			if (range <= 0)
				continue;

			float cur = 0;
			sumRange.Find(fk, cur);
			sumRange.Set(fk, cur + range);

			if (cb.IsHQ())
				hqPos.Set(fk, cb.GetOwner().GetOrigin());
		}

		// un cercle par faction qui a un HQ
		string arr = "[";
		int n = 0;
		foreach (string fk, vector pos : hqPos)
		{
			float rad = 0;
			sumRange.Find(fk, rad);
			if (rad <= 0)
				continue;
			if (n > 0)
				arr += ",";
			int col = 0;
			facColor.Find(fk, col);
			arr += string.Format("{\"faction\":\"%1\",\"x\":%2,\"z\":%3,\"radius\":%4,\"color\":%5}",
				JsonEsc(fk), (int)pos[0], (int)pos[2], (int)rad, col);
			n++;
		}
		arr += "]";
		string body = "{\"coverage\":" + arr + "}";

		// Optimisation : les HQ/relais ne bougent quasi jamais -> on n'envoie QUE si ça a changé
		// (capture de base, relais construit). Évite le spam à chaque tick.
		if (body == m_sLastCoverageBody)
			return;
		m_sLastCoverageBody = body;

		RestContext rc = api.GetContext(m_CoverageUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbCoverage, "", body);
	}

	// Objectifs actifs (tâches Fleet) -> POST /api/v1/objectives {"objectives":[{id,faction,name,desc,x,z}]}.
	protected void SendObjectives()
	{
		if (m_ObjectivesUrl == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;
		string inner = Flt_Objectives.BuildActiveJson();	// contenu entre crochets (peut être "")
		string body = "{\"objectives\":[" + inner + "]}";
		RestContext rc = api.GetContext(m_ObjectivesUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbObjectives, "", body);
	}

	// Liaison compte web <-> UID en jeu : envoie {code, uid, nom} au site (POST /api/v1/link).
	// Appelé par la commande chat #link <code>. Le site matche le code -> lie le compte à l'UID.
	void Flt_PostLink(string code, string uid, string name)
	{
		if (!m_bStarted || !m_Config)
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;
		string url = DeriveUrl("link");
		if (url == "")
			return;
		string body = string.Format("{\"code\":\"%1\",\"uid\":\"%2\",\"name\":\"%3\"}",
			JsonEsc(code), JsonEsc(uid), JsonEsc(name));
		RestContext rc = api.GetContext(url);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbLink, "", body);
		Print(string.Format("[GTGPOS] Lien envoyé : code=%1 uid=%2", code, uid), LogLevel.NORMAL);
	}

	// Journal admin : POST /api/v1/events/log.
	// Un evenement notable du serveur (connexion, depart, chat...). Le site l'empile dans
	// server_events et la page /journal les affiche melanges aux combats.
	// Envoi unitaire : ces evenements sont rares (quelques-uns par minute au pire), pas la
	// peine de mettre en place un lot comme pour les positions.
	void Flt_PostEvent(string type, string uid, string playerName, string platform, string faction, string message)
	{
		if (!m_bStarted || !m_Config || m_EventsUrl == "" || type == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		string body = string.Format(
			"{\"events\":[{\"type\":\"%1\",\"playerUid\":\"%2\",\"playerName\":\"%3\",\"platform\":\"%4\",\"faction\":\"%5\",\"message\":\"%6\"}]}",
			JsonEsc(type), JsonEsc(uid), JsonEsc(playerName), JsonEsc(platform), JsonEsc(faction), JsonEsc(message));

		RestContext rc = api.GetContext(m_EventsUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbEvents, "", body);
	}

	// Départ VOLONTAIRE d'un joueur : POST /api/v1/leave {uid} -> le site le retire tout de suite
	// (sinon on garde le comportement 3 min pour les crashs/pertes de connexion).
	void Flt_PostLeave(string uid)
	{
		if (!m_bStarted || !m_Config || m_LeaveUrl == "" || uid == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;
		string body = string.Format("{\"uid\":\"%1\"}", JsonEsc(uid));
		RestContext rc = api.GetContext(m_LeaveUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbLeave, "", body);
		Print(string.Format("[GTGPOS] Départ volontaire envoyé : uid=%1", uid), LogLevel.NORMAL);
	}

	// ---- Loadouts nommés (admin crée en jeu -> site ; joueur charge via caisse/commande) ----

	// Publie un loadout capturé sous un nom (POST /api/v1/loadouts). data = JSON de CaptureJson.
	void Flt_PublishLoadout(string name, string faction, string data)
	{
		if (!m_bStarted || !m_Config || m_LoadoutsUrl == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;
		string body = string.Format("{\"name\":\"%1\",\"faction\":\"%2\",\"data\":\"%3\"}",
			JsonEsc(name), JsonEsc(faction), JsonEsc(data));
		RestContext rc = api.GetContext(m_LoadoutsUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_CbLoadoutPost, "", body);
		Print(string.Format("[LOADOUT] Publié '%1' (%2, %3 car.)", name, faction, data.Length()), LogLevel.NORMAL);
	}

	// Récupère la liste des loadouts (GET /api/v1/loadouts). wantName="" -> liste seulement ;
	// sinon -> applique ce loadout au joueur playerId à réception.
	void Flt_FetchLoadouts(int playerId, string wantName)
	{
		if (!m_bStarted || !m_Config || m_LoadoutsUrl == "")
			return;
		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;
		RestContext rc = api.GetContext(m_LoadoutsUrl);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		Flt_GTG_LoadoutGetCallback cb = new Flt_GTG_LoadoutGetCallback(playerId, wantName);
		m_aLoadoutGetCb.Insert(cb);	// garde une réf le temps de l'async
		rc.GET(cb, "");
	}

	// Libère le callback GET une fois terminé (Success/Error/Timeout).
	void Flt_ReleaseLoadoutCb(Flt_GTG_LoadoutGetCallback cb)
	{
		int i = m_aLoadoutGetCb.Find(cb);
		if (i >= 0)
			m_aLoadoutGetCb.Remove(i);
	}

	protected void Tick()
	{
		if (!Replication.IsServer())
			return;

		SendMarkers();		// marqueurs Anarchy -> GTG (indépendant des joueurs)
		SendDrawings();		// dessins (crayon) -> GTG
		SendSquads();		// escouades (groupes joueurs) -> GTG
		SendRanks();		// liste des grades dispo par faction -> GTG (statique)
		SendCoverage();		// cercle(s) de portée radio (HQ + relais) -> GTG
		SendObjectives();	// objectifs actifs (tâches Fleet) -> GTG
		PullCommands();		// ordres web -> jeu (place/move/edit/remove/setrank)

		// Rafraîchit le cache serveur des loadouts (pour l'arsenal) : au 1er tick puis ~toutes les 12x.
		m_iLoadoutFetchCtr++;
		if (m_LoadoutsUrl != "" && (m_iLoadoutFetchCtr == 0 || m_iLoadoutFetchCtr % 12 == 0))
			Flt_FetchLoadouts(0, "*cache*");

		PlayerManager pm = GetGame().GetPlayerManager();
		BackendApi ba = GetGame().GetBackendApi();
		if (!pm || !ba)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);
		if (ids.IsEmpty())
			return;

		int ts = System.GetUnixTime();
		string vfmt = "{\"x\":%1,\"y\":%2,\"z\":%3}";

		// Outer map { guid : "<inner json string>" }
		SCR_JsonSaveContext outer = new SCR_JsonSaveContext();
		int written = 0;

		foreach (int pid : ids)
		{
			IEntity ent = pm.GetPlayerControlledEntity(pid);
			if (!ent)
				continue;
			SCR_ChimeraCharacter chr = SCR_ChimeraCharacter.Cast(ent);
			if (!chr)
				continue;

			Faction fac = chr.GetFaction();
			if (!fac)
				continue;

			string guid = ba.GetPlayerIdentityId(pid);
			if (guid == "")
				guid = pid.ToString();	// fallback local
			string pname = pm.GetPlayerName(pid);
			string facName = fac.GetFactionName();
			string facKey = fac.GetFactionKey();	// clé (US/USSR) -> join avec le catalogue grades
			Color col = fac.GetFactionColor();

			// --- Couverture radio (RP / brouillard de guerre) ---
			int radio = Flt_RadioStatus(ent);	// 0 NONE / 1 RECEIVE / 2 SEND / 3 BOTH_WAYS
			// "En radio" = liaison BIDIRECTIONNELLE (radio == 3). En dessous (0/1/2) -> figé.
			bool canSend = (radio >= 3);		// BOTH_WAYS uniquement -> le HQ reçoit la position LIVE
			vector pos;
			int lastSeen;
			if (canSend)
			{
				pos = ent.GetOrigin();			// live
				m_mLastPos.Set(guid, pos);
				m_mLastSeen.Set(guid, ts);
				lastSeen = ts;
			}
			else
			{
				// hors portée : on FIGE à la dernière position connue avec radio
				if (!m_mLastPos.Find(guid, pos))
					pos = ent.GetOrigin();		// jamais vu couvert -> position actuelle
				if (!m_mLastSeen.Find(guid, lastSeen))
					lastSeen = ts;
			}
			vector rot = ent.GetYawPitchRoll();
			vector rel = RelCoords(pos);

			int inVeh = 0;
			if (chr.IsInVehicle())
				inVeh = 1;

			// Rang détecté en jeu + XP courant (pour l'affichage site + vue "à engueuler").
			int rank = (int)SCR_CharacterRankComponent.GetCharacterRank(ent);

			// L'XP vient de NOTRE table (Flt_XpProgression), pas de SCR_PlayerXPHandlerComponent.
			//
			// Piège : GetPlayerXP() lit l'XP du JEU DE BASE, alimentée par SCR_XPHandlerComponent
			// — que le mode de jeu FF ne possède pas. Elle vaut donc TOUJOURS 0, et le site
			// affichait fidèlement ce 0 alors que le joueur montait bel et bien en grade
			// (l'auto-promotion, elle, se base sur Flt_XpProgression). Deux compteurs d'XP
			// coexistent : seul le nôtre est alimenté.
			int xp = Flt_XpProgression.GetXp(guid);

			string absC = string.Format(vfmt, pos[0], pos[1], pos[2]);
			string relC = string.Format(vfmt, rel[0], rel[1], rel[2]);
			string rotC = string.Format(vfmt, rot[0], rot[1], rot[2]);
			string posEntry = string.Format(
				"{\"timestamp\":%1,\"position\":{\"absolute\":%2,\"relative\":%3},\"rotation\":%4,\"inVehicle\":%5}",
				ts, absC, relC, rotC, inVeh);

			// string.Format plafonne à %9 -> on scinde en 2 fragments puis on assemble.
			string innerA = string.Format(
				"\"playerGuid\":\"%1\",\"playerName\":\"%2\",\"factionName\":\"%3\",\"factionKey\":\"%4\",\"factionColor\":{\"r\":%5,\"g\":%6,\"b\":%7}",
				guid, pname, facName, facKey, col.R(), col.G(), col.B());
			string innerB = string.Format(
				"\"rank\":%1,\"xp\":%2,\"radio\":%3,\"lastSeen\":%4,\"positions\":[%5]",
				rank, xp, radio, lastSeen, posEntry);
			string inner = "{" + innerA + "," + innerB + "}";

			outer.WriteValue(guid, inner);	// valeur = string -> échappée (map[string]string côté Go)
			written++;
		}

		if (written == 0)
			return;

		string body = outer.ExportToString();
		if (body == "" || body == "{}")
			return;

		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;
		RestContext rc = api.GetContext(m_Config.data.url);
		if (!rc)
			return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_Config.data.apiKey));
		rc.POST(m_Cb, "", body);
	}
}

// Boot serveur (chaîne avec les autres modded SCR_BaseGameMode de Fleet).
modded class SCR_BaseGameMode
{
	override void OnGameModeStart()
	{
		super.OnGameModeStart();
		if (Replication.IsServer())
			GetGame().GetCallqueue().CallLater(Flt_GTG_Boot, 2500, false);
	}

	protected void Flt_GTG_Boot()
	{
		Flt_GTGPositions.GetInstance().StartServer();
	}

	override void OnGameEnd()
	{
		Flt_GTGPositions.GetInstance().StopServer();
		super.OnGameEnd();
	}
}
