// ============================================================================
//  Flt_RankSystem — grades FIXES pilotés par le portail web / Discord.
//
//  Choix de design (Benji) :
//   - On SUPPRIME le changement de grade automatique par XP. L'XP continue de
//     monter/descendre (stats), mais ne fait JAMAIS changer le grade en jeu.
//   - Le grade est une donnée MANUELLE : posée depuis le web/Discord, stockée
//     par UID dans $profile:Fleet/Ranks.json, ré-appliquée à chaque spawn.
//   - Si un joueur passe sous le seuil XP de son grade, le jeu ne rétrograde
//     PAS ; le portail web affichera l'écart (vue "à engueuler").
//
//  Clé = UID Reforger (BackendApi.GetPlayerIdentityId), stable et = clé Discord.
//  Serveur uniquement (les setters de grade font un RPC vers les clients).
// ============================================================================

// ---------------------------------------------------------------------------
//  Persistance : $profile:Fleet/Ranks.json  ( { entries:[ {uid, rank}, ... ] } )
// ---------------------------------------------------------------------------
class Flt_RankEntry
{
	string uid;
	int    rank;	// valeur de SCR_ECharacterRank
}

class Flt_RankStore
{
	ref array<ref Flt_RankEntry> entries = {};
}

class Flt_RankRegistry
{
	protected static ref Flt_RankRegistry s_Instance;
	const string FILE = "$profile:Fleet/Ranks.json";
	protected ref map<string, int> m_mByUid = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	static Flt_RankRegistry GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new Flt_RankRegistry();
			s_Instance.Load();
		}
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void Load()
	{
		m_mByUid.Clear();
		if (!FileIO.FileExists("$profile:Fleet/"))
			FileIO.MakeDirectory("$profile:Fleet/");
		if (!FileIO.FileExists(FILE))
			return;

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		Flt_RankStore store = new Flt_RankStore();
		if (ctx.LoadFromFile(FILE) && ctx.ReadValue("", store))
		{
			foreach (Flt_RankEntry e : store.entries)
			{
				if (e && e.uid != "")
					m_mByUid.Set(e.uid, e.rank);
			}
			Print(string.Format("[RANK] Registre chargé : %1 grade(s)", m_mByUid.Count()), LogLevel.NORMAL);
		}
		else
		{
			Print("[RANK] Registre illisible — vide", LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	void Save()
	{
		Flt_RankStore store = new Flt_RankStore();
		foreach (string uid, int rank : m_mByUid)
		{
			Flt_RankEntry e = new Flt_RankEntry();
			e.uid  = uid;
			e.rank = rank;
			store.entries.Insert(e);
		}
		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", store);
		ctx.SaveToFile(FILE);
	}

	//------------------------------------------------------------------------------------------------
	//! \return grade assigné, ou -1 si aucun
	int GetAssignedRank(string uid)
	{
		if (uid == "")
			return -1;
		int r;
		if (m_mByUid.Find(uid, r))
			return r;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	void SetAssignedRank(string uid, int rank)
	{
		if (uid == "")
			return;
		m_mByUid.Set(uid, rank);
		Save();
		Print(string.Format("[RANK] Grade assigné : %1 -> %2", uid, rank), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	void ClearAssignedRank(string uid)
	{
		if (m_mByUid.Contains(uid))
		{
			m_mByUid.Remove(uid);
			Save();
			Print(string.Format("[RANK] Grade retiré : %1", uid), LogLevel.NORMAL);
		}
	}
}

// ---------------------------------------------------------------------------
//  Application d'un grade sur un perso (serveur). SetCharacterRank RPC -> clients.
// ---------------------------------------------------------------------------
class Flt_RankApply
{
	//------------------------------------------------------------------------------------------------
	//! Applique un grade au perso contrôlé d'un playerId.
	static void ApplyToPlayerId(int playerId, int rank)
	{
		if (rank < 0)
			return;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;
		IEntity ent = pm.GetPlayerControlledEntity(playerId);
		if (!ent)
			return;
		SCR_CharacterRankComponent comp = SCR_CharacterRankComponent.Cast(ent.FindComponent(SCR_CharacterRankComponent));
		if (!comp)
			return;

		SCR_ECharacterRank rk = rank;
		comp.SetCharacterRank(rk, false);
		// L'XP est gérée à part par Flt_XpProgression (le système XP de base est
		// inerte dans FF), donc rien à recaler ici.
	}

	//------------------------------------------------------------------------------------------------
	//! Trouve le playerId en ligne pour un UID puis applique (effet immédiat).
	//! Si le joueur est hors-ligne : rien ici, ce sera appliqué à son prochain spawn.
	static void ApplyToUID(string uid, int rank)
	{
		if (uid == "")
			return;
		PlayerManager pm = GetGame().GetPlayerManager();
		BackendApi ba = GetGame().GetBackendApi();
		if (!pm || !ba)
			return;
		array<int> ids = {};
		pm.GetPlayers(ids);
		foreach (int id : ids)
		{
			if (ba.GetPlayerIdentityId(id) == uid)
			{
				ApplyToPlayerId(id, rank);
				return;
			}
		}
	}
}

// ---------------------------------------------------------------------------
//  Flt_XpProgression — l'XP monte avec le TEMPS DE JEU mais est plafonnée au
//  haut du grade courant : la barre se remplit puis STOPPE tant que le site
//  n'a pas promu le joueur. Le plafond d'un grade = (seuil du grade suivant - 1)
//  pour que la barre soit pleine sans jamais faire passer au grade suivant.
//  Serveur uniquement. Constantes de réglage ci-dessous.
// ---------------------------------------------------------------------------
class Flt_XpProgression
{
	// --- Réglages ---
	static const bool ENABLE_PLAYTIME_XP = true;  // gain d'XP juste en jouant
	static const int  TICK_MS            = 60000; // toutes les 60 s
	static const int  XP_PER_TICK        = 10;    // XP accordé par tick

	// --- Auto-promotion Soldat ---
	// Le SEUL grade obtenu automatiquement. Au bout de AUTO_SOLDAT_SECONDS de temps de
	// jeu cumule (toutes sessions confondues), un joueur JAMAIS grade par le site/admin
	// passe Renegat(0) -> Soldat(1). Tous les grades au-dessus restent manuels
	// (portail web ou #promote / #setrank en jeu). Un joueur explicitement mis a 0 par
	// un admin n'est PAS re-promu : seul "aucune assignation" (-1) declenche l'auto.
	static const int  AUTO_SOLDAT_SECONDS = 1800;	// 30 min
	static const int  AUTO_SOLDAT_RANK    = 1;		// SCR_ECharacterRank : Soldat
	// Palier XP de Soldat = ce qu'on accumule en AUTO_SOLDAT_SECONDS (+10 XP / 60 s),
	// pour que la barre du grade 0 se remplisse exactement sur la duree de l'auto-promo.
	static const int  AUTO_SOLDAT_XP      = (AUTO_SOLDAT_SECONDS / (TICK_MS / 1000)) * XP_PER_TICK;

	// Temps de jeu cumule par UID, en secondes. Contrairement a l'XP il n'est JAMAIS
	// plafonne par le grade : c'est lui qui sert de mesure fiable de l'anciennete
	// (l'XP, elle, est bornee au palier du grade et se fige quand la barre est pleine).
	static ref map<string, int> s_mPlaySecByUid = new map<string, int>();

	// Store XP par UID. FF n'a PAS d'awarder XP de base (GameMode_Base n'a pas de
	// SCR_XPHandlerComponent) -> AwardXP ne fait rien. On gère donc l'XP nous-mêmes.
	static const string FILE = "$profile:Fleet/Xp.json";
	static ref map<string, int> s_mXpByUid = new map<string, int>();
	static bool s_bLoaded;

	//------------------------------------------------------------------------------------------------
	// NOTRE propre table de paliers (l'échelle de grades FF est creuse/faussée : la FIA n'a que
	// ~4 grades avec des seuils bizarres). Grade = SCR_ECharacterRank :
	//   0 Renegat · 1 Soldat · 2 Caporal · 3 Sergent · 4 Lieutenant · 5 Capitaine ·
	//   6 Commandant · 7 Colonel · 8 General. Ajuste librement ces seuils.
	static int GradeFloor(int grade)
	{
		if (grade < 0)
			return 0;

		// Source de verite : l'echelle du JEU (Configs/Ranks/FIAMilitaryRanks.conf pour
		// REMIXED). Le seuil d'un grade est son m_iRequiredXP. On ne duplique plus rien :
		// changer un seuil dans le .conf suffit, en jeu comme sur le site.
		SCR_RankInfo ri = Flt_RankLadder.At(grade);
		if (ri)
		{
			int req = ri.GetRequiredRankXP();
			// Les premiers grades de l'echelle FR ont des seuils NEGATIFS (Deserteur -21,
			// 1ere Classe -20) : c'est leur facon de marquer "avant tout merite". Pour nos
			// paliers on ramene ca a 0, sinon l'XP partirait dans le negatif.
			if (req < 0)
				req = 0;
			return req;
		}

		// Repli : echelle pas encore lisible (factions non construites au demarrage).
		switch (grade) {
			case 0: return 0;
			case 1: return AUTO_SOLDAT_XP;
			case 2: return 500;
			case 3: return 1200;
			case 4: return 2200;
			case 5: return 3500;
			case 6: return 5000;
			case 7: return 7000;
			case 8: return 10000;
		}
		return 10000 + (grade - 8) * 3000;
	}

	// Plafond d'un grade = juste sous le plancher du grade suivant (barre pleine puis stop).
	static int GradeCeiling(int grade)
	{
		return GradeFloor(grade + 1) - 1;
	}

	//------------------------------------------------------------------------------------------------
	// Grade assigné par le site (registre), ou 0 si aucun.
	static int EffectiveRank(int playerId)
	{
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba) return 0;
		return GradeForUid(ba.GetPlayerIdentityId(playerId));
	}

	static int GradeForUid(string uid)
	{
		int r = Flt_RankRegistry.GetInstance().GetAssignedRank(uid);
		if (r < 0) return 0;
		return r;
	}

	//------------------------------------------------------------------------------------------------
	static int GetXp(string uid)
	{
		if (uid == "") return 0;
		int xp;
		if (s_mXpByUid.Find(uid, xp)) return xp;
		return 0;
	}

	// Ajoute de l'XP, bornée dans [plancher, plafond] du grade courant. Renvoie la nouvelle valeur.
	static int AddXp(string uid, int grade, int amount)
	{
		if (uid == "") return 0;

		int cur  = GetXp(uid);
		int next = cur + amount;

		int floorXP = GradeFloor(grade);
		int ceilXP  = GradeCeiling(grade);
		if (next < floorXP) next = floorXP;
		if (next > ceilXP)  next = ceilXP;

		if (next != cur) {
			s_mXpByUid.Set(uid, next);
			Save();
		}
		return next;
	}

	//------------------------------------------------------------------------------------------------
	// Temps de jeu cumule (secondes), jamais plafonne.
	static int GetPlaytime(string uid)
	{
		if (uid == "") return 0;
		int sec;
		if (s_mPlaySecByUid.Find(uid, sec)) return sec;
		return 0;
	}

	static int AddPlaytime(string uid, int seconds)
	{
		if (uid == "") return 0;
		int next = GetPlaytime(uid) + seconds;
		s_mPlaySecByUid.Set(uid, next);
		return next;
	}

	//------------------------------------------------------------------------------------------------
	// Passe un joueur jamais grade a Soldat une fois AUTO_SOLDAT_SECONDS atteintes.
	// Ne touche a rien si le site/un admin a deja assigne un grade (meme 0).
	static void TryAutoPromote(string uid, int playSec)
	{
		if (playSec < AUTO_SOLDAT_SECONDS) return;
		if (Flt_RankRegistry.GetInstance().GetAssignedRank(uid) >= 0) return;	// deja grade : manuel uniquement

		Flt_RankRegistry.GetInstance().SetAssignedRank(uid, AUTO_SOLDAT_RANK);
		Flt_RankApply.ApplyToUID(uid, AUTO_SOLDAT_RANK);
		// L'XP est bornee au palier du grade : on la recale sur le plancher Soldat.
		AddXp(uid, AUTO_SOLDAT_RANK, 0);
		Print(string.Format("[RANK] Auto-promotion Soldat : %1 (%2 s de jeu)", uid, playSec), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	// XP de temps de jeu : +XP_PER_TICK à chaque joueur, plafonné à son grade.
	// Le compteur de temps de jeu, lui, monte sans plafond et pilote l'auto-promotion.
	static void Tick()
	{
		if (!ENABLE_PLAYTIME_XP) return;

		PlayerManager pm = GetGame().GetPlayerManager();
		BackendApi ba = GetGame().GetBackendApi();
		if (!pm || !ba) return;

		int tickSeconds = TICK_MS / 1000;

		array<int> ids = {};
		pm.GetPlayers(ids);
		foreach (int id : ids) {
			string uid = ba.GetPlayerIdentityId(id);
			if (uid == "") continue;

			int playSec = AddPlaytime(uid, tickSeconds);
			TryAutoPromote(uid, playSec);
			AddXp(uid, GradeForUid(uid), XP_PER_TICK);
		}

		Save();
	}

	//------------------------------------------------------------------------------------------------
	//  Persistance $profile:Fleet/Xp.json  ( { entries:[ {uid, xp}, ... ] } )
	static void Load()
	{
		if (s_bLoaded) return;
		s_bLoaded = true;

		s_mXpByUid.Clear();
		s_mPlaySecByUid.Clear();
		if (!FileIO.FileExists("$profile:Fleet/"))
			FileIO.MakeDirectory("$profile:Fleet/");
		if (!FileIO.FileExists(FILE)) return;

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		Flt_XpStore store = new Flt_XpStore();
		if (ctx.LoadFromFile(FILE) && ctx.ReadValue("", store)) {
			foreach (Flt_XpEntry e : store.entries) {
				if (!e || e.uid == "") continue;
				s_mXpByUid.Set(e.uid, e.xp);
				s_mPlaySecByUid.Set(e.uid, e.sec);
			}
			Print(string.Format("[XP] Registre chargé : %1 joueur(s)", s_mXpByUid.Count()));
		}
	}

	static void Save()
	{
		Flt_XpStore store = new Flt_XpStore();
		foreach (string uid, int xp : s_mXpByUid) {
			Flt_XpEntry e = new Flt_XpEntry();
			e.uid = uid;
			e.xp  = xp;
			e.sec = GetPlaytime(uid);
			store.entries.Insert(e);
		}
		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", store);
		ctx.SaveToFile(FILE);
	}
}

class Flt_XpEntry
{
	string uid;
	int    xp;
	int    sec;	// temps de jeu cumule, en secondes (absent des anciens fichiers -> 0)
}

class Flt_XpStore
{
	ref array<ref Flt_XpEntry> entries = {};
}

// ---------------------------------------------------------------------------
//  Coupe le grade automatique par XP. Le grade vient UNIQUEMENT du registre.
//  UpdatePlayerRank est le point de passage rappelé au spawn et à chaque
//  changement d'XP -> on y remplace le calcul par XP par le grade assigné.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  Ré-application du grade À CHAQUE SPAWN (clé = UID), avec recalage d'XP pour
//  que le HUD affiche le bon grade. C'est ici (spawn) et PAS dans UpdatePlayerRank
//  (appelé à chaque changement d'XP) — sinon l'XP serait gelé en jeu.
//  -> corrige "le grade ne se réapplique pas après un redémarrage / reconnexion".
// ---------------------------------------------------------------------------
modded class SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
		{
			GetOnPlayerSpawned().Insert(Flt_ReapplyRankOnSpawn);
			// XP de temps de jeu (système autonome : FF n'a pas d'awarder XP).
			Flt_XpProgression.Load();
			GetGame().GetCallqueue().CallLater(Flt_XpTick, Flt_XpProgression.TICK_MS, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Flt_XpTick()
	{
		Flt_XpProgression.Tick();
	}

	//------------------------------------------------------------------------------------------------
	protected void Flt_ReapplyRankOnSpawn(int playerId, IEntity player)
	{
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba)
			return;
		string uid = ba.GetPlayerIdentityId(playerId);
		int rank = Flt_RankRegistry.GetInstance().GetAssignedRank(uid);
		if (rank < 0)
			return;
		// Léger délai : au spawn, le RankContainer/XP handler peut ne pas être prêt pour le snap d'XP.
		GetGame().GetCallqueue().CallLater(Flt_ApplyRankDelayed, 1000, false, playerId, rank);
	}

	//------------------------------------------------------------------------------------------------
	protected void Flt_ApplyRankDelayed(int playerId, int rank)
	{
		Flt_RankApply.ApplyToPlayerId(playerId, rank);
	}

	//------------------------------------------------------------------------------------------------
	// Départ d'un joueur : distingue départ PROPRE (volontaire/kick, timeout<=0 -> pas de reconnexion
	// réservée) d'une PERTE DE CONNEXION (timeout>0 -> fenêtre de reconnexion). Pour un départ propre,
	// on prévient le site (retrait immédiat) ; pour une perte de co, le web garde les 3 min.
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);
		if (!Replication.IsServer())
			return;

		// Journal admin : on trace TOUS les departs, y compris les pertes de connexion
		// (c'est justement ce qu'on veut voir apres coup). Fait avant le retour anticipe
		// ci-dessous, qui ne concerne que le retrait immediat de la carte.
		string leaveReason = "";
		if (timeout > 0)
			leaveReason = "perte de connexion";
		Flt_EventLog.Flt_Send(Flt_EventLog.TYPE_DISCONNECT, playerId, leaveReason);

		if (timeout > 0)
			return;	// perte de connexion : on laisse le web gérer le fondu 3 min
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba)
			return;
		string uid = ba.GetPlayerIdentityId(playerId);
		if (uid != "")
			Flt_GTGPositions.GetInstance().Flt_PostLeave(uid);
	}
}

modded class SCR_PlayerXPHandlerComponent
{
	//------------------------------------------------------------------------------------------------
	override void UpdatePlayerRank(bool notify = true)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetOwner());
		if (!pc)
			return;

		int playerId = pc.GetPlayerId();
		if (playerId <= 0)
			return;

		BackendApi ba = GetGame().GetBackendApi();
		if (!ba)
			return;

		string uid = ba.GetPlayerIdentityId(playerId);
		int assigned = Flt_RankRegistry.GetInstance().GetAssignedRank(uid);

		// L'insigne vient UNIQUEMENT du grade assigné par le site (jamais de l'XP).
		if (assigned >= 0)
		{
			IEntity player = pc.GetMainEntity();
			if (player)
			{
				SCR_CharacterRankComponent comp = SCR_CharacterRankComponent.Cast(player.FindComponent(SCR_CharacterRankComponent));
				if (comp)
				{
					SCR_ECharacterRank rk = assigned;
					comp.SetCharacterRank(rk, !notify);
				}
			}
		}

	}
}
