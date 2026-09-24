// FF - REMIXED - PVE
// Civilian counter-espionage (D7/D9): a hostile / low-trust civilian near you SNITCHES -- reports
// your position to the enemy. Mistreating the population (extortion, frisking innocents, low trust)
// has a real cost: those civilians report you, raising your wanted heat -> enemy response.
//
// Reuses the proven MCD alert mechanism (MCD_CivilianInteractionHelper.ApplyHeat). No FF-internal
// modding. Server-only, rate-limited so it's an occasional tense event, not spam. Booted from
// FFRX_Groups.c. Complements the disguised-spy trust tie-in (high trust never betrays; low trust
// betrays here).

class FFRX_CounterEspionage
{
	protected static ref FFRX_CounterEspionage s_Instance;

	protected static const int   FFRX_TICK_MS             = 30000;
	protected static const float FFRX_SCAN_RANGE          = 40.0;
	protected static const float FFRX_REPORT_CHANCE       = 0.35;
	protected static const float FFRX_LOW_TRUST           = 35.0;    // trust below this = willing to snitch
	protected static const float FFRX_REPORT_COOLDOWN_MS  = 120000.0; // min time between snitch events

	protected float m_fNextReportTime;
	protected ref array<IEntity> m_aScan = {};

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;
		s_Instance = new FFRX_CounterEspionage();
		GetGame().GetCallqueue().CallLater(s_Instance.FFRX_Tick, FFRX_TICK_MS, true);
		Print("[FFRX][CounterEsp] Contre-espionnage civil demarre.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		if (now < m_fNextReportTime)
			return;

		BaseWorld world = GetGame().GetWorld();
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!world || !pm)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);

		foreach (int pid : ids)
		{
			IEntity pe = pm.GetPlayerControlledEntity(pid);
			if (!pe)
				continue;

			IEntity snitch = FFRX_FindSnitch(world, pe.GetOrigin());
			if (!snitch)
				continue;

			if (Math.RandomFloat01() >= FFRX_REPORT_CHANCE)
				continue;

			// This civilian reports the player -> wanted heat + feedback.
			MCD_CivilianInteractionHelper.ApplyHeat(pe, JWK_WantedHeatComponent.HEAT_CIV_EXTORTION);
			FFRX_LoadoutSystem.NotifyPlayer(pe, "Un civil hostile vient de signaler ta position a l'ennemi.");
			Print("[FFRX][CounterEsp] Un civil hostile/mefiant a signale le joueur.", LogLevel.NORMAL);

			m_fNextReportTime = now + FFRX_REPORT_COOLDOWN_MS;
			return; // one snitch per cooldown
		}
	}

	// Nearest civilian that would snitch: NEGATIVE attitude OR low trust.
	protected IEntity FFRX_FindSnitch(BaseWorld world, vector center)
	{
		m_aScan.Clear();
		vector mins = center - Vector(FFRX_SCAN_RANGE, 20, FFRX_SCAN_RANGE);
		vector maxs = center + Vector(FFRX_SCAN_RANGE, 20, FFRX_SCAN_RANGE);
		world.QueryEntitiesByAABB(mins, maxs, FFRX_CollectCiv, FFRX_FilterChar, EQueryEntitiesFlags.DYNAMIC);

		FFRX_CivIdentityRegistry reg = FFRX_CivIdentityRegistry.Get();
		foreach (IEntity ent : m_aScan)
		{
			JWK_CivilianCharacterComponent civ = JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(ent);
			if (!civ)
				continue;

			bool hostile = (civ.GetResistanceAttitude_S() == JWK_EResistanceAttitude.NEGATIVE);
			bool distrust = (reg.TrustOf(ent) < FFRX_LOW_TRUST);
			if (hostile || distrust)
				return ent;
		}
		return null;
	}

	protected bool FFRX_FilterChar(IEntity ent)
	{
		return ChimeraCharacter.Cast(ent) != null;
	}

	protected bool FFRX_CollectCiv(IEntity ent)
	{
		if (JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(ent))
			m_aScan.Insert(ent);
		return true;
	}
}
