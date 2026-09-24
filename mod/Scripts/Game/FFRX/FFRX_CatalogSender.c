// One-time capture of FF reference catalogs (per-faction entity catalogs: characters/vehicles/items).
// Sent ONCE at boot to /ffcatalog (static reference, unlike /ffstate which is live every 5s).
// Retries until the world/catalogs are ready and the POST succeeds. Reuses Fleet config (GTG.json).
// Boot called from FFRX_Groups.c. ASCII only (Enforce compiler desyncs on UTF-8).

class FFRX_CatalogSender
{
	protected static ref FFRX_CatalogSender s_Instance;

	protected string m_sUrl;
	protected string m_sApiKey;
	protected ref FFRX_FFRestCb m_Cb;
	protected bool m_bSent;
	protected int m_iTries;

	static void Boot()
	{
		if (!Replication.IsServer()) return;
		// Nouveau sender a chaque demarrage de partie (le catalogue = capture unique par partie).
		s_Instance = new FFRX_CatalogSender();
		s_Instance.Start();
	}

	void Start()
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile("$profile:Fleet/GTG.json")) return;

		string posUrl = "";
		ctx.ReadValue("url", posUrl);
		ctx.ReadValue("apiKey", m_sApiKey);
		m_sUrl = DeriveUrl(posUrl, "ffcatalog");
		if (m_sUrl == "" || m_sApiKey == "") return;

		m_Cb = new FFRX_FFRestCb();
		m_Cb.m_sTag = "Catalog";   // label logs so /ffcatalog errors are distinct from /ffstate
		// Catalogs may not be ready at boot -> retry until we build a non-empty body and POST it.
		GetGame().GetCallqueue().CallLater(TrySend, 10000, true);
	}

	protected string DeriveUrl(string base, string suffix)
	{
		int idx = base.IndexOf("positions");
		if (idx < 0) return "";
		return base.Substring(0, idx) + suffix;
	}

	protected void TrySend()
	{
		if (m_bSent) { GetGame().GetCallqueue().Remove(TrySend); return; }
		m_iTries = m_iTries + 1;

		string body = BuildCatalogJson();
		if (body == "")
		{
			if (m_iTries > 18) GetGame().GetCallqueue().Remove(TrySend); // give up after ~3 min
			return;
		}

		RestApi api = GetGame().GetRestApi();
		if (!api) return;
		RestContext rc = api.GetContext(m_sUrl);
		if (!rc) return;
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_sApiKey));
		m_Cb.m_iLastBodyBytes = body.Length();
		rc.POST(m_Cb, "", body);

		m_bSent = true;
		GetGame().GetCallqueue().Remove(TrySend);
		Print(string.Format("[FFRX][Catalog] catalogue envoye (%1 B) -> %2", body.Length(), m_sUrl), LogLevel.NORMAL);
	}

	protected string BuildCatalogJson()
	{
		FactionManager fmgr = GetGame().GetFactionManager();
		if (!fmgr) return "";

		array<Faction> factions = {};
		fmgr.GetFactionsList(factions);
		if (factions.IsEmpty()) return "";

		string facJson = "";
		foreach (Faction f : factions)
		{
			SCR_Faction sf = SCR_Faction.Cast(f);
			if (!sf) continue;

			string key = f.GetFactionKey();
			string chars = BuildCatalog(sf, EEntityCatalogType.CHARACTER);
			string vehs = BuildCatalog(sf, EEntityCatalogType.VEHICLE);
			string items = BuildCatalog(sf, EEntityCatalogType.ITEM);

			if (facJson != "") facJson = facJson + ",";
			// NOTE: build with '+' concatenation, NOT string.Format -- Enforce's string.Format
			// TRUNCATES its output at ~8192 bytes, and chars/vehs/items are large. Passing them
			// through Format cut the JSON mid-string (body stuck at exactly 8191 B) -> the server
			// got unterminated JSON -> HTTP 400 "invalid ffcatalog json".
			facJson = facJson + "{\"key\":\"" + Esc(key) + "\",\"characters\":[" + chars
				+ "],\"vehicles\":[" + vehs + "],\"items\":[" + items + "]}";
		}

		return "{\"factions\":[" + facJson + "]}";
	}

	protected string BuildCatalog(SCR_Faction sf, EEntityCatalogType type)
	{
		SCR_EntityCatalog cat = sf.GetFactionEntityCatalogOfType(type);
		if (!cat) return "";

		array<SCR_EntityCatalogEntry> entries = {};
		cat.GetEntityList(entries);

		string s = "";
		int n = 0;
		foreach (SCR_EntityCatalogEntry entry : entries)
		{
			if (!entry) continue;
			if (n >= 400) break; // garde-fou
			ResourceName rn = entry.GetPrefab();
			string ps = rn;
			if (s != "") s = s + ",";
			s = s + string.Format("\"%1\"", Esc(CleanPrefab(ps)));
			n = n + 1;
		}
		return s;
	}

	// "{GUID}Prefabs/.../Name.et" -> "Prefabs/.../Name.et"
	protected string CleanPrefab(string p)
	{
		string o = p;
		int b = o.IndexOf("}");
		if (b >= 0 && b + 1 < o.Length()) o = o.Substring(b + 1, o.Length() - b - 1);
		return o;
	}

	protected string Esc(string s)
	{
		string o = s;
		o.Replace("\\", "\\\\");
		o.Replace("\"", "\\\"");
		return o;
	}
}
