// ============================================================================
//  Flt_RankCommand — promotion / retrogradation d'un joueur EN JEU (admin).
//
//  Rappel du design des grades (voir Flt_RankSystem.c) :
//   - Soldat (1) est le SEUL grade automatique : il tombe tout seul apres
//     Flt_XpProgression.AUTO_SOLDAT_SECONDS de temps de jeu cumule.
//   - Tout le reste est MANUEL : portail web, ou ces commandes.
//   - Le grade est stocke par UID dans $profile:Fleet/Ranks.json et re-applique
//     a chaque spawn, donc il survit a la deconnexion et au redemarrage.
//
//  Usage (chat, admin uniquement) :
//     #promote <playerId | nom>            -> grade + 1
//     #demote  <playerId | nom>            -> grade - 1
//     #setrank <playerId | nom> <0..8>     -> grade exact
//     #ranks                               -> liste les joueurs connectes + grade
//
//  Le joueur doit etre CONNECTE (on a besoin de son UID, qu'on ne peut resoudre
//  que via BackendApi sur un playerId en ligne).
//
//  Note d'implementation : le moteur enregistre TOUTE classe qui herite de
//  ScrServerCommand, donc pas de classe de base intermediaire ici (elle
//  apparaitrait comme une commande sans mot-cle). Les helpers vivent dans
//  Flt_RankCmdUtil et chaque commande herite directement de ScrServerCommand.
//
//  Grades : 0 Renegat · 1 Soldat · 2 Caporal · 3 Sergent · 4 Lieutenant ·
//           5 Capitaine · 6 Commandant · 7 Colonel · 8 General
// ============================================================================

//! Echelle de grades, LUE DANS LE JEU (SCR_RankContainer de la faction joueur).
//!
//! POURQUOI. Cette table etait ecrite en dur ici, avec 9 grades ("Renegat, Soldat,
//! Caporal...") -- alors que le scenario definit sa PROPRE echelle francaise dans
//! Configs/Ranks/FIAMilitaryRanks.conf : 15 grades, de Deserteur a Colonel, avec
//! leurs seuils d'XP. Les deux tables ne disaient pas la meme chose : "#setrank 5"
//! annoncait "Capitaine" dans le chat pendant que le jeu affichait "Sergent-Chef".
//! On lit donc la seule source qui compte, celle que le jeu utilise pour l'insigne.
class Flt_RankLadder
{
	//! Cle de faction dont on prend l'echelle. FIA = la faction joueur de FF/REMIXED.
	//! On accepte tout prefixe (FIA_DESERT sur Anizay partage la meme echelle).
	static const string FACTION_KEY_PREFIX = "FIA";

	protected static ref array<ref SCR_RankInfo> s_aRanks;
	protected static bool s_bResolved;

	//------------------------------------------------------------------------------------------------
	//! Echelle du jeu, ou null si introuvable (l'appelant retombe sur la table de secours).
	static array<ref SCR_RankInfo> Get()
	{
		if (s_bResolved)
			return s_aRanks;

		FactionManager fm = GetGame().GetFactionManager();
		if (!fm)
			return null;	// pas encore pret : on RE-essaiera au prochain appel

		array<Faction> factions = {};
		fm.GetFactionsList(factions);

		array<ref SCR_RankInfo> best;
		foreach (Faction f : factions)
		{
			SCR_Faction sf = SCR_Faction.Cast(f);
			if (!sf)
				continue;

			SCR_RankContainer cont = sf.GetRanks();
			if (!cont)
				continue;

			array<ref SCR_RankInfo> ranks = cont.GetAllRanks();
			if (!ranks || ranks.IsEmpty())
				continue;

			// Faction joueur -> c'est celle-la, on s'arrete.
			string key = sf.GetFactionKey();
			key.ToUpper();
			if (key.StartsWith(FACTION_KEY_PREFIX))
			{
				best = ranks;
				break;
			}

			// Sinon on garde la plus fournie : mieux vaut une echelle du jeu qu'une
			// table en dur, meme si ce n'est pas exactement la faction visee.
			if (!best || ranks.Count() > best.Count())
				best = ranks;
		}

		if (best)
		{
			s_aRanks = best;
			s_bResolved = true;
			Print(string.Format("[RANK] Echelle lue dans le jeu : %1 grades.", best.Count()), LogLevel.NORMAL);
		}
		return s_aRanks;
	}

