// ============================================================================
//  Flt_Objectives — créer/assigner un OBJECTIF (tâche du menu J) depuis le site.
//
//  Utilise le système de tâches standard (SCR_TaskSystem) -> l'objectif apparaît
//  dans le panneau objectifs (touche J) du/des joueur(s) visé(s).
//  Cibles : "player" (par UID) | "squad" (par id de groupe) | "faction" (par clé).
//  Serveur uniquement (CreateTask/AssignTask sont server-only).
// ============================================================================
// Métadonnées d'un objectif Fleet (pour l'affichage web : id int + faction).
class Flt_ObjMeta
{
	int    id;
	string faction;
}

class Flt_Objectives
{
	protected static int s_iCounter;
	protected static ref map<string, ref Flt_ObjMeta> s_mMeta = new map<string, ref Flt_ObjMeta>();	// taskID -> meta
	const string BASE_TASK = "{1D0F815858EE24AD}Prefabs/Tasks/BaseTask.et";	// prefab tâche de base

	//------------------------------------------------------------------------------------------------
	static void Create(string target, string uid, int squadId, string factionKey, string name, string desc, vector pos)
	{
		if (Replication.IsClient())
			return;

		SCR_TaskSystem ts = SCR_TaskSystem.GetInstance();
		if (!ts)
			return;	// pas de système de tâches (scénario non-campagne ?)

		int objId = s_iCounter;
		// id unique (heure + compteur) pour éviter les collisions
		string tid = "flt_" + System.GetUnixTime().ToString() + "_" + s_iCounter.ToString();
		s_iCounter++;

		SCR_Task task = ts.CreateTask(BASE_TASK, tid, name, desc, pos, -1);
		if (!task)
			return;

		string fkey = factionKey;	// faction pour le web (couleur du marqueur)

		if (target == "player")
		{
			int pid = ResolvePlayer(uid);
			if (pid > 0)
			{
				ts.AssignTask(task, SCR_TaskExecutor.FromPlayerID(pid), true);
				if (fkey == "")
					fkey = FactionKeyOfPlayer(pid);
			}
		}
		else if (target == "squad")
		{
			if (squadId > 0)
			{
				ts.AssignTask(task, SCR_TaskExecutor.FromGroup(squadId), true);
				if (fkey == "")
					fkey = FactionKeyOfGroup(squadId);
			}
		}
		else if (target == "faction")
		{
			AssignFaction(ts, task, factionKey);
		}

		// mémorise pour le push web (id int + faction)
		Flt_ObjMeta meta = new Flt_ObjMeta();
		meta.id = objId;
		meta.faction = fkey;
		s_mMeta.Set(tid, meta);

		Print(string.Format("[OBJ] Objectif '%1' créé -> cible=%2 uid=%3 squad=%4 faction=%5", name, target, uid, squadId, fkey), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Construit le tableau JSON des objectifs ACTIFS (tâches vivantes qui sont les nôtres).
	//! Renvoie le CONTENU entre crochets (sans les [ ]). "" si aucun.
	static string BuildActiveJson()
	{
		SCR_TaskSystem ts = SCR_TaskSystem.GetInstance();
		if (!ts)
			return "";
		array<SCR_Task> tasks = {};
		ts.GetTasks(tasks);

		string arr = "";
		foreach (SCR_Task t : tasks)
		{
			if (!t)
				continue;
			string tid = t.GetTaskID();
			Flt_ObjMeta meta;
			if (!s_mMeta.Find(tid, meta) || !meta)
				continue;	// pas un objectif Fleet

			vector p = t.GetTaskPosition();
			string nm = "";
			string ds = "";
			SCR_TaskUIInfo info = t.GetTaskUIInfo();
			if (info)
			{
				nm = info.GetName();
				ds = info.GetDescription();
			}

			if (arr != "")
				arr += ",";
			arr += string.Format("{\"id\":%1,\"faction\":\"%2\",\"name\":\"%3\",\"desc\":\"%4\",\"x\":%5,\"z\":%6}",
				meta.id, Esc(meta.faction), Esc(nm), Esc(ds), p[0], p[2]);
		}
		return arr;
	}

	//------------------------------------------------------------------------------------------------
	//! Change l'état d'un objectif Fleet par son id web (CANCELLED = supprimé, COMPLETED = fait).
	//! La tâche quitte alors la liste active -> disparaît du web + close côté clients.
	static void SetStateById(int id, SCR_ETaskState state)
	{
		if (Replication.IsClient())
			return;
		SCR_TaskSystem ts = SCR_TaskSystem.GetInstance();
		if (!ts)
			return;
		array<SCR_Task> tasks = {};
		ts.GetTasks(tasks);
		foreach (SCR_Task t : tasks)
		{
			if (!t)
				continue;
			string tid = t.GetTaskID();
			Flt_ObjMeta meta;
			if (!s_mMeta.Find(tid, meta) || !meta || meta.id != id)
				continue;
			ts.SetTaskState(t, state);
			s_mMeta.Remove(tid);
			Print(string.Format("[OBJ] Objectif #%1 -> état %2", id, state), LogLevel.NORMAL);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static string Esc(string s)
	{
		string r = s;
		r.Replace("\\", "\\\\");
		r.Replace("\"", "\\\"");
		r.Replace("\n", " ");
		r.Replace("\r", " ");
		return r;
	}

	//------------------------------------------------------------------------------------------------
	protected static string FactionKeyOfGroup(int groupId)
	{
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm)
			return "";
		SCR_AIGroup g = gm.FindGroup(groupId);
		if (!g)
			return "";
		Faction f = g.GetFaction();
		if (!f)
			return "";
		return f.GetFactionKey();
	}

	//------------------------------------------------------------------------------------------------
	protected static string FactionKeyOfPlayer(int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return "";
		FactionAffiliationComponent fc = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
		if (!fc)
			return "";
		Faction f = fc.GetAffiliatedFaction();
		if (!f)
			return "";
		return f.GetFactionKey();
	}

	//------------------------------------------------------------------------------------------------
	protected static int ResolvePlayer(string uid)
	{
		if (uid == "")
			return 0;
		PlayerManager pm = GetGame().GetPlayerManager();
		BackendApi ba = GetGame().GetBackendApi();
		if (!pm || !ba)
			return 0;
		array<int> ids = {};
		pm.GetPlayers(ids);
		foreach (int id : ids)
		{
			if (ba.GetPlayerIdentityId(id) == uid)
				return id;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AssignFaction(SCR_TaskSystem ts, SCR_Task task, string factionKey)
	{
		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!fm || !gm)
			return;
		Faction f = fm.GetFactionByKey(factionKey);
		if (!f)
			return;
		array<SCR_AIGroup> groups = gm.GetPlayableGroupsByFaction(f);
		if (!groups)
			return;
		foreach (SCR_AIGroup g : groups)
		{
			if (g)
				ts.AssignTask(task, SCR_TaskExecutor.FromGroup(g.GetGroupID()), true);
		}
	}
}
