// FF - REMIXED - PVE
// FF-awareness for DARC missions, re-implemented from DarcMissionsCompatFF -- which we
// CANNOT depend on: it uses EPF classes but declares FF (CAFEBEE) without EPF, poisoning
// FF's EPF_CharacterSaveData static (see mem ff-epf-dependency-ordering). We mod the same
// DARC base classes ourselves; FF-REMIXED depends on DarcCore + DarcMissions and sees FF
// via Reoccupation, so this compiles clean. Config values are hardcoded (were in the
// compat's dc_compatFFConfig.json).

// Mission-placement error reasons (mirrors the compat's own additions).
modded enum SDRC_EMissionError
{
	FFRX_IN_SAFEZONE,
	FFRX_HIDEOUT_TOO_CLOSE
}

// Only let DARC missions spawn in ENEMY territory, away from resistance hideouts.
modded class SDRC_MissionPosHelper
{
	static const float FFRX_GREEN_ZONE_SPAWN_RATE = 0.35;  // small chance a player/support zone is still allowed
	static const float FFRX_HIDEOUT_SAFE_DISTANCE = 300.0; // keep missions this far from hideouts

	override static SDRC_EMissionError IsValidMissionPos(vector pos, bool onlyBasicChecks = false, bool isRequested = false, float distanceToMission = -1, float distanceToPlayer = -1)
	{
		SDRC_EMissionError missionError = super.IsValidMissionPos(pos, onlyBasicChecks, isRequested, distanceToMission, distanceToPlayer);
		if (missionError != SDRC_EMissionError.NONE)
			return missionError;

		// Reject player/supporting (green) zones, except a small random fraction.
		JWK_EFactionRole factionRole = JWK.GetTerritoryControl().GetControllingRoleAt(pos);
		if (Math.RandomFloat(0, 1) > FFRX_GREEN_ZONE_SPAWN_RATE)
		{
			if (factionRole == JWK_EFactionRole.PLAYER || factionRole == JWK_EFactionRole.SUPPORTING)
				return SDRC_EMissionError.FFRX_IN_SAFEZONE;
		}

		// Keep missions away from resistance hideouts (-1 = game not started yet).
		float distance = JWK_IndexSystem.Get().FindDistanceToNearestXZ(JWK_ResistanceHideoutEntity, pos);
		if (distance != -1 && distance < FFRX_HIDEOUT_SAFE_DISTANCE)
			return SDRC_EMissionError.FFRX_HIDEOUT_TOO_CLOSE;

		return missionError;
	}
}

// Make DARC use FF's ENEMY faction for its mission enemies (was the compat's
// SetEnemyFactionAutomatically). Runs on a timer until the game mode is out of PREGAME.
modded class SDRC_Compat
{
	override static bool Init()
	{
		bool ok = super.Init();
		GetGame().GetCallqueue().CallLater(FFRX_SetEnemyFaction, 5000, true);
		return ok;
	}

	static void FFRX_SetEnemyFaction()
	{
		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gm || !gm.missionFrame || !gm.missionFrame.m_Config)
			return;
		if (gm.GetState() == SCR_EGameModeState.PREGAME)
			return;

		JWK_FactionManager fm = JWK_FactionManager.Cast(GetGame().GetFactionManager());
		if (!fm)
			return;
		JWK_Faction enemy = JWK_Faction.GetByRole(JWK_EFactionRole.ENEMY);
		if (!enemy)
			return;
		string key = enemy.GetKey();

		// THE important one: DARC's SDRC_EnemyHelper defaults its enemy pool to ALL game
		// factions (US, FIA, CIV, USSR...) -> missions spawn US/FIA/CIV enemies + mismatched
		// vehicles. Force it to ONLY the FF enemy faction (USSR).
		array<string> only = {};
		only.Insert(key);
		SDRC_EnemyHelper.SetEnemyFactions(only);

		// Keep missionFrame's own config list in sync too.
		array<string> ef = gm.missionFrame.m_Config.enemyFactions;
		bool needChange = ef.Count() != 1 || ef[0] != key;
		if (needChange)
		{
			ef.Clear();
			ef.Insert(key);
			Print("[FFRX][Darc] DARC enemy faction forced to FF enemy: " + key, LogLevel.NORMAL);
		}
	}
}

// Keep DARC mission VEHICLES from vanishing: FF streams out vehicles far from players
// (and EPF can delete them) -> a tracked convoy/crashsite would disappear. Enable EPF
// persistence + disable FF streaming on them, exactly like the compat did.
modded class SDRC_SpawnHelper
{
	override static void SetPersistence(IEntity entity, bool persistence = true)
	{
		// Keep DARC mission vehicles from vanishing: disable FF's vehicle STREAMING (FF
		// despawns vehicles far from players). We deliberately do NOT touch
		// EPF_PersistenceComponent here -- referencing an EPF class from this module
		// re-triggers FF's EPF_CharacterSaveData static poison in the strict/clean build
		// (see mem ff-epf-dependency-ordering). Disabling streaming is the part that
		// actually stops the despawn, so we lose nothing important.
		if (SDRC_VehicleHelper.IsVehicle(entity))
		{
			JWK_StreamableVehicleComponent streamable = JWK_CompTU<JWK_StreamableVehicleComponent>.FindIn(entity);
			if (streamable)
				streamable.SetStreamingEnabled_S(false);
		}

		super.SetPersistence(entity, persistence);
	}
}