	//------------------------------------------------------------------------------------------------
	//! Grade a l'index donne, null hors bornes.
	static SCR_RankInfo At(int index)
	{
		array<ref SCR_RankInfo> ranks = Get();
		if (!ranks || index < 0 || index >= ranks.Count())
			return null;
		return ranks[index];
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		array<ref SCR_RankInfo> ranks = Get();
		if (!ranks)
			return 0;
		return ranks.Count();
	}
}

class Flt_RankNames
{
	static const int MIN_RANK = 0;

	//! Dernier grade de l'echelle du jeu. Repli a 8 tant qu'elle n'est pas lisible
	//! (les factions ne sont pas encore construites au tout debut du chargement).
	static int MaxRank()
	{
		int n = Flt_RankLadder.Count();
		if (n <= 0)
			return 8;
		return n - 1;
	}

	//------------------------------------------------------------------------------------------------
	static string Get(int rank)
	{
		SCR_RankInfo ri = Flt_RankLadder.At(rank);
		if (ri && ri.GetRankName() != "")
			return ri.GetRankName();

		return string.Format("Grade #%1", rank);
	}
}

// ---------------------------------------------------------------------------
//! Table des paliers d'XP au format JSON, poussee au site avec /ranks.
//!
//! POURQUOI. Le site affichait ces seuils EN DUR dans sa page /soldier. Deux tables
//! pour une seule verite : changer un seuil en jeu rendait la page fausse sans que
//! rien ne le signale. On publie donc la table depuis le jeu, qui en est la source.
//!
//! Forme : [{"g":0,"floor":0,"ceil":299,"name":"Renegat","auto":false}, ...]
//!   floor/ceil = bornes du palier ; auto = grade obtenu SANS promotion manuelle
//!   (seul Soldat l'est, apres Flt_XpProgression.AUTO_SOLDAT_SECONDS de jeu).
//!
//! Concatenation avec '+' : string.Format tronque sa sortie a ~8 Ko.
// ---------------------------------------------------------------------------
string Flt_BuildXpLadderJson()
{
	string txt = "[";
	for (int g = Flt_RankNames.MIN_RANK; g <= Flt_RankNames.MaxRank(); g++)
	{
		if (g > Flt_RankNames.MIN_RANK)
			txt = txt + ",";

		// NB : `auto` est un mot reserve en Enforce -> isAuto.
		bool isAuto = (g == Flt_XpProgression.AUTO_SOLDAT_RANK);

		txt = txt + "{\"g\":" + g.ToString()
			+ ",\"floor\":" + Flt_XpProgression.GradeFloor(g).ToString()
			+ ",\"ceil\":" + Flt_XpProgression.GradeCeiling(g).ToString()
			+ ",\"name\":\"" + Flt_RankNames.Get(g) + "\""
			+ ",\"auto\":" + isAuto.ToString()
			+ "}";
	}
	return txt + "]";
}

