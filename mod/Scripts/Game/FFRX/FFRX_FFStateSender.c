// FF + Reoccupation state emitter to the admin livemap (endpoint /ffstate).
// Server only. Reuses Fleet config ($profile:Fleet/GTG.json : url + apiKey),
// derives positions -> ffstate URL, POSTs the snapshot every 5s.
// Boot is called from FFRX_Groups.c (modded SCR_BaseGameMode.OnGameModeStart). See TASK_FFSTATE_SENDER.
// NOTE: ASCII only (Enforce compiler desyncs on UTF-8 accents in comments/strings).

// ---- REST callback (SetOnSuccess/SetOnError, no override -> avoids native spam) ----
class FFRX_FFRestCb : RestCallback
{
	string m_sTag = "FFState";   // which endpoint this callback serves (set by the sender) -- the
	                             // callback is shared by /ffstate AND /ffcatalog, so label the logs.
	int m_iLastBodyBytes;        // size of the last POSTed body (set by the sender before POST)
	protected int m_iOk;
	protected int m_iErr;

	void FFRX_FFRestCb()
	{
		SetOnSuccess(FFRX_OnOk);
		SetOnError(FFRX_OnErr);
	}

	protected void FFRX_OnOk(RestCallback cb)
	{
		m_iOk = m_iOk + 1;
		// First success after failures -> note recovery once.
		if (m_iErr > 0)
		{
			Print(string.Format("[FFRX][%1] POST OK again after %2 errors.", m_sTag, m_iErr), LogLevel.NORMAL);
			m_iErr = 0;
		}
	}

	protected void FFRX_OnErr(RestCallback cb)
	{
		m_iErr = m_iErr + 1;
		// First error: log the REAL reason (HTTP code + rest result + server response body) so we
		// stop guessing WHY /ffstate rejects us (400 schema? 401 auth? 413 too big?).
		if (m_iErr == 1)
		{
			int http = 0;
			int rest = 0;
			string resp = "";
			if (cb)
			{
				http = (int)cb.GetHttpCode();
				rest = (int)cb.GetRestResult();
				resp = cb.GetData();
			}
			if (resp.Length() > 400)
				resp = resp.Substring(0, 400);
			Print(string.Format("[FFRX][%1] POST rejected: HTTP=%2 restResult=%3 body=%4B resp=<%5>", m_sTag, http, rest, m_iLastBodyBytes, resp), LogLevel.WARNING);
		}
		else if (m_iErr % 12 == 0)
			Print(string.Format("[FFRX][%1] POST rejected (x%2).", m_sTag, m_iErr), LogLevel.WARNING);
	}
}

// ---- Emitter ----
class FFRX_FFStateSender
{
	protected static ref FFRX_FFStateSender s_Instance;

	protected string m_sUrl;
	protected string m_sApiKey;
	protected ref FFRX_FFRestCb m_Cb;
	protected bool m_bReady;
	protected bool m_bLoggedBody;

	// Accumulators used by the world-query callbacks (vehicles + characters).
	protected string m_sVehAccum;

	// Territoires : dernier etat connu par noeud ("ROLE|border"), pour n'envoyer que ce
	// qui change. Voir FFRX_TerritoryZonesJson.
	protected ref map<string, string> m_mTerrState;
	// Compte a rebours avant le prochain envoi COMPLET (avec les polygones). A 30 s par
	// envoi, 20 = une resynchronisation toutes les 10 minutes.
	protected int m_iTerrTick;
	protected static const int TERR_FULL_EVERY = 20;
	protected int m_iVehCount;
	protected string m_sUnitAccum;
	protected string m_sPlayerAccum;
	protected int m_iUnitCount;
	// Killfeed: ring buffer of recent kill JSON objects (last 40), sent inside /ffstate.
	protected ref array<string> m_aKills = {};
	protected bool m_bKillsSubbed;

	static void Boot()
	{
		if (!Replication.IsServer()) return;
		if (s_Instance) return;
		s_Instance = new FFRX_FFStateSender();
		s_Instance.Start();
	}

