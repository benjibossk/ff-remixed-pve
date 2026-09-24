// FF - REMIXED - PVE
// Accoutumance a l'arme : plus on porte une arme, plus on la tient stable.
//
// ------------------------------------------------------------------------------------
// L'INTENTION
//
// Le malus "arme etrangere" dit ce qu'on ne maitrise PAS. Il manquait le versant positif :
// un soldat qui garde la meme arme depuis des heures doit la tenir mieux qu'au premier
// jour. Ca recompense la fidelite a une arme plutot que le changement permanent, et ca
// donne du sens au choix d'une dotation.
//
// Le gain est volontairement MODESTE (-25 % de tremblement au maximum) et LENT (4 h de
// port). Un bonus fort creerait deux classes de joueurs : les anciens intouchables et les
// nouveaux impuissants -- exactement l'effet qu'on veut eviter sur un serveur ou des
// recrues arrivent en permanence.
//
// ------------------------------------------------------------------------------------
// POURQUOI LE SERVEUR COMPTE, ET PAS LE CLIENT
//
// Le tremblement est calcule cote CLIENT (FFRX_ADSSway est un composant de camera). Mais
// laisser le client compter son propre temps de jeu reviendrait a lui laisser fixer son
// propre bonus. Le SERVEUR echantillonne donc l'arme portee par chaque joueur, cumule le
// temps par (UID, prefab), persiste le tout, et POUSSE le facteur au client concerne.
//
// On compte le temps ARME EN MAIN, pas le temps en visee : le serveur ne sait pas de
// facon fiable si un joueur est en ADS, et "porter" est de toute facon la bonne mesure
// de l'accoutumance a une arme.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

// ------------------------------------------------------------------------------------
//  DIAGNOSTIC : le malus "arme etrangere" frappe-t-il NOS armes ?
// ------------------------------------------------------------------------------------
//
// LA QUESTION A TRANCHER : quand le FAMAS ou la Minimi tremblent, est-ce (a) notre malus
// qui les classe comme armes ennemies, ou (b) le reglage de l'arme dans le mod AMF ?
//
// La regle appliquee par FFRX_ADSSway est deja la version prudente : une arme n'est
// "etrangere" QUE si elle appartient au catalogue de la faction ENNEMIE. Nos armes,
// l'arsenal, le neutre et l'inconnu ne subissent AUCUN malus. Donc pour que le FAMAS soit
// penalise, il faudrait qu'il figure dans le catalogue ennemi -- ce qui serait surprenant.
//
// Ce diagnostic repond sans ambiguite :
//   "ennemi=non"  -> le multiplicateur vaut 1.0, notre code ne touche RIEN.
//                    Le tremblement vient de l'arme elle-meme (dispersion/recul du mod).
//   "ennemi=OUI"  -> c'est bien nous, et la piste devient le peuplement des catalogues.
//
// On imprime aussi l'appartenance au catalogue JOUEUR : si elle est "non" pour toutes nos
// armes, c'est que le catalogue de la faction joueur est vide ou mal peuple (piste 1 de la
// roadmap, redirection FIA_DESERT), information utile meme si le verdict est "non".
class FFRX_WeaponCatalogDiag
{
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<string> s_aSeen;

	protected static array<string> Seen()
	{
		if (!s_aSeen)
			s_aSeen = new array<string>();

		return s_aSeen;
	}