// ---------------------------------------------------------------------------
//  Helpers partages par les commandes de grade.
// ---------------------------------------------------------------------------
class Flt_RankCmdUtil
{
	//------------------------------------------------------------------------------------------------
	//! Recolle argv[from..to] (inclus) en une seule chaine : les pseudos ont des espaces.
	static string JoinArgs(array<string> argv, int from, int to)
	{
		string joined = "";
		for (int i = from; i <= to && i < argv.Count(); i++)
		{
			if (joined != "")
				joined = joined + " ";
			joined = joined + argv[i];
		}
		return joined;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsNumeric(string s)
	{
		if (s == "")
			return false;
		int n = s.Length();
		for (int i = 0; i < n; i++)
		{
			string c = s.Get(i);
			if (c < "0" || c > "9")
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static string UidOf(int playerId)
	{
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba)
			return "";
		return ba.GetPlayerIdentityId(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Grade courant d'un joueur : assignation si elle existe, sinon 0 (Renegat).
	static int CurrentRank(string uid)
	{
		int r = Flt_RankRegistry.GetInstance().GetAssignedRank(uid);
		if (r < 0)
			return 0;
		return r;
	}

	//------------------------------------------------------------------------------------------------
	//! Resout un playerId a partir d'un id numerique ou d'un bout de pseudo (insensible a la casse).
	//! \return playerId en ligne, ou 0 si introuvable / ambigu (error renseigne).
	static int ResolveTarget(string token, out string error)
	{
		error = "";
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
		{
			error = "PlayerManager indisponible.";
			return 0;
		}

		array<int> ids = {};
		pm.GetPlayers(ids);

		if (IsNumeric(token))
		{
			int id = token.ToInt();
			if (ids.Contains(id))
				return id;
			error = string.Format("Aucun joueur connecte avec l'id %1.", id);
			return 0;
		}

		string needle = token;
		needle.ToLower();

		int found = 0;
		int matches = 0;
		foreach (int candidate : ids)
		{
			string lower = pm.GetPlayerName(candidate);
			lower.ToLower();
			if (!lower.Contains(needle))
				continue;
			matches++;
			found = candidate;
		}

		if (matches == 1)
			return found;

		if (matches == 0)
			error = string.Format("Aucun joueur connecte ne correspond a '%1'.", token);
		else
			error = string.Format("'%1' correspond a %2 joueurs : utilise le playerId (voir #ranks).", token, matches);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Ecrit le grade, l'applique tout de suite au perso et renvoie le compte-rendu.
	static ScrServerCmdResult ApplyRank(int targetId, string uid, int oldRank, int newRank)
	{
		if (newRank < Flt_RankNames.MIN_RANK || newRank > Flt_RankNames.MaxRank())
			return ScrServerCmdResult(
				string.Format("Grade hors bornes (%1..%2).", Flt_RankNames.MIN_RANK, Flt_RankNames.MaxRank()),
				EServerCmdResultType.ERR);

		Flt_RankRegistry.GetInstance().SetAssignedRank(uid, newRank);
		Flt_RankApply.ApplyToPlayerId(targetId, newRank);

		// Recale l'XP dans le palier du nouveau grade pour que la barre du HUD suive.
		Flt_XpProgression.Load();
		Flt_XpProgression.AddXp(uid, newRank, 0);

		string name = GetGame().GetPlayerManager().GetPlayerName(targetId);
		string msg = string.Format("%1 : %2 -> %3.", name, Flt_RankNames.Get(oldRank), Flt_RankNames.Get(newRank));
		Print(string.Format("[RANK] %1 (uid %2)", msg, uid), LogLevel.NORMAL);
		return ScrServerCmdResult(msg, EServerCmdResultType.OK);
	}

	//------------------------------------------------------------------------------------------------
	//! Chemin commun de #promote / #demote : tout ce qui suit le mot-cle = la cible.
	static ScrServerCmdResult ShiftRank(array<string> argv, int delta, string usage)
	{
		if (argv.Count() < 2)
			return ScrServerCmdResult(usage, EServerCmdResultType.PARAMETERS);

		string error;
		int targetId = ResolveTarget(JoinArgs(argv, 1, argv.Count() - 1), error);
		if (targetId <= 0)
			return ScrServerCmdResult(error, EServerCmdResultType.ERR);

		string uid = UidOf(targetId);
		if (uid == "")
			return ScrServerCmdResult("UID introuvable pour ce joueur, reessaye.", EServerCmdResultType.ERR);

		int oldRank = CurrentRank(uid);
		int newRank = oldRank + delta;
		string name = GetGame().GetPlayerManager().GetPlayerName(targetId);

		if (newRank < Flt_RankNames.MIN_RANK)
			return ScrServerCmdResult(
				string.Format("%1 est deja au grade le plus bas (%2).", name, Flt_RankNames.Get(oldRank)),
				EServerCmdResultType.ERR);

		if (newRank > Flt_RankNames.MaxRank())
			return ScrServerCmdResult(
				string.Format("%1 est deja au grade le plus haut (%2).", name, Flt_RankNames.Get(oldRank)),
				EServerCmdResultType.ERR);

		return ApplyRank(targetId, uid, oldRank, newRank);
	}
}

// ---------------------------------------------------------------------------
//  #promote <playerId | nom>
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class Flt_PromoteCommand : ScrServerCommand
{
	static const string USAGE = "Usage : #promote <playerId | nom>";

	override string GetKeyword() { return "promote"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Flt_RankCmdUtil.ShiftRank(argv, 1, USAGE); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return Flt_RankCmdUtil.ShiftRank(argv, 1, USAGE); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}

// ---------------------------------------------------------------------------
//  #demote <playerId | nom>
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class Flt_DemoteCommand : ScrServerCommand
{
	static const string USAGE = "Usage : #demote <playerId | nom>";

	override string GetKeyword() { return "demote"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Flt_RankCmdUtil.ShiftRank(argv, -1, USAGE); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return Flt_RankCmdUtil.ShiftRank(argv, -1, USAGE); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}

// ---------------------------------------------------------------------------
//  #setrank <playerId | nom> <0..8>
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class Flt_SetRankCommand : ScrServerCommand
{
	static const string USAGE = "Usage : #setrank <playerId | nom> <0..8> (0 Renegat, 1 Soldat, 2 Caporal, 3 Sergent, 4 Lieutenant, 5 Capitaine, 6 Commandant, 7 Colonel, 8 General)";

	override string GetKeyword() { return "setrank"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Handle(argv); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return Handle(argv); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Handle(array<string> argv)
	{
		// Le grade est le DERNIER argument ; tout ce qui precede est la cible (pseudo a espaces).
		if (argv.Count() < 3)
			return ScrServerCmdResult(USAGE, EServerCmdResultType.PARAMETERS);

		int last = argv.Count() - 1;
		string rankToken = argv[last];
		if (!Flt_RankCmdUtil.IsNumeric(rankToken))
			return ScrServerCmdResult(USAGE, EServerCmdResultType.PARAMETERS);

		string error;
		int targetId = Flt_RankCmdUtil.ResolveTarget(Flt_RankCmdUtil.JoinArgs(argv, 1, last - 1), error);
		if (targetId <= 0)
			return ScrServerCmdResult(error, EServerCmdResultType.ERR);

		string uid = Flt_RankCmdUtil.UidOf(targetId);
		if (uid == "")
			return ScrServerCmdResult("UID introuvable pour ce joueur, reessaye.", EServerCmdResultType.ERR);

		return Flt_RankCmdUtil.ApplyRank(targetId, uid, Flt_RankCmdUtil.CurrentRank(uid), rankToken.ToInt());
	}
}

// ---------------------------------------------------------------------------
//  #ranks — liste les joueurs connectes (playerId, pseudo, grade, temps de jeu).
//  Sert surtout a recuperer le playerId quand deux pseudos se ressemblent.
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class Flt_RanksListCommand : ScrServerCommand
{
	override string GetKeyword() { return "ranks"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Handle(); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return Handle(); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Handle()
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return ScrServerCmdResult("PlayerManager indisponible.", EServerCmdResultType.ERR);

		Flt_XpProgression.Load();

		array<int> ids = {};
		pm.GetPlayers(ids);
		if (ids.IsEmpty())
			return ScrServerCmdResult("Aucun joueur connecte.", EServerCmdResultType.OK);

		// Concatenation avec '+' : string.Format tronque sa sortie a ~8 Ko.
		string msg = "";
		foreach (int id : ids)
		{
			string uid = Flt_RankCmdUtil.UidOf(id);
			int minutes = Flt_XpProgression.GetPlaytime(uid) / 60;
			if (msg != "")
				msg = msg + " | ";
			msg = msg + string.Format("%1 %2 (%3, %4 min)",
				id, pm.GetPlayerName(id), Flt_RankNames.Get(Flt_RankCmdUtil.CurrentRank(uid)), minutes);
		}

		return ScrServerCmdResult(msg, EServerCmdResultType.OK);
	}
}
