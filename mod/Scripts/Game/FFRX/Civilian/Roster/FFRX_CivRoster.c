// FF - REMIXED - PVE
// Persistent civilian identities -- APPROACH A1 (robust, no FF-internal modding).
//
// Instead of overriding FF's civilian spawn (fragile -- broke the build), we OBSERVE: a server
// system scans civilians near players and BINDS a stable identity (name + memory) to each one,
// anchored to where we first see it. FF keeps full control of spawning/AI/despawn/appearance.
//
// This gives the RPG core: you recognize "Ivan Kovac" by NAME + your history with him. The FACE
// stays FF's (fixing it required the fragile spawn override) -- can be revisited later once the
// installed FF's placement API is verified.
//
// PHASE 1 here: identity binding. PHASE 2 (later): persistent TRUST per identity (JSON) wired to
// the MCD dialogue + the frisk. Server-only. Booted from FFRX_Groups.c.

class FFRX_CivRoster
{
	protected static ref FFRX_CivRoster s_Instance;

	protected static const int   FFRX_TICK_MS    = 5000;
	protected static const float FFRX_SCAN_RANGE = 60.0;  // bind civilians within this of a player

	protected ref array<IEntity> m_aScan = {};

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;
		s_Instance = new FFRX_CivRoster();
		GetGame().GetCallqueue().CallLater(s_Instance.FFRX_Tick, FFRX_TICK_MS, true);
		GetGame().GetCallqueue().CallLater(s_Instance.FFRX_SaveTick, 60000, true); // persist trust every 60s
		Print("[FFRX][Civ] Roster identites civiles demarre.", LogLevel.NORMAL);
	}

	protected void FFRX_SaveTick()
	{
		FFRX_CivIdentityRegistry.Get().FFRX_Save();
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_Tick()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);

		foreach (int pid : ids)
		{
			IEntity pe = pm.GetPlayerControlledEntity(pid);
			if (!pe)
				continue;

			vector p = pe.GetOrigin();
			m_aScan.Clear();
			vector mins = p - Vector(FFRX_SCAN_RANGE, 30, FFRX_SCAN_RANGE);
			vector maxs = p + Vector(FFRX_SCAN_RANGE, 30, FFRX_SCAN_RANGE);
			world.QueryEntitiesByAABB(mins, maxs, FFRX_CollectCiv, FFRX_FilterChar, EQueryEntitiesFlags.DYNAMIC);

			FFRX_CivIdentityRegistry reg = FFRX_CivIdentityRegistry.Get();
			// Libere d'abord les habitants dont le civil a disparu : sans ce menage, une zone
			// epuiserait ses habitants libres et en fabriquerait de nouveaux sans fin.
			reg.PruneDead();

			foreach (IEntity civ : m_aScan)
			{
				if (reg.IdentityOf(civ))
					continue; // already has an identity this session
				FFRX_CivIdentity id = reg.GetForZone(civ.GetOrigin());
				reg.Bind(civ.GetID(), id);
				reg.NoteSeenAt(id, civ.GetOrigin());

				// Remembered relationship shows on sight: a civilian you befriended greets you
				// warmly; one you wronged is hostile. Applied ONCE (only on first bind).
				JWK_CivilianCharacterComponent cc = JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(civ);
				if (cc)
				{
					if (id.m_fTrust >= 65.0)
						cc.SetResistanceAttitude_S(JWK_EResistanceAttitude.POSITIVE);
					else if (id.m_fTrust <= 30.0)
						cc.SetResistanceAttitude_S(JWK_EResistanceAttitude.NEGATIVE);
				}
			}
		}
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

//----------------------------------------------------------------------------------------------------
// Dev command "#civname": print the identity (name) bound to the nearest civilian. Validates Phase 1.
[BaseContainerProps()]
class FFRX_CivNameCommand : ScrServerCommand
{
	override string GetKeyword() { return "civname"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		IEntity best = null;
		float bestSq = 30.0 * 30.0;
		vector p = ent.GetOrigin();
		array<IEntity> found = {};
		FFRX_CivQueryHelper.NearbyCivs(GetGame().GetWorld(), p, 30.0, found);
		foreach (IEntity c : found)
		{
			float dSq = vector.DistanceSqXZ(c.GetOrigin(), p);
			if (dSq < bestSq) { bestSq = dSq; best = c; }
		}
		if (!best)
			return ScrServerCmdResult("Aucun civil a proximite.", EServerCmdResultType.OK);

		FFRX_CivIdentity id = FFRX_CivIdentityRegistry.Get().IdentityOfOrCreate(best);
		return ScrServerCmdResult(string.Format("Civil le plus proche : %1 | confiance %2/100 (%3 interactions, cell %4).",
			id.m_sName, (int)id.m_fTrust, id.m_iInteractions, id.m_sKey), EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}

//----------------------------------------------------------------------------------------------------
// Dev command "#civtrust <delta>": change the nearest civilian's trust (default +15). Tests persistence.
[BaseContainerProps()]
class FFRX_CivTrustCommand : ScrServerCommand
{
	override string GetKeyword() { return "civtrust"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		float delta = 15.0;
		if (argv && argv.Count() > 0)
			delta = argv[0].ToFloat();

		vector p = ent.GetOrigin();
		array<IEntity> found = {};
		FFRX_CivQueryHelper.NearbyCivs(GetGame().GetWorld(), p, 30.0, found);
		IEntity best = null;
		float bestSq = 30.0 * 30.0;
		foreach (IEntity c : found)
		{
			float dSq = vector.DistanceSqXZ(c.GetOrigin(), p);
			if (dSq < bestSq) { bestSq = dSq; best = c; }
		}
		if (!best)
			return ScrServerCmdResult("Aucun civil a proximite.", EServerCmdResultType.OK);

		FFRX_CivIdentityRegistry reg = FFRX_CivIdentityRegistry.Get();
		FFRX_CivIdentity id = reg.IdentityOfOrCreate(best);
		reg.AddTrust(id, delta);
		return ScrServerCmdResult(string.Format("%1 : confiance -> %2/100 (delta %3).", id.m_sName, (int)id.m_fTrust, (int)delta), EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}

//----------------------------------------------------------------------------------------------------
class FFRX_CivQueryHelper
{
	protected static ref array<IEntity> s_Found;

	static void NearbyCivs(BaseWorld world, vector center, float range, notnull array<IEntity> outCivs)
	{
		if (!world)
			return;
		s_Found = outCivs;
		vector mins = center - Vector(range, 30, range);
		vector maxs = center + Vector(range, 30, range);
		world.QueryEntitiesByAABB(mins, maxs, FFRX_Collect, FFRX_Filter, EQueryEntitiesFlags.DYNAMIC);
	}

	protected static bool FFRX_Filter(IEntity ent)
	{
		return ChimeraCharacter.Cast(ent) != null;
	}

	protected static bool FFRX_Collect(IEntity ent)
	{
		if (s_Found && JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(ent))
			s_Found.Insert(ent);
		return true;
	}
}