	//! Une ligne par prefab d'arme distinct. Sans ce verrou, on ecrirait la meme ligne
	//! toutes les 30 s par joueur -- un log illisible dit la meme chose qu'aucun log.
	static void LogOnce(string wpnPrefab)
	{
		if (wpnPrefab == "")
			return;
		if (Seen().Contains(wpnPrefab))
			return;
		Seen().Insert(wpnPrefab);

		JWK_FactionManager fm = JWK.GetFactions();
		if (!fm)
		{
			Print("[FFRX][SwayDiag] gestionnaire de factions indisponible.", LogLevel.WARNING);
			return;
		}

		string myKey = "(aucune)";
		string mine  = "?";
		JWK_Faction playerFac = fm.GetJWKFactionByRole(JWK_EFactionRole.PLAYER);
		if (playerFac)
		{
			myKey = playerFac.GetKey();
			if (playerFac.IsItemInCatalog(wpnPrefab)) mine = "OUI"; else mine = "non";
		}

		string enKey  = "(aucune)";
		string theirs = "?";
		JWK_Faction enemyFac = fm.GetJWKFactionByRole(JWK_EFactionRole.ENEMY);
		if (enemyFac)
		{
			enKey = enemyFac.GetKey();
			if (enemyFac.IsItemInCatalog(wpnPrefab)) theirs = "OUI"; else theirs = "non";
		}

		// Le verdict, en clair : c'est la seule ligne a lire pour trancher.
		string verdict = "PAS DE MALUS (x1.0) -> tremblement = reglage de l'arme, pas nous";
		if (theirs == "OUI")
			verdict = "MALUS APPLIQUE -> c'est bien notre code";

		Print(string.Format("[FFRX][SwayDiag] %1 | catalogue joueur '%2'=%3 | catalogue ennemi '%4'=%5 | %6",
			FFRX_ShortPrefabName(wpnPrefab), myKey, mine, enKey, theirs, verdict), LogLevel.NORMAL);
	}

	//! "{GUID}Prefabs/.../Famas_F1_Inf_RIS.et" -> "Famas_F1_Inf_RIS"
	static string FFRX_ShortPrefabName(string rn)
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

class FFRX_FamiliarityTuning
{
	static const int   SAMPLE_MS  = 30000;  // le serveur echantillonne toutes les 30 s
	static const float FULL_HOURS = 4.0;    // duree de port pour atteindre le bonus maxi

	//! Reduction maximale, lue dans les REGLAGES FF ("Accoutumance arme %", defaut 10 %).
	//! Reglable en jeu plutot que codee en dur : c'est un curseur d'equilibrage, et
	//! chaque serveur doit pouvoir l'annuler (0 %) sans toucher au code.
	static float MaxReduction()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return 0.10;                       // cache pas encore pret -> valeur par defaut
		return c.m_fFFRX_WeaponFamiliarityPct / 100.0;
	}

	//! Facteur multiplicatif applique au tremblement, entre 0.75 (maitrise) et 1.0 (neuf).
	static float FactorFromSeconds(float seconds)
	{
		if (seconds <= 0)
			return 1.0;

		float full = FULL_HOURS * 3600.0;
		float t = seconds / full;
		if (t > 1.0) t = 1.0;

		// Progression en RACINE : les premieres heures apportent le plus, puis ca plafonne.
		// Une progression lineaire donnerait l'impression que rien ne bouge au debut, une
		// exponentielle recompenserait trop tard.
		return 1.0 - (MaxReduction() * Math.Sqrt(t));
	}
}

// ------------------------------------------------------------------------------------
//  CLIENT : le facteur de l'arme courante, pousse par le serveur
// ------------------------------------------------------------------------------------
class FFRX_FamiliarityClient
{
	//! Facteur de l'arme actuellement portee. 1.0 tant que le serveur n'a rien dit --
	//! l'absence d'information ne doit jamais AVANTAGER ni penaliser.
	protected static float s_fFactor = 1.0;

	static float GetFactor() { return s_fFactor; }
	static void  SetFactor(float f)
	{
		if (f < 0.5) f = 0.5;   // garde-fou : jamais plus de -50 %, meme sur donnee aberrante
		if (f > 1.0) f = 1.0;
		s_fFactor = f;
	}
}

// ------------------------------------------------------------------------------------
//  SERVEUR : comptage, persistance, diffusion
// ------------------------------------------------------------------------------------
class FFRX_FamiliarityStore : JsonApiStruct
{
	ref array<string> keys;    // "uid|prefab"
	ref array<float>  secs;

	void FFRX_FamiliarityStore()
	{
		keys = {};
		secs = {};
		RegV("keys"); RegV("secs");
	}
}

class FFRX_Familiarity
{
	protected static const string FILE = "$profile:FFRX/weapon_familiarity.json";

	protected static ref map<string, float> s_mSeconds;

	protected static map<string, float> Seconds()
	{
		if (!s_mSeconds)
			s_mSeconds = new map<string, float>();

		return s_mSeconds;
	}
	protected static bool s_bStarted;
	protected static bool s_bDirty;

	//------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer()) return;
		if (s_bStarted) return;
		s_bStarted = true;

