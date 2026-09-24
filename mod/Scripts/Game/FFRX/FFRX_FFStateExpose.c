// Exposes Reoccupation's protected collections + FF battle points, for the FF state
// emitter (admin livemap). See TASK_FFSTATE_SENDER.
// A modded class can access protected members of the class it extends.
// NOTE: ASCII only (Enforce compiler desyncs on UTF-8 accents).

modded class FF_ReoccupationManager
{
	// Scheduled counterattack queue (each entry = target + timing + recon state).
	array<ref FF_ReoccupationEntry> FFRX_GetQueue()
	{
		return m_aQueue;
	}

	// Patrol manager (GetPatrolManager is protected).
	FF_PatrolManager FFRX_GetPatrolMgr()
	{
		return GetPatrolManager();
	}
}

modded class FF_PatrolManager
{
	// Active enemy patrols (position/route/state).
	array<ref FF_PatrolTask> FFRX_GetActivePatrols()
	{
		return m_aActivePatrols;
	}
}

modded class JWK_BattleControllerEntity
{
	// Battle progress: 0-100 deployment, 100-200 domination.
	int FFRX_GetPoints()
	{
		return m_iPoints;
	}
}

modded class JWK_AIGarrisonComponent
{
	// Patrol range (protected) : rayon de patrouille de la garnison.
	int FFRX_GetPatrolRange()
	{
		return m_iPatrolRange;
	}
}
