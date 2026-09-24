// FF - REMIXED - PVE
// Le fichier des civils rencontres part vers le site (endpoint /ffcivs).
//
// ======================================================================================
//  A QUOI CA SERT
// ======================================================================================
//
// Le registre civil (FFRX_CivIdentityRegistry) vit dans $profile:FFRX_civilians.json, sur
// le serveur. Personne ne peut le lire en jeu. Cette page-la existe pour deux raisons :
//
//   1. VERIFIER que la persistance marche vraiment -- qu'un civil croise hier a toujours
//      son nom, sa confiance et son historique de controles aujourd'hui.
//   2. Servir de BASE DE RENSEIGNEMENT : qui a ete controle, ou, et combien de ces
//      controles ont donne un espion.
//
// ======================================================================================
//  CE QU'ON ENVOIE, ET CE QU'ON N'ENVOIE PAS
// ======================================================================================
//
// On envoie les civils PERSISTES -- ceux avec qui on a reellement interagi. Pas tous les
// civils apercus : le fichier se remplirait de gens a qui personne n'a jamais parle.
//
// RAPPEL SUR CE QU'EST UN "CIVIL" ICI (depuis 2026-09-18) : chaque ZONE (carre d'1 km)
// possede une liste d'HABITANTS. Quand on croise un civil, on lui attribue un habitant de
// sa zone qui n'est pas deja incarne. Les memes visages reviennent donc dans leur village
// avec leur memoire. Ce qu'on ne garantit PAS, faute de controler le spawn de FF : qu'un
// habitant donne soit present a un instant donne, ni qu'un meme groupe se recompose.
//
// ======================================================================================
//  TRANSPORT
// ======================================================================================
//
// Identique a FFRX_SpawnCensus : URL derivee de $profile:Fleet/GTG.json, header Bearer,
// remplacement complet a chaque envoi (pas de delta a reconcilier cote site).
//
// JSON assemble avec des '+' et NON string.Format : celui-ci tronque sa sortie a ~8 Ko et
// une longue campagne depasse largement ca (cf. memoire enforce-stringformat-8kb-truncation,
// qui nous avait deja coute les 400 sur /ffstate).
//
// NOTE : ASCII uniquement dans les chaines (le dedie compile en strict).

class FFRX_CivSenderCb : RestCallback
{
	override void OnError(int errorCode)
	{
		Print(string.Format("[FFRX][Civ] POST /ffcivs erreur reseau (%1).", errorCode), LogLevel.WARNING);
	}

	override void OnTimeout()
	{
		Print("[FFRX][Civ] POST /ffcivs : delai depasse.", LogLevel.WARNING);
	}

	override void OnSuccess(string data, int dataSize)
	{
	}
}

class FFRX_CivSender
{
	//! Cadence d'envoi. Plus court que le recensement (10 min) parce que la donnee change a
	//! chaque interaction et qu'on veut pouvoir verifier un controle peu apres l'avoir fait.
	protected static const int SEND_MS = 300000;   // 5 min

	protected static bool s_bStarted;
	protected static string s_sUrl;
	protected static string s_sApiKey;
	protected static ref FFRX_CivSenderCb s_Cb;

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_bStarted)
			return;

		s_bStarted = true;
		LoadEndpoint();

		if (s_sUrl == "")
			return;

		GetGame().GetCallqueue().CallLater(Send, SEND_MS, true);
		Print("[FFRX][Civ] envoi du fichier civil au site actif (toutes les 5 min).", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static void Send()
	{
		if (s_sUrl == "")
			return;

		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		array<ref FFRX_CivSaveEntry> list = {};
		FFRX_CivIdentityRegistry.Get().FFRX_Snapshot(list);
		if (list.IsEmpty())
			return;

		RestContext rc = api.GetContext(s_sUrl);
		rc.SetHeaders(string.Format("Authorization, Bearer %1", s_sApiKey));
		rc.POST(s_Cb, "", BuildJson(list));
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildJson(notnull array<ref FFRX_CivSaveEntry> list)
	{
		string body = "{\"entries\":[";
		bool first = true;

		foreach (FFRX_CivSaveEntry e : list)
		{
			if (!e || e.key == "")
				continue;

			if (!first)
				body = body + ",";
			first = false;

			body = body + "{\"k\":\"" + e.key + "\"";
			body = body + ",\"n\":\"" + Escape(e.name) + "\"";
			body = body + ",\"t\":" + e.trust.ToString();
			body = body + ",\"i\":" + e.interactions.ToString();
			body = body + ",\"f\":" + e.frisks.ToString();
			body = body + ",\"s\":" + e.spyFound.ToString();
			// La zone et la derniere position vue sont PERSISTEES par le registre : on ne les
			// recalcule plus ici. Deux calculs separes auraient fini par diverger.
			body = body + ",\"x\":" + Math.Round(e.x).ToString();
			body = body + ",\"z\":" + Math.Round(e.z).ToString();
			body = body + ",\"zn\":\"" + Escape(e.zone) + "\"}";
		}

		return body + "]}";
	}

	//------------------------------------------------------------------------------------------------
	//! Les noms sont generes par nous (liste fixe, sans guillemet ni backslash), mais on
	//! echappe quand meme : le jour ou la liste s'ouvrira a autre chose, un seul guillemet
	//! casserait tout le JSON et le site rejetterait l'envoi entier.
	protected static string Escape(string s)
	{
		string outStr = s;
		outStr.Replace("\\", "\\\\");
		outStr.Replace("\"", "\\\"");
		return outStr;
	}

	//------------------------------------------------------------------------------------------------
	protected static string DeriveUrl(string base, string suffix)
	{
		int idx = base.IndexOf("positions");
		if (idx < 0)
			return "";
		return base.Substring(0, idx) + suffix;
	}

	//------------------------------------------------------------------------------------------------
	protected static void LoadEndpoint()
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile("$profile:Fleet/GTG.json"))
		{
			Print("[FFRX][Civ] $profile:Fleet/GTG.json absent -> envoi du fichier civil desactive.", LogLevel.WARNING);
			return;
		}

		string posUrl = "";
		ctx.ReadValue("url", posUrl);
		ctx.ReadValue("apiKey", s_sApiKey);
		if (posUrl == "" || s_sApiKey == "")
		{
			Print("[FFRX][Civ] url/apiKey manquants dans GTG.json -> envoi du fichier civil desactive.", LogLevel.WARNING);
			return;
		}

		s_sUrl = DeriveUrl(posUrl, "ffcivs");
		s_Cb = new FFRX_CivSenderCb();
	}
}