		Load();
		GetGame().GetCallqueue().CallLater(Tick, FFRX_FamiliarityTuning.SAMPLE_MS, true);
		// Ecriture disque espacee : le comptage change toutes les 30 s, mais sauver a
		// chaque tick userait le disque pour rien. Une minute de perte au pire.
		GetGame().GetCallqueue().CallLater(SaveIfDirty, 60000, true);
		Print("[FFRX][Familiarity] comptage demarre.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------
	protected static void Tick()
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm) return;

		array<int> ids = {};
		pm.GetPlayers(ids);

		float add = FFRX_FamiliarityTuning.SAMPLE_MS / 1000.0;

		foreach (int pid : ids)
		{
			IEntity ent = pm.GetPlayerControlledEntity(pid);
			if (!ent) continue;

			string prefab = FFRX_CurrentWeapon(ent);
			if (prefab == "") continue;

			// DIAGNOSTIC (malus "arme etrangere"). On se greffe ICI et pas dans
			// FFRX_ADSSway parce que le log de la camera ne sort QUE si le joueur vise :
			// 11 sessions de log, zero ligne. Le comptage d'accoutumance, lui, echantillonne
			// l'arme EN MAIN toutes les 30 s, sans rien demander au joueur.
			FFRX_WeaponCatalogDiag.LogOnce(prefab);

			string uid = FFRX_LoadoutSystem.UidOfPlayer(pid);
			if (uid == "") continue;

			string key = uid + "|" + prefab;
			float cur = 0;
			Seconds().Find(key, cur);
			cur = cur + add;
			Seconds().Set(key, cur);
			s_bDirty = true;

			FFRX_PushFactor(pid, FFRX_FamiliarityTuning.FactorFromSeconds(cur));
		}
	}

	//! Prefab de l'arme actuellement en main, "" si aucune.
	protected static string FFRX_CurrentWeapon(IEntity character)
	{
		BaseWeaponManagerComponent mgr =
			BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!mgr || !mgr.GetCurrentWeapon())
			return "";
		IEntity w = mgr.GetCurrentWeapon().GetOwner();
		if (!w || !w.GetPrefabData())
			return "";
		return w.GetPrefabData().GetPrefabName();
	}

	protected static void FFRX_PushFactor(int playerId, float factor)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm) return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(pm.GetPlayerController(playerId));
		if (pc)
			pc.FFRX_PushFamiliarity(factor);
	}

	//------------------------------------------------------------------------------------
	//! Temps cumule pour un couple (joueur, arme) -- utile au diagnostic.
	static float GetSeconds(string uid, string prefab)
	{
		float v = 0;
		Seconds().Find(uid + "|" + prefab, v);
		return v;
	}

	//------------------------------------------------------------------------------------
	protected static void SaveIfDirty()
	{
		if (!s_bDirty) return;
		s_bDirty = false;
		Save();
	}

	protected static void Save()
	{
		FFRX_FamiliarityStore st = new FFRX_FamiliarityStore();
		foreach (string k, float v : Seconds())
		{
			st.keys.Insert(k);
			st.secs.Insert(v);
		}
		st.SaveToFile(FILE);
	}

	protected static void Load()
	{
		FFRX_FamiliarityStore st = new FFRX_FamiliarityStore();
		if (!st.LoadFromFile(FILE))
			return;
		if (!st.keys || !st.secs)
			return;

		int n = st.keys.Count();
		if (st.secs.Count() < n) n = st.secs.Count();
		for (int i = 0; i < n; i++)
			Seconds().Set(st.keys[i], st.secs[i]);

		Print(string.Format("[FFRX][Familiarity] %1 couple(s) joueur/arme recharges.", n), LogLevel.NORMAL);
	}
}

// ------------------------------------------------------------------------------------
//  Transport serveur -> client
// ------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	void FFRX_PushFamiliarity(float factor)
	{
		PlayerController local = GetGame().GetPlayerController();
		if (local && local.GetPlayerId() == GetPlayerId())
		{
			FFRX_FamiliarityClient.SetFactor(factor);   // hote local
			return;
		}
		Rpc(FFRX_RpcRecvFamiliarity, factor);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcRecvFamiliarity(float factor)
	{
		FFRX_FamiliarityClient.SetFactor(factor);
	}
}