	void Start()
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile("$profile:Fleet/GTG.json"))
		{
			Print("[FFRX][FFState] $profile:Fleet/GTG.json missing -> emitter disabled", LogLevel.WARNING);
			return;
		}

		string posUrl = "";
		string explicitUrl = "";
		ctx.ReadValue("url", posUrl);
		ctx.ReadValue("apiKey", m_sApiKey);
		ctx.ReadValue("ffstateUrl", explicitUrl);

		if (explicitUrl != "")
			m_sUrl = explicitUrl;
		else
			m_sUrl = DeriveUrl(posUrl, "ffstate");

		if (m_sUrl == "" || m_sApiKey == "")
		{
			Print("[FFRX][FFState] url/apiKey missing in GTG.json -> emitter disabled", LogLevel.WARNING);
			return;
		}

		m_Cb = new FFRX_FFRestCb();
		m_bReady = true;
		GetGame().GetCallqueue().CallLater(Tick, 5000, true);
		// Killfeed hooks (FF game mode + AI manager may not exist yet -> retry until ready).
		GetGame().GetCallqueue().CallLater(FFRX_SubscribeKills, 3000, true);
		Print("[FFRX][FFState] emitter started -> " + m_sUrl, LogLevel.NORMAL);
	}

	// positions -> ffstate (same logic as Fleet DeriveUrl)
	protected string DeriveUrl(string base, string suffix)
	{
		int idx = base.IndexOf("positions");
		if (idx < 0) return "";
		return base.Substring(0, idx) + suffix;
	}

	protected void Tick()
	{
		if (!m_bReady) return;
		if (!JWK.GetFactions()) return; // FF faction system not ready yet -> skip this tick
		RestApi api = GetGame().GetRestApi();
		if (!api) return;

		string body = BuildJson();

		// One-time diagnostic (keeps the console fluid): body size + a short head, so we can
		// tell if /ffstate is rejecting an oversized payload or a schema it dislikes.
		if (!m_bLoggedBody)
		{
			m_bLoggedBody = true;
			Print(string.Format("[FFRX][FFState] emitter live -> %1 | body=%2 bytes", m_sUrl, body.Length()), LogLevel.NORMAL);
		}

		RestContext rc = api.GetContext(m_sUrl);
		if (!rc) return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_sApiKey));
		m_Cb.m_iLastBodyBytes = body.Length();
		rc.POST(m_Cb, "", body);
	}

	// -----------------------------------------------------------------------------------

	protected string BuildJson()
	{
		int now = JWK_GameplayTimestamp.GetSystemAccumulator();
		World world = GetGame().GetWorld();

		string pois = FFRX_BuildPois(world);
		// ---- Active battle ----
		string battles = "";
		JWK_BattleManagerComponent bm = JWK.GetBattleManager();
		if (bm)
		{
			JWK_BattleControllerEntity ctrl = bm.GetController();
			if (ctrl && ctrl.IsActive_S())
			{
				JWK_BattleSubjectComponent subj = ctrl.GetSubject_S();
				if (subj && subj.GetOwner())
				{
					vector bp = subj.GetOwner().GetOrigin();
					string attKey = "";
					Faction att = ctrl.GetAttackingFaction_S();
					if (att) attKey = att.GetFactionKey();
					battles = string.Format("{\"x\":%1,\"z\":%2,\"attacker\":\"%3\",\"points\":%4,\"name\":\"%5\",\"state\":\"active\"}",
						(int)bp[0], (int)bp[2], Esc(attKey), ctrl.FFRX_GetPoints(), Esc(ctrl.GetLocationName()));
				}
			}
		}

		// ---- Counterattacks + patrols (Reoccupation) ----
		string cas = "";
		string pats = "";
		FF_ReoccupationManager rm = FF_ReoccupationManager.Get();
		if (rm)
		{
			array<ref FF_ReoccupationEntry> q = rm.FFRX_GetQueue();
			if (q)
			{
				foreach (FF_ReoccupationEntry en : q)
				{
					if (!en || !en.m_Subject || !en.m_Subject.GetOwner()) continue;
					vector tp = en.m_Subject.GetOwner().GetOrigin();

					string tname = "";
					JWK_NamedLocationComponent nl2 = JWK_CompTU<JWK_NamedLocationComponent>.FindIn(en.m_Subject.GetOwner());
					if (nl2) tname = nl2.GetName();

					string recon = "null";
					if (en.m_bReconEnabled && en.m_iReconState >= 2)
					{
						vector rp = en.m_vReconLastKnownPosition;
						recon = string.Format("{\"state\":%1,\"x\":%2,\"z\":%3,\"route\":%4}",
							en.m_iReconState, (int)rp[0], (int)rp[2], BuildRoute(en.m_aReconRoute));
					}

					if (cas != "") cas = cas + ",";
					cas = cas + string.Format("{\"targetName\":\"%1\",\"targetX\":%2,\"targetZ\":%3,\"state\":\"scheduled\",\"triggerIn\":%4,\"strength\":%5,\"recon\":%6}",
						Esc(tname), (int)tp[0], (int)tp[2], en.m_iTriggerTime - now, en.m_fReconAttackStrengthBonus, recon);
				}
			}

			FF_PatrolManager pm = rm.FFRX_GetPatrolMgr();
			if (pm)
			{
				array<ref FF_PatrolTask> tasks = pm.FFRX_GetActivePatrols();
				if (tasks)
				{
					foreach (FF_PatrolTask t : tasks)
					{
						if (!t) continue;
						vector lp = t.m_vLastKnownPosition;
						if (pats != "") pats = pats + ",";
						pats = pats + string.Format("{\"x\":%1,\"z\":%2,\"state\":\"%3\",\"route\":%4}",
							(int)lp[0], (int)lp[2], PatrolStateStr(t.m_iState), BuildRoute(t.m_Route));
					}
				}
			}
		}

		// ---- Units (enemy + civilians) + players via world character query (real entities) ----
		m_sUnitAccum = "";
		m_sPlayerAccum = "";
		m_iUnitCount = 0;
		vector uqmins = Vector(-2000, -1000, -2000);
		vector uqmaxs = Vector(20000, 3000, 20000);
		world.QueryEntitiesByAABB(uqmins, uqmaxs, FFRX_CharCallback, FFRX_CharFilter, EQueryEntitiesFlags.DYNAMIC);
		string units = m_sUnitAccum;
		string players = m_sPlayerAccum;

		// ---- Vehicles (whole-map dynamic query) ----
		m_sVehAccum = "";
		m_iVehCount = 0;
		vector qmins = Vector(-2000, -1000, -2000);
		vector qmaxs = Vector(20000, 3000, 20000);
		world.QueryEntitiesByAABB(qmins, qmaxs, FFRX_VehCallback, FFRX_VehFilter, EQueryEntitiesFlags.DYNAMIC);

		// ---- War zones (battle areas + enemy POI areas) as polygons ----
		string zones = "";
		array<GenericComponent> zc = JWK_IndexSystem.Get(world).GetAllGC(JWK_WorldZoneComponent);
		foreach (GenericComponent gc : zc)
		{
			JWK_WorldZoneComponent zone = JWK_WorldZoneComponent.Cast(gc);
			if (!zone) continue;
			bool isBattle = zone.HasTag(JWK_EWorldZoneTag.BATTLE_AREA);
			bool isPoi = zone.HasTag(JWK_EWorldZoneTag.POI_AREA);
			if (!isBattle && !isPoi) continue;

			array<vector> pts = zone.GetPoints();
			if (!pts || pts.Count() < 3) continue;

			string ptsJson = "[";
			for (int i = 0; i < pts.Count(); i++)
			{
				if (i > 0) ptsJson = ptsJson + ",";
				vector v = pts[i];
				ptsJson = ptsJson + string.Format("[%1,%2]", (int)v[0], (int)v[2]);
			}
			ptsJson = ptsJson + "]";

			string tag = "battle";
			if (isPoi && !isBattle) tag = "poi";
			if (zones != "") zones = zones + ",";
			zones = zones + string.Format("{\"tag\":\"%1\",\"pts\":%2}", tag, ptsJson);
		}

		// ---- Territoires + ligne de front ----
		// Les noeuds de controle FF portent deja tout ce qu'il faut : leur polygone
		// (GetAreaPoly), le camp qui les tient (GetFactionRole) et surtout IsBorder(),
		// qui EST la definition FF de la ligne de front. On les sort tels quels, la carte
		// admin decide de les montrer ou non.
		bool terrFull = false;
		string terr = FFRX_TerritoryZonesJson(world, terrFull);
		if (terr != "")
		{
			if (zones != "") zones = zones + ",";
			zones = zones + terr;
		}

		// ---- Zones de menace adaptative (jauges blinde / aerien par secteur) ----
		string threat = FFRX_AdaptiveThreat.Get().ZonesJson();
		if (threat != "")
		{
			if (zones != "") zones = zones + ",";
			zones = zones + threat;
		}

		// ---- Diag panel (CoreStats du Diag Menu FF : agents IA, forces, vehicules, population, GC...) ----
		string diag = "";
		JWK_CoreStatSystem cs = JWK_CoreStatSystem.Get(world);
		if (cs)
		{
			JWK_CoreStatsStringified st = cs.GetFormattedStats(true, false);
			if (st && st.m_aLabels && st.m_aValues)
			{
				for (int i = 0; i < st.m_aLabels.Count() && i < st.m_aValues.Count(); i++)
				{
					if (diag != "") diag = diag + ",";
					diag = diag + string.Format("{\"l\":\"%1\",\"v\":\"%2\"}", Esc(st.m_aLabels[i]), Esc(st.m_aValues[i]));
				}
			}
		}

		// ---- Campaign progress % (avancement global de la campagne) ----
		int campaignPct = -1;
		JWK_GameProgressManagerComponent gp = JWK_GameProgressManagerComponent.GetInstance();
		if (gp) campaignPct = (int)gp.GetTotalProgressPercentage();

		// ---- Garrisons (JWK_AIGarrisonComponent : position + effectif + rayon de patrouille) ----
		string garr = "";
		array<GenericComponent> glist = JWK_IndexSystem.Get(world).GetAllGC(JWK_AIGarrisonComponent);
		foreach (GenericComponent ggc : glist)
		{
			JWK_AIGarrisonComponent gar = JWK_AIGarrisonComponent.Cast(ggc);
			if (!gar || !gar.GetOwner()) continue;
			vector gpos = gar.GetOwner().GetOrigin();
			int gsize = FFRX_Garrison(gar.GetOwner());
			int grange = gar.FFRX_GetPatrolRange();
			if (garr != "") garr = garr + ",";
			garr = garr + string.Format("{\"x\":%1,\"z\":%2,\"size\":%3,\"range\":%4}", (int)gpos[0], (int)gpos[2], gsize, grange);
		}

		string fobs = FFRX_BuildFobs(world);

		// ---- Killfeed (recent kills, oldest first) ----
		string kills = "";
		if (m_aKills)
		{
			foreach (string k : m_aKills)
			{
				if (kills != "") kills = kills + ",";
				kills = kills + k;
			}
		}

		// ---- Assemble ----
		// head = numbers only (safe via Format). The big ARRAY parts are built with '+' NOT
		// string.Format: Format truncates its output at ~8192 bytes, and on a busy server the
		// units/vehicles/kills arrays blow past that -> truncated = invalid JSON = HTTP 400.
		// string.Format tronque au-dela de ~8 Ko : on decoupe le summary en deux appels
		// plutot que d'ajouter 10 arguments a la ligne existante.
		string head = string.Format("{\"ts\":%1,\"summary\":{\"playerPct\":%2,\"enemyPct\":%3,\"enemyManpower\":%4,\"campaignPct\":%5",
			now, playerPct, enemyPct, enemyManpower, campaignPct);
		head = head + string.Format(",\"supplies\":%1,\"suppliesMax\":%2,\"prod\":%3,\"factories\":%4,\"factoriesTotal\":%5",
			supTotal, supMaxTotal, prodTotal, facPlayer, facTotal);
		head = head + string.Format(",\"fuel\":%1,\"fuelMax\":%2,\"fuelProd\":%3,\"fuelStations\":%4,\"fuelStationsTotal\":%5",
			fuelTotal, fuelMaxTotal, fuelProdTotal, fuelStationsPlayer, fuelStationsTotal);
		// Serie "carte entiere" : le potentiel total du theatre, donc ce qui reste a
		// conquerir. Champs suffixes "All" pour ne pas casser les lecteurs existants.
		head = head + string.Format(",\"suppliesAll\":%1,\"suppliesMaxAll\":%2,\"prodAll\":%3,\"fuelAll\":%4,\"fuelMaxAll\":%5,\"fuelProdAll\":%6},",
			supAll, supMaxAll, FFRX_ProductionTracker.GetSuppliesPerHourAll(),
			fuelAll, fuelMaxAll, FFRX_ProductionTracker.GetFuelPerHourAll());
		string partA = "\"pois\":[" + pois + "],\"battles\":[" + battles + "],\"counterattacks\":[" + cas + "],\"patrols\":[" + pats + "],";
		// terrFull=1 : cet envoi porte la geometrie complete des territoires, le site doit
		// repartir de zero. terrFull=0 : ne sont presents que les noeuds qui ont change,
		// sans polygone -> le site FUSIONNE dans ce qu'il a deja.
		int terrFullFlag = 0;
		if (terrFull)
			terrFullFlag = 1;

		// ⚠️ NE PAS RECOMPACTER CECI EN UNE SEULE EXPRESSION.
		// Une chaine d'une vingtaine de concatenations d'affilee fait echouer le
		// compilateur -- "Formula too complex", suivi d'une "Incompatible parameter"
		// trompeuse sur le premier operande, et de ~13 erreurs en cascade dans des
		// fichiers du JEU DE BASE qui n'ont rien a voir.
		//
		// Et on ne peut pas revenir a string.Format : il TRONQUE sa sortie a ~8 Ko, ce qui
		// coupait ce JSON en plein milieu (cf. memoire enfusion-stringformat-8kb-truncation).
		// Entre les deux limites, la seule forme sure est l'accumulation par etapes.
		string partB = "\"units\":[" + units + "],";
		partB = partB + "\"vehicles\":[" + m_sVehAccum + "],";
		partB = partB + "\"zones\":[" + zones + "],";
		partB = partB + "\"terrFull\":" + terrFullFlag.ToString() + ",";
		partB = partB + "\"diag\":[" + diag + "],";
		partB = partB + "\"garrisons\":[" + garr + "],";
		partB = partB + "\"kills\":[" + kills + "],";
		partB = partB + "\"players\":[" + players + "],";
		partB = partB + "\"fobs\":[" + fobs + "]}";

		return head + partA + partB;
	}

	//! Territoires + ligne de front, en DIFFERENTIEL.
	//!
	//! POURQUOI PAS TOUT ENVOYER A CHAQUE FOIS. La geometrie d'un territoire ne bouge
	//! quasiment jamais -- ce sont des polygones poses a la main dans le monde -- alors
	//! que le payload part toutes les 30 s. Renvoyer tous les contours en boucle, c'est
	//! l'essentiel du volume pour zero information nouvelle. Seuls le CAMP qui tient le
	//! noeud et son statut de FRONTIERE changent, et ca tient en quelques octets.
	//!
	//! Donc : un envoi COMPLET (avec les `pts`) de temps en temps, et entre deux, seuls
	//! les noeuds dont l'etat a REELLEMENT change, sans leur geometrie. Le site conserve
	//! ce qu'il a deja et fusionne (cf. ffTerrCache dans mapadmin.html).
	//!
	//! POURQUOI PAS LES EVENTS DE FF. JWK_TerritoryControlSystem declare bien
	//! OnNodeAreaChanged et OnNodeBorderChanged... mais AUCUN code de FF ne s'y abonne :
	//! impossible de verifier qu'un ConnectEvent global les recoit vraiment, et on s'est
	//! deja fait avoir exactement comme ca avec OnAiSpawnRequestDone_S (event bien jete,
	//! jamais recu, recensement a zero pendant 30 min). On compare donc nous-memes a
	//! l'etat precedent : aucune dependance a un event non teste.
	//!
	//! Le renvoi complet periodique sert de resynchronisation : si le site redemarre, il
	//! retrouve la geometrie sans qu'on ait a le detecter.
	protected string FFRX_TerritoryZonesJson(BaseWorld world, out bool outIsFull)
	{
		outIsFull = false;

		if (!world)
			return "";

		array<GenericComponent> comps = JWK_IndexSystem.Get(world).GetAllGC(JWK_TerritoryControlNodeComponent);
		if (!comps)
			return "";

		if (!m_mTerrState)
			m_mTerrState = new map<string, string>();

		// Envoi complet au premier passage, puis toutes les TERR_FULL_EVERY fois.
		bool full = (m_iTerrTick <= 0);
		m_iTerrTick--;
		if (full)
		{
			m_iTerrTick = TERR_FULL_EVERY;
			m_mTerrState.Clear();
		}
		outIsFull = full;

		string txt = "";

		foreach (GenericComponent gc : comps)
		{
			JWK_TerritoryControlNodeComponent node = JWK_TerritoryControlNodeComponent.Cast(gc);
			if (!node)
				continue;
			if (!node.IsEnabled() || !node.HasTerritory())
				continue;

			int border = 0;
			if (node.IsBorder())
				border = 1;

			string role = RoleStr(node.GetFactionRole());

			// Identite du noeud : son centroide arrondi. Stable d'une session a l'autre,
			// contrairement a un EntityID qui est reattribue a chaque demarrage.
			vector c = node.GetCentroid();
			string id = ((int)c[0]).ToString() + "_" + ((int)c[2]).ToString();

			string state = role + "|" + border.ToString();

			if (!full)
			{
				string known;
				if (m_mTerrState.Find(id, known) && known == state)
					continue; // rien de neuf pour ce noeud
			}

			m_mTerrState.Set(id, state);

			if (txt != "")
				txt = txt + ",";

			txt = txt + "{\"tag\":\"territory\",\"id\":\"" + id + "\""
				+ ",\"role\":\"" + role + "\""
				+ ",\"border\":" + border.ToString();

			// La geometrie ne part que dans les envois complets.
			if (full)
			{
				array<vector> poly = {};
				node.GetAreaPoly(poly);
				if (poly.Count() >= 3)
				{
					txt = txt + ",\"pts\":[";
					for (int i = 0; i < poly.Count(); i++)
					{
						if (i > 0)
							txt = txt + ",";
						txt = txt + "[" + ((int)poly[i][0]).ToString() + "," + ((int)poly[i][2]).ToString() + "]";
					}
					txt = txt + "]";
				}
			}

			txt = txt + "}";
		}

		return txt;
	}

	// World-query filter: keep only characters.
	protected bool FFRX_CharFilter(IEntity ent)
	{
		return ChimeraCharacter.Cast(ent) != null;
	}

	// World-query callback: collects enemy + civilian characters, and players (separately).
	protected bool FFRX_CharCallback(IEntity ent)
	{
		ChimeraCharacter ch = ChimeraCharacter.Cast(ent);
		if (!ch) return true;

		JWK_EFactionRole role = JWK_EFactionRole.UNDEFINED;
		FactionAffiliationComponent fac = JWK_CompTU<FactionAffiliationComponent>.FindIn(ch);
		JWK_FactionManager fmc = JWK.GetFactions();
		if (fmc && fac) role = fmc.GetRole(fac);

		// Players get their own array with full GM-style details.
		if (role == JWK_EFactionRole.PLAYER)
		{
			FFRX_CollectPlayer(ch);
			return true;
		}
		if (role == JWK_EFactionRole.SUPPORTING || role == JWK_EFactionRole.UNDEFINED)
			return true;

		if (m_iUnitCount >= 600) return true; // units cap reached (keep scanning for players)

		vector p = ch.GetOrigin();
		// Enemy AI: attach GM-style details (type/weapon/health/group/gear).
		string extra = "";
		if (role == JWK_EFactionRole.ENEMY) extra = FFRX_UnitExtra(ch);
		if (m_sUnitAccum != "") m_sUnitAccum = m_sUnitAccum + ",";
		m_sUnitAccum = m_sUnitAccum + string.Format("{\"x\":%1,\"z\":%2,\"role\":\"%3\"%4}", (int)p[0], (int)p[2], RoleStr(role), extra);
		m_iUnitCount = m_iUnitCount + 1;
		return true;
	}

	// Collect a player character with UID (matches Fleet's playerGuid) + GM-style details.
	protected void FFRX_CollectPlayer(IEntity ch)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm) return;
		int pid = pm.GetPlayerIdFromControlledEntity(ch);
		if (pid <= 0) return; // not actually player-controlled (e.g. unconscious/AI takeover)

		string name = pm.GetPlayerName(pid);
		string uid = "";
		BackendApi be = GetGame().GetBackendApi();
		if (be) uid = be.GetPlayerIdentityId(pid);

		vector p = ch.GetOrigin();
		string extra = FFRX_UnitExtra(ch); // type/hp/weapon/group/gear

		// Player in a vehicle: Fleet already shows the player marker (car badge) at the
		// vehicle position -> here we only add the vehicle type for the info panel.
		SCR_ChimeraCharacter scr = SCR_ChimeraCharacter.Cast(ch);
		if (scr && scr.IsInVehicle())
		{
			string vt = FFRX_VehicleOfCharType(ch);
			extra = extra + string.Format(",\"inVehicle\":1,\"veh\":\"%1\"", Esc(vt));
		}

		if (m_sPlayerAccum != "") m_sPlayerAccum = m_sPlayerAccum + ",";
		m_sPlayerAccum = m_sPlayerAccum + string.Format("{\"uid\":\"%1\",\"name\":\"%2\",\"x\":%3,\"z\":%4%5}",
			Esc(uid), Esc(name), (int)p[0], (int)p[2], extra);
	}

	// Display name / short prefab of the vehicle a character currently occupies.
	protected string FFRX_VehicleOfCharType(IEntity ch)
	{
		CompartmentAccessComponent cac = JWK_CompTU<CompartmentAccessComponent>.FindIn(ch);
		if (!cac) return "";
		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot) return "";
		IEntity veh = slot.GetOwner();
		if (!veh) return "";
		string t = JWK_UIUtils.GetDisplayName(veh);
		if (t == "" && veh.GetPrefabData()) t = FFRX_ShortName(veh.GetPrefabData().GetPrefabName());
		return t;
	}

	// GM-style enemy detail fragment (starts with a comma; "" if unavailable).
	protected string FFRX_UnitExtra(IEntity ch)
	{
		string frag = "";

		string type = JWK_UIUtils.GetDisplayName(ch);
		if (type != "") frag = frag + string.Format(",\"type\":\"%1\"", Esc(type));

		SCR_DamageManagerComponent dmg = JWK_CompTU<SCR_DamageManagerComponent>.FindIn(ch);
		if (dmg)
		{
			int hp = (int)(dmg.GetHealthScaled() * 100);
			frag = frag + string.Format(",\"hp\":%1", hp);
		}

		BaseWeaponManagerComponent wm = JWK_CompTU<BaseWeaponManagerComponent>.FindIn(ch);
		if (wm)
		{
			BaseWeaponComponent w = wm.GetCurrentWeapon();
			if (w && w.GetOwner() && w.GetOwner().GetPrefabData())
			{
				string wn = FFRX_ShortName(w.GetOwner().GetPrefabData().GetPrefabName());
				if (wn != "") frag = frag + string.Format(",\"weapon\":\"%1\"", Esc(wn));
			}
		}

		string grp = FFRX_GroupName(ch);
		if (grp != "") frag = frag + string.Format(",\"group\":\"%1\"", Esc(grp));

		string gear = FFRX_CharGear(ch);
		frag = frag + string.Format(",\"gear\":[%1]", gear);

		return frag;
	}

	protected string FFRX_GroupName(IEntity ch)
	{
		AIControlComponent aic = JWK_CompTU<AIControlComponent>.FindIn(ch);
		if (!aic) return "";
		AIAgent agent = aic.GetControlAIAgent();
		if (!agent) return "";
		SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!grp) return "";
		SCR_EditableGroupComponent eg = JWK_CompTU<SCR_EditableGroupComponent>.FindIn(grp);
		if (!eg) return "";
		return eg.GetDisplayName();
	}

	// Worn gear = short prefab names of the main clothing slots.
	protected string FFRX_CharGear(IEntity ch)
	{
		SCR_CharacterInventoryStorageComponent inv = JWK_CompTU<SCR_CharacterInventoryStorageComponent>.FindIn(ch);
		if (!inv) return "";
		array<typename> areas = {LoadoutHeadCoverArea, LoadoutJacketArea, LoadoutVestArea, LoadoutArmoredVestSlotArea, LoadoutBackpackArea};
		string s = "";
		foreach (typename a : areas)
		{
			IEntity it = inv.GetClothFromArea(a);
			if (!it || !it.GetPrefabData()) continue;
			string nm = FFRX_ShortName(it.GetPrefabData().GetPrefabName());
			if (nm == "") continue;
			if (s != "") s = s + ",";
			s = s + string.Format("\"%1\"", Esc(nm));
		}
		return s;
	}

	// "{GUID}Prefabs/.../Rifle_k98k.et" -> "Rifle_k98k"
	protected string FFRX_ShortName(ResourceName rn)
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

	// World-query filter: keep only vehicles (runs before the add callback).
	protected bool FFRX_VehFilter(IEntity ent)
	{
		return Vehicle.Cast(ent) != null;
	}

	// World-query callback: collects dynamic vehicles (position + faction role).
	protected bool FFRX_VehCallback(IEntity ent)
	{
		if (m_iVehCount >= 300) return false;
		Vehicle v = Vehicle.Cast(ent);
		if (!v) return true;

		vector p = v.GetOrigin();
		JWK_EFactionRole role = JWK_EFactionRole.UNDEFINED;
		FactionAffiliationComponent fac = JWK_CompTU<FactionAffiliationComponent>.FindIn(v);
		if (fac)
		{
			JWK_FactionManager fmv = JWK.GetFactions();
			if (fmv) role = fmv.GetRole(fac);
		}

		string extra = FFRX_VehExtra(v);
		if (m_sVehAccum != "") m_sVehAccum = m_sVehAccum + ",";
		m_sVehAccum = m_sVehAccum + string.Format("{\"x\":%1,\"z\":%2,\"role\":\"%3\"%4}", (int)p[0], (int)p[2], RoleStr(role), extra);
		m_iVehCount = m_iVehCount + 1;
		return true;
	}

	// Vehicle detail fragment: type/health/crew/hasPlayer (starts with a comma).
	protected string FFRX_VehExtra(IEntity v)
	{
		string frag = "";

		string type = JWK_UIUtils.GetDisplayName(v);
		if (type == "" && v.GetPrefabData()) type = FFRX_ShortName(v.GetPrefabData().GetPrefabName());
		if (type != "") frag = frag + string.Format(",\"type\":\"%1\"", Esc(type));

		SCR_DamageManagerComponent dmg = JWK_CompTU<SCR_DamageManagerComponent>.FindIn(v);
		if (dmg) frag = frag + string.Format(",\"hp\":%1", (int)(dmg.GetHealthScaled() * 100));

		int crew = 0;
		int hasPlayer = 0;
		string seats = "";
		FFRX_VehCrew(v, crew, hasPlayer, seats);
		frag = frag + string.Format(",\"crew\":%1,\"hasPlayer\":%2", crew, hasPlayer);
		if (seats != "") frag = frag + string.Format(",\"seats\":\"%1\"", Esc(seats));

		// Carburant : en pourcentage plutot qu'en litres, sinon la valeur n'est pas
		// comparable d'un vehicule a l'autre. GetTotalMaxFuel() peut valoir 0 sur les
		// engins sans reservoir (remorques) -> on evite la division par zero.
		FuelManagerComponent fuel = JWK_CompTU<FuelManagerComponent>.FindIn(v);
		if (fuel)
		{
			float maxFuel = fuel.GetTotalMaxFuel();
			if (maxFuel > 0)
				frag = frag + string.Format(",\"fuel\":%1", (int)((fuel.GetTotalFuel() / maxFuel) * 100));
		}

		// Ravitaillement transporte (camions de supply). Absent sur la plupart des
		// vehicules : on n'emet la cle que si le conteneur existe.
		SCR_ResourceComponent res = JWK_CompTU<SCR_ResourceComponent>.FindIn(v);
		if (res)
		{
			SCR_ResourceContainer cont = res.GetContainer(EResourceType.SUPPLIES);
			if (cont)
				frag = frag + string.Format(",\"sup\":%1,\"supMax\":%2", (int)cont.GetResourceValue(), (int)cont.GetMaxResourceValue());
		}

		// Vitesse en km/h. Sert surtout au rendu : le snapshot part toutes les 5 s, et
		// sans vitesse la carte ne peut qu'un teleporter les vehicules d'un point a
		// l'autre. Avec elle, le front peut extrapoler le deplacement entre deux
		// envois et obtenir un mouvement fluide.
		Physics ph = v.GetPhysics();
		if (ph)
			frag = frag + string.Format(",\"spd\":%1", (int)(ph.GetVelocity().Length() * 3.6));

		return frag;
	}

	// Count occupants of a vehicle and whether a player is aboard.
	// Occupants d'un vehicule.
	//   crew      : nombre total d'occupants (joueurs + IA)
	//   hasPlayer : au moins un joueur a bord
	//   seats     : noms des joueurs a bord, separes par "|" (les IA sont ignorees --
	//               un bus de 20 IA n'apprendrait rien et gonflerait le payload)
	protected void FFRX_VehCrew(IEntity v, out int crew, out int hasPlayer, out string seats)
	{
		crew = 0;
		hasPlayer = 0;
		seats = "";
		BaseCompartmentManagerComponent cm = JWK_CompTU<BaseCompartmentManagerComponent>.FindIn(v);
		if (!cm) return;
		array<BaseCompartmentSlot> slots = {};
		cm.GetCompartments(slots);
		PlayerManager pm = GetGame().GetPlayerManager();
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot) continue;
			IEntity occ = slot.GetOccupant();
			if (!occ) continue;
			crew = crew + 1;
			if (!pm) continue;
			int pid = pm.GetPlayerIdFromControlledEntity(occ);
			if (pid <= 0) continue;
			hasPlayer = 1;
			string pn = pm.GetPlayerName(pid);
			if (pn == "") continue;
			if (seats != "") seats = seats + "|";
			seats = seats + pn;
		}
	}

	// -----------------------------------------------------------------------------------

	protected string BuildRoute(array<vector> r)
	{
		if (!r || r.IsEmpty()) return "[]";
		string o = "[";
		for (int i = 0; i < r.Count(); i++)
		{
			if (i > 0) o = o + ",";
			vector v = r[i];
			o = o + string.Format("[%1,%2]", (int)v[0], (int)v[2]);
		}
		return o + "]";
	}

	// ---- Killfeed hooks (player + AI deaths, server-side) ----
	protected void FFRX_SubscribeKills()
	{
		if (m_bKillsSubbed) { GetGame().GetCallqueue().Remove(FFRX_SubscribeKills); return; }
		JWK_GameMode gm = JWK.GetGameMode();
		if (!gm) return; // not ready yet -> retry on next CallLater
		gm.GetOnPlayerKilled().Insert(FFRX_OnPlayerKilled);

		// Kills IA. On cherchait autrefois un invoker cote FF (JWK_AIManagerComponent),
		// absent de la FF installee -> "Unknown type", et le flux IA etait reste coupe.
		// La bonne source est l'invoker du JEU DE BASE, present quelle que soit la
		// version de FF (SCR_BaseGameMode.c l.653). Methode reprise du mod ReforgerJS
		// Support, qui s'y branche exactement pareil.
		gm.GetOnControllableDestroyed().Insert(FFRX_OnControllableDestroyed);
		m_bKillsSubbed = true;
		GetGame().GetCallqueue().Remove(FFRX_SubscribeKills);
		Print("[FFRX][Kill] killfeed subscribed", LogLevel.NORMAL);
	}

	protected void FFRX_OnPlayerKilled(SCR_InstigatorContextData ctx)
	{
		if (!ctx) return;
		int vpid = ctx.GetVictimPlayerID();
		IEntity vent = GetGame().GetPlayerManager().GetPlayerControlledEntity(vpid);
		FFRX_RecordKill(vent, vpid, ctx.GetInstigator());
	}

	protected void FFRX_OnAIKilled(IEntity ai, Instigator inst)
	{
		FFRX_RecordKill(ai, 0, inst);
	}

	// Mort d'une entite controlable non-joueur -> killfeed IA.
	//
	// Cet invoker se declenche pour TOUT ce qui est detruit (vehicules, tourelles,
	// composants), d'ou les deux filtres, calques sur RJSSupport_GameMode :
	//   1. victimPlayerId > 0 = un joueur, deja traite par GetOnPlayerKilled -> on
	//      sort, sinon chaque mort de joueur serait comptee DEUX fois ;
	//   2. la victime doit etre un SCR_ChimeraCharacter, sinon une epave de vehicule
	//      remonterait comme un "soldat tue".
	protected void FFRX_OnControllableDestroyed(SCR_InstigatorContextData ctx)
	{
		if (!ctx) return;
		if (ctx.GetVictimPlayerID() > 0) return;

		IEntity victim = ctx.GetVictimEntity();
		if (!victim) return;
		if (!SCR_ChimeraCharacter.Cast(victim)) return;

		FFRX_OnAIKilled(victim, ctx.GetInstigator());
	}

	protected void FFRX_RecordKill(IEntity victimEnt, int victimPlayerId, Instigator inst)
	{
		if (!m_aKills) m_aKills = {};

		string vName = FFRX_ActorName(victimEnt, victimPlayerId);
		string vRole = RoleStr(FFRX_ActorRole(victimEnt));

		string kName = "?";
		string kRole = "UNDEFINED";
		int kPid = 0;
		IEntity kEnt = null;
		if (inst)
		{
			kPid = inst.GetInstigatorPlayerID();
			kEnt = inst.GetInstigatorEntity();
		}
		if (kEnt || kPid > 0)
		{
			kName = FFRX_ActorName(kEnt, kPid);
			kRole = RoleStr(FFRX_ActorRole(kEnt));
		}

		vector pos = "0 0 0";
		if (victimEnt) pos = victimEnt.GetOrigin();
		else if (kEnt) pos = kEnt.GetOrigin();

		int t = JWK_GameplayTimestamp.GetSystemAccumulator();
		string obj = string.Format("{\"t\":%1,\"vic\":\"%2\",\"vrole\":\"%3\",\"kil\":\"%4\",\"krole\":\"%5\",\"x\":%6,\"z\":%7}",
			t, Esc(vName), vRole, Esc(kName), kRole, (int)pos[0], (int)pos[2]);

		m_aKills.Insert(obj);
		while (m_aKills.Count() > 40) m_aKills.RemoveOrdered(0);
	}

	// Player -> profile name ; AI -> "IA".
	protected string FFRX_ActorName(IEntity e, int playerId)
	{
		if (playerId > 0)
		{
			string pn = GetGame().GetPlayerManager().GetPlayerName(playerId);
			if (pn != "") return pn;
		}
		return "IA";
	}

	protected JWK_EFactionRole FFRX_ActorRole(IEntity e)
	{
		if (!e) return JWK_EFactionRole.UNDEFINED;
		return JWK.GetFactions().GetEntityRole(e);
	}

	// Buildings placed on a player FOB: type (config title) + category + supply storage.
	protected string FFRX_FobBuildings(JWK_PlayerManagedSiteComponent site)
	{
		JWK_BuildAreaControllerComponent ba = site.GetBuildArea();
		if (!ba) return "";
		array<IEntity> items = {};
		ba.GetBuildItems(items);

		JWK_ConstructionManagerComponent cm = JWK.GetConstruction();
		string s = "";
		int n = 0;
		foreach (IEntity it : items)
		{
			if (!it) continue;
			if (n >= 60) break; // garde-fou

			string type = "";
			int cat = 0;
			if (cm)
			{
				ResourceName prefab = JWK_PrefabUtils.GetEntityPrefabName(it);
				JWK_BuildItemConfig cfg = cm.GetBuildItemConfigForPrefab(prefab);
				if (cfg)
				{
					type = cfg.m_sTitle;
					cat = cfg.m_iCategory;
				}
			}

			int bsup = -1;
			int bsupMax = -1;
			JWK_LogisticsStorageControllerComponent st = JWK_CompTU<JWK_LogisticsStorageControllerComponent>.FindIn(it);
			if (st)
			{
				bsup = st.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
				bsupMax = st.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES);
			}

			if (s != "") s = s + ",";
			s = s + string.Format("{\"type\":\"%1\",\"cat\":%2,\"supplies\":%3,\"suppliesMax\":%4}", Esc(type), cat, bsup, bsupMax);
			n = n + 1;
		}
		return s;
	}

	// Garrison manpower = alive AI characters of the entity's force (-1 if no force component).
	protected int FFRX_Garrison(IEntity e)
	{
		JWK_AIForceComponent afc = JWK_CompTU<JWK_AIForceComponent>.FindIn(e);
		if (!afc) return -1;
		JWK_AIForce force = afc.GetForce_S();
		if (!force) return -1;
		return force.GetAliveCharactersNum();
	}

	protected string RoleStr(JWK_EFactionRole r)
	{
		switch (r)
		{
			case JWK_EFactionRole.PLAYER:     return "PLAYER";
			case JWK_EFactionRole.ENEMY:      return "ENEMY";
			case JWK_EFactionRole.SUPPORTING: return "SUPPORTING";
			case JWK_EFactionRole.AMBIENT:    return "AMBIENT";
			case JWK_EFactionRole.NONE:       return "NONE";
		}
		return "UNDEFINED";
	}

	protected string PatrolStateStr(int s)
	{
		switch (s)
		{
			case 1: return "spawn";
			case 2: return "active";
			case 3: return "returning";
			case 4: return "finished";
			case 5: return "failed";
		}
		return "";
	}

	// Extra town/POI info for the map tooltip. Returns a JSON fragment that STARTS
	// with a comma (or "" when the entity is not a town). Mirrors the fields FF shows
	// on its in-game map info panel (Hearts & Minds + logistics + frontline).
	protected string FFRX_TownExtra(IEntity e)
	{
		string frag = "";

		JWK_HeartsAndMindsLocationComponent hm = JWK_CompTU<JWK_HeartsAndMindsLocationComponent>.FindIn(e);
		if (hm)
		{
			frag = frag + string.Format(",\"supporters\":%1,\"hostiles\":%2,\"threat\":%3",
				hm.GetSupporters(), hm.GetHostiles(), hm.GetThreat());
		}

		JWK_LogisticsStorageControllerComponent log = JWK_CompTU<JWK_LogisticsStorageControllerComponent>.FindIn(e);
		if (log)
		{
			int sup = log.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
			int supMax = log.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES);
			frag = frag + string.Format(",\"supplies\":%1,\"suppliesMax\":%2", sup, supMax);

			// Le CARBURANT n'etait remonte que pour les FOB. Les stations-service
			// (JWK_FuelStationEntity) sont des POI comme les autres : sans ces deux
			// champs, leur stock etait invisible sur la carte admin, alors que c'est la
			// seule source de fuel de la carte et qu'elle se regenere lentement
			// (3-6% de la capacite par heure de jeu, et d'autant plus souvent que le
			// reservoir est vide). Meme composant, deux lignes de plus.
			int fu = log.GetResources(JWK_ELogisticsResourceType.FUEL);
			int fuMax = log.GetMaxResources(JWK_ELogisticsResourceType.FUEL);
			if (fuMax > 0)
				frag = frag + string.Format(",\"fuel\":%1,\"fuelMax\":%2", fu, fuMax);
		}

		// Production horaire d'une usine, pour afficher la VITESSE de generation et pas
		// seulement le stock. m_fBaseSuppliesProduction est un [Attribute] public (100
		// par defaut, "per in-game hour") ; le facteur vient des reglages de partie.
		//
		// C'est un maximum THEORIQUE : le calcul reel (GetResourceProduction, protected)
		// applique en plus le facteur horaire jour/nuit, et une efficacite aleatoire de
		// 0 a 50% si l'usine est tenue par l'ennemi -- lequel cesse meme de produire
		// des que son stock depasse la moitie. Une usine reprise produit donc au moins
		// le double de ce qu'elle donnait a l'adversaire.
		// Capacite NOMINALE de l'usine (prodMax), a ne pas confondre avec le debit reel
		// du resume, qui lui est mesure (FFRX_ProductionTracker). Ici c'est le plafond
		// theorique du site : utile pour comparer deux usines entre elles.
		JWK_FactoryEntity fac = JWK_FactoryEntity.Cast(e);
		if (fac)
		{
			float pfactor = 1;
			if (JWK.GameSettingsCache())
				pfactor = JWK.GameSettingsCache().m_fFactoryResourceProductionFactor;
			frag = frag + string.Format(",\"prodMax\":%1", (int)(fac.m_fBaseSuppliesProduction * pfactor));
		}

		// Frontline flag (node bordering an enemy territory).
		JWK_TerritoryControlNodeComponent node = JWK_CompTU<JWK_TerritoryControlNodeComponent>.FindIn(e);
		if (node)
		{
			int border = 0;
			if (node.IsBorder()) border = 1;
			frag = frag + string.Format(",\"border\":%1", border);
		}

		// Active negative effects = THREAT + HOSTILES modifiers (named), as a string array.
		if (hm)
		{
			string effects = FFRX_Effects(hm);
			frag = frag + string.Format(",\"effects\":[%1]", effects);
		}

		return frag;
	}

	// Collect the titles of the active THREAT + HOSTILES modifiers on a town.
	protected string FFRX_Effects(JWK_HeartsAndMindsLocationComponent hm)
	{
		string s = "";
		s = FFRX_AppendModifiers(s, hm.GetModifierSystemData(JWK_ETownModifierSystem.THREAT));
		s = FFRX_AppendModifiers(s, hm.GetModifierSystemData(JWK_ETownModifierSystem.HOSTILES));
		return s;
	}

	protected string FFRX_AppendModifiers(string acc, JWK_OverTimeModifierSystemData data)
	{
		if (!data) return acc;
		foreach (JWK_OverTimeModifier m : data.m_aModifiers)
		{
			if (!m || !m.m_Config) continue;
			string title = m.m_Config.title;
			if (title == "") title = m.m_Config.name;
			if (title == "") continue;
			if (acc != "") acc = acc + ",";
			acc = acc + string.Format("\"%1\"", Esc(title));
		}
		return acc;
	}

	// JSON escaping. Go's json parser REJECTS any unescaped CONTROL char (ASCII < 32) inside
	// a string -> a single raw control byte anywhere (Diag stats, a display name, gear...)
	// made the whole /ffstate POST a 400. Enforce can't Replace arbitrary control bytes, so
	// after escaping the JSON metacharacters we rebuild the string char by char, turning any
	// control char (< 32) or DEL (127) into a space. Bytes >= 128 (UTF-8) are kept as-is.
	protected string Esc(string s)
	{
		string e = s;
		e.Replace("\\", "\\\\");
		e.Replace("\"", "\\\"");

		int n = e.Length();
		string o = "";
		for (int i = 0; i < n; i++)
		{
			int c = e.ToAscii(i);
			if (c >= 32 && c != 127)
				o = o + e.Substring(i, 1);
			else
				o = o + " ";
		}
		return o;
	}

	// ------------------------------------------------------------------------------------
	//  Accumulateurs du snapshot, remontes en CHAMPS DE CLASSE.
	//
	//  POURQUOI : BuildJson() faisait 429 lignes et le compilateur Enforce a fini par
	//  rendre "Too many instructions per function" -- une erreur qui ne cite PAS la
	//  fonction fautive mais une cinquantaine de fichiers du jeu de base et de FF, ce
	//  qui rend le diagnostic trompeur (cf. la cascade decrite dans CLAUDE.md).
	//  On a donc extrait les deux plus gros blocs (POIs, FOBs) dans leurs propres
	//  methodes ; leurs resultats transitent par ces champs plutot que par vingt
	//  parametres de sortie.
	//
	//  Ils sont remis a zero au debut de FFRX_BuildPois().
	// ------------------------------------------------------------------------------------
	protected int pTotal, pPlayer, pEnemy, enemyManpower;
	protected int supTotal, supMaxTotal, fuelTotal, fuelMaxTotal;
	protected int supAll, supMaxAll, fuelAll, fuelMaxAll;
	protected int prodTotal, fuelProdTotal;
	protected int facPlayer, facTotal, fuelStationsPlayer, fuelStationsTotal;
	protected int playerPct, enemyPct;

	//! Balaie tous les sites controles : construit le JSON des POIs et remplit les
	//! accumulateurs d'economie et de territoire ci-dessus.
	protected string FFRX_BuildPois(World world)
	{
		pTotal = 0; pPlayer = 0; pEnemy = 0; enemyManpower = 0;
		supTotal = 0; supMaxTotal = 0; fuelTotal = 0; fuelMaxTotal = 0;
		supAll = 0; supMaxAll = 0; fuelAll = 0; fuelMaxAll = 0;
		prodTotal = 0; fuelProdTotal = 0;
		facPlayer = 0; facTotal = 0; fuelStationsPlayer = 0; fuelStationsTotal = 0;
		playerPct = 0; enemyPct = 0;

		// ---- POIs : TOUS les lieux controles (villes, bases, usines, checkpoints, radio) ----
		string pois = "";

		// Economie (sites TENUS uniquement) : stocks, capacites et vitesses de generation.
		// Repond a "combien on a, et a quelle vitesse ca rentre" sans avoir a ouvrir
		// chaque site un par un.
		// Meme mesure sur TOUTE la carte (nos sites + ceux de l'ennemi). Sans cette
		// seconde serie, on ne voit que ce qu'on possede, jamais ce qui reste a prendre :
		// 3000 supplies ne racontent pas la meme campagne selon que la carte en contient
		// 4000 ou 40000. Les usines ennemies produisent, elles aussi.

		array<GenericComponent> fclist = JWK_IndexSystem.Get(world).GetAllGC(JWK_FactionControlComponent);
		foreach (GenericComponent gc : fclist)
		{
			JWK_FactionControlComponent fc = JWK_FactionControlComponent.Cast(gc);
			if (!fc) continue;
			IEntity e = fc.GetOwner();
			if (!e) continue;

			vector p = e.GetOrigin();

			// Dedicated server: some indexed control components have no faction
			// affiliation yet -> GetFactionRole() would deref null (spammed the log
			// every tick). Skip them.
			if (!fc.Affiliation()) continue;
			JWK_EFactionRole role = fc.GetFactionRole();

			string nm = "";
			JWK_NamedLocationComponent nl = JWK_CompTU<JWK_NamedLocationComponent>.FindIn(e);
			if (nl) nm = nl.GetName();

			int garrison = FFRX_Garrison(e);

			// Type du POI. "town" n'est PLUS le defaut : c'etait un fourre-tout.
			// Beaucoup d'entites portent un JWK_FactionControlComponent sans etre un
			// point d'interet (stockages, controleurs, sous-objets d'une base). Elles
			// tombaient toutes en "town", sans nom, et se superposaient exactement au
			// vrai POI -- d'ou "Landay Military Airfield" (BASE) double d'une "town"
			// anonyme au meme endroit. On classe donc explicitement, et on IGNORE ce
			// qu'on ne reconnait pas.
			string ptype = "";
			int pop = -1;
			JWK_TownEntity town = JWK_TownEntity.Cast(e);
			if (town) pop = town.m_iPopulation;

			// Detection par TYPE D'ENTITE et non par composant.
			//
			// L'ancienne version cherchait JWK_FactoryResourceStorageComponent pour les
			// usines : ce composant n'existe pas sur JWK_FactoryEntity, qui porte un
			// JWK_LogisticsStorageControllerComponent. Resultat : AUCUNE usine n'etait
			// classee (0 usine et 0 base remontees alors que le serveur en annonce 10 et 6
			// au demarrage) -- elles retombaient toutes en "town", faussant la livemap et
			// l'historique economique.
			//
			// Le cast d'entite est plus sur : il ne depend pas de la composition interne
			// des prefabs FF, qui peut changer d'une version a l'autre.
			if (JWK_CheckpointEntity.Cast(e)) ptype = "checkpoint";
			else if (JWK_FactoryEntity.Cast(e)) ptype = "factory";
			else if (JWK_FuelStationEntity.Cast(e)) ptype = "fuel";
			else if (JWK_MilitaryBaseEntity.Cast(e)) ptype = "base";
			else if (town) ptype = "town";
			else if (JWK_CompTU<JWK_RadioTowerComponent>.FindIn(e)) ptype = "radio";

			// Entite non identifiee : ce n'est pas un point d'interet, on ne l'emet pas.
			// Elle n'apporterait qu'une icone anonyme superposee a un vrai POI.
			if (ptype == "") continue;

			// Rich town/POI info (supporters/hostiles/threat/supplies/effects/border) for the map tooltip.
			string extra = FFRX_TownExtra(e);

			if (pois != "") pois = pois + ",";
			pois = pois + string.Format("{\"type\":\"%1\",\"name\":\"%2\",\"x\":%3,\"z\":%4,\"role\":\"%5\",\"garrison\":%6,\"pop\":%7%8}",
				ptype, Esc(nm), (int)p[0], (int)p[2], RoleStr(role), garrison, pop, extra);

			pTotal = pTotal + 1;
			if (role == JWK_EFactionRole.PLAYER) pPlayer = pPlayer + 1;
			else if (role == JWK_EFactionRole.ENEMY)
			{
				pEnemy = pEnemy + 1;
				if (garrison > 0) enemyManpower = enemyManpower + garrison;
			}

			// Totaux d'economie, pour repondre d'un coup d'oeil a "de combien on dispose
			// et a quelle vitesse ca rentre". On ne compte QUE les sites tenus : c'est ce
			// dont les joueurs disposent reellement, pas ce qui traine sur la carte.
			JWK_LogisticsStorageControllerComponent slog =
				JWK_CompTU<JWK_LogisticsStorageControllerComponent>.FindIn(e);
			if (slog)
			{
				int sSup    = slog.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
				int sSupMax = slog.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES);
				int sFuel   = slog.GetResources(JWK_ELogisticsResourceType.FUEL);
				int sFuelMax= slog.GetMaxResources(JWK_ELogisticsResourceType.FUEL);

				// Serie "carte entiere" : tous les sites, quel que soit le camp.
				supAll     = supAll + sSup;
				supMaxAll  = supMaxAll + sSupMax;
				fuelAll    = fuelAll + sFuel;
				fuelMaxAll = fuelMaxAll + sFuelMax;

				// Serie "tenu" : ce dont les joueurs disposent reellement.
				if (role == JWK_EFactionRole.PLAYER)
				{
					supTotal = supTotal + sSup;
					supMaxTotal = supMaxTotal + sSupMax;
					fuelTotal = fuelTotal + sFuel;
					fuelMaxTotal = fuelMaxTotal + sFuelMax;
				}
			}

			// On ne compte plus que les SITES : les debits de production viennent de
			// FFRX_ProductionTracker, qui les MESURE au lieu de les estimer.
			if (JWK_FactoryEntity.Cast(e))
			{
				facTotal = facTotal + 1;
				if (role == JWK_EFactionRole.PLAYER) facPlayer = facPlayer + 1;
			}
			else if (JWK_FuelStationEntity.Cast(e))
			{
				fuelStationsTotal = fuelStationsTotal + 1;
				if (role == JWK_EFactionRole.PLAYER) fuelStationsPlayer = fuelStationsPlayer + 1;
			}
		}

		// Debits REELS de la derniere heure de jeu complete (0 tant qu'aucune heure n'est
		// ecoulee depuis le demarrage). Voir FFRX_ProductionTracker : on lit le stock
		// avant/apres la production de FF plutot que de rejouer sa formule, qui est
		// protected et depend d'un facteur horaire et d'un alea.
		prodTotal = FFRX_ProductionTracker.GetSuppliesPerHour();
		fuelProdTotal = FFRX_ProductionTracker.GetFuelPerHour();

		int playerPct = 0;
		int enemyPct = 0;
		if (pTotal > 0)
		{
			playerPct = (pPlayer * 100) / pTotal;
			enemyPct = (pEnemy * 100) / pTotal;
		}
		return pois;
	}

	//! Sites geres par les joueurs (FOB, camps) : JSON + appoint des totaux d'economie.
	protected string FFRX_BuildFobs(World world)
	{
		// ---- FOBs (player-managed sites) : buildings + storage ----
		string fobs = "";
		array<GenericComponent> fobList = JWK_IndexSystem.Get(world).GetAllGC(JWK_PlayerManagedSiteComponent);
		foreach (GenericComponent fgc : fobList)
		{
			JWK_PlayerManagedSiteComponent site = JWK_PlayerManagedSiteComponent.Cast(fgc);
			if (!site || !site.GetOwner()) continue;
			IEntity fe = site.GetOwner();

			// JWK_PlayerManagedSiteComponent n'est PAS reserve aux FOB : il est aussi
			// porte par JWK_FactoryController et JWK_MilitaryBaseController. Sans ce
			// filtre, chaque usine et chaque base militaire etait emise DEUX fois --
			// une fois dans "pois" (avec son type et ses infos), une fois ici comme
			// FOB. Sur la carte les deux icones se superposaient exactement, celle de
			// la FOB masquant celle de la base : on perdait l'info du site.
			// On ne garde donc que les sites construits par les joueurs.
			if (JWK_FactoryEntity.Cast(fe) || JWK_MilitaryBaseEntity.Cast(fe)) continue;

			vector fp = fe.GetOrigin();

			// ⚠️ NE PAS utiliser JWK.GetFactions().GetEntityRole() ICI.
			// Cette methode exige un FactionAffiliationComponent, et le prefab de FOB
			// (JWK_PlayerFOBController.et) n'en a PAS : elle renvoyait donc UNDEFINED
			// pour toute FOB. Consequences observees : icone de FOB sans camp sur la
			// carte, et surtout ravitaillement des FOB jamais compte dans l'economie --
			// le site affichait 0 alors que la FOB de depart contenait 5000 supplies.
			//
			// La FOB porte en revanche un JWK_FactionControlComponent (m_iDefaultRole
			// PLAYER), qui est la source correcte -- c'est deja ce qu'utilise la boucle
			// des POI plus haut.
			JWK_EFactionRole frole = JWK_EFactionRole.UNDEFINED;
			JWK_FactionControlComponent ffc = JWK_CompTU<JWK_FactionControlComponent>.FindIn(fe);
			if (ffc && ffc.Affiliation())
				frole = ffc.GetFactionRole();

			string fname = "";
			JWK_NamedLocationComponent fnl = JWK_CompTU<JWK_NamedLocationComponent>.FindIn(fe);
			if (fnl) fname = fnl.GetName();

			// FOB-wide storage (sum of all linked storages).
			int fsup = -1;
			int fsupMax = -1;
			int ffuel = -1;
			int ffuelMax = -1;
			JWK_LogisticsAreaStorageControllerComponent la = site.GetLogisticsArea();
			if (la)
			{
				fsup = la.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
				fsupMax = la.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES);
				ffuel = la.GetResources(JWK_ELogisticsResourceType.FUEL);
				ffuelMax = la.GetMaxResources(JWK_ELogisticsResourceType.FUEL);

				// ⚠️ LES FOB COMPTENT DANS L'ECONOMIE. Elles sont emises dans "fobs" et
				// PAS dans "pois" (elles n'ont aucun des 6 types de POI reconnus), donc
				// la boucle des POI -- qui calcule les totaux -- les ignorait totalement.
				// Resultat : en debut de campagne, ou la FOB de depart est le SEUL site
				// tenu, le site et la carte tactique affichaient 0 ravitaillement alors
				// que la FOB en contenait 5000. Regression introduite en meme temps que
				// la classification explicite des POI (le `continue` sur type inconnu).
				//
				// Pas de double comptage : la boucle des POI ecarte deja les FOB (type
				// vide -> continue), et les usines/bases sont ecartees d'ici plus haut.
				if (fsup > 0)     supAll     = supAll + fsup;
				if (fsupMax > 0)  supMaxAll  = supMaxAll + fsupMax;
				if (ffuel > 0)    fuelAll    = fuelAll + ffuel;
				if (ffuelMax > 0) fuelMaxAll = fuelMaxAll + ffuelMax;

				if (frole == JWK_EFactionRole.PLAYER)
				{
					if (fsup > 0)     supTotal     = supTotal + fsup;
					if (fsupMax > 0)  supMaxTotal  = supMaxTotal + fsupMax;
					if (ffuel > 0)    fuelTotal    = fuelTotal + ffuel;
					if (ffuelMax > 0) fuelMaxTotal = fuelMaxTotal + ffuelMax;
				}
			}

			string builds = FFRX_FobBuildings(site);

			if (fobs != "") fobs = fobs + ",";
			string fhead = string.Format("{\"name\":\"%1\",\"x\":%2,\"z\":%3,\"role\":\"%4\"", Esc(fname), (int)fp[0], (int)fp[2], RoleStr(frole));
			string fstore = string.Format(",\"supplies\":%1,\"suppliesMax\":%2,\"fuel\":%3,\"fuelMax\":%4", fsup, fsupMax, ffuel, ffuelMax);
			fobs = fobs + fhead + fstore + string.Format(",\"buildings\":[%1]}", builds);
		}
		return fobs;
	}

}
