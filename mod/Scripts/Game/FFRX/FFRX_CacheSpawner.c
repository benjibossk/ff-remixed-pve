// FF - REMIXED - PVE
// Guarded cache = a native FF "clear enemy camp" mission (reuse FF's clear-camp
// system instead of a custom spawn). We ask FF's dynamic-POI manager to spawn a
// JOB_ENEMY_CAMP -- a real enemy camp with a faction-correct garrison, streaming
// (spawns in when a player is near), its own loot and a native "camp cleared"
// objective. A note points near its grid coords. Server only.
//
// Triggered when a civilian or an enemy corpse drops the intel paper (cache note).
// We use ONLY the native camp -- no extra supply building; the camp carries its own
// loot/rewards.
//
// D7 perishable + slot cleanup: each camp is tracked by EntityID; a single repeating
// checker frees the MAX_ACTIVE slot when the camp is gone (player cleared it -> its
// entity is deleted) OR dissolves it if left untouched past CAMP_LIFETIME_MS. We track
// by EntityID (never a held component/entity ref) so a deleted camp is detected safely
// (FindEntityByID returns null) -- no dangling pointer.

class FFRX_CampRecord
{
	EntityID id;
	int      expireTick; // System.GetTickCount() (ms) at which an uncleared camp dissolves
}

class FFRX_CacheSpawner
{
	static const string CAMP_KEY          = "JOB_ENEMY_CAMP";
	static const int    MAX_ACTIVE        = 3;       // never more than 3 camps at once
	static const int    CAMP_LIFETIME_MS  = 1200000; // 20 min: an untouched camp dissolves (D7 perishable)
	static const int    CHECK_INTERVAL_MS = 30000;   // slot/expiry checker cadence

	protected static int s_iActive;
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<ref FFRX_CampRecord> s_aCamps;

	protected static array<ref FFRX_CampRecord> Camps()
	{
		if (!s_aCamps)
			s_aCamps = new array<ref FFRX_CampRecord>();

		return s_aCamps;
	}
	protected static bool s_bCheckerStarted;

	// True while another camp can still be spawned (server + under the cap). Used to
	// gate intel-paper drops so we never queue a camp beyond MAX_ACTIVE.
	static bool CanSpawn()
	{
		return Replication.IsServer() && s_iActive < MAX_ACTIVE;
	}

	// Spawn a clear-camp mission; return the note's (imprecise) grid coords. false if the
	// cap is reached or no camp slot is available.
	static bool SpawnGuardedCache(out int gridX, out int gridZ)
	{
		if (!Replication.IsServer()) return false;
		if (s_iActive >= MAX_ACTIVE) return false;

		JWK_DynamicPoiManagerComponent mgr = JWK.GetDynamicPois();
		if (!mgr) return false;

		JWK_GenericWorldSlotComponent slot = mgr.FindSlotForPoiType(CAMP_KEY);
		if (!slot) return false;

		JWK_DynamicPoiControllerComponent poi = mgr.SpawnPoi(CAMP_KEY, slot);
		if (!poi || !poi.GetOwner()) return false;

		vector pos = poi.GetOwner().GetOrigin();

		s_iActive = s_iActive + 1;

		// Track the camp for the perishability/slot checker.
		FFRX_CampRecord rec = new FFRX_CampRecord();
		rec.id         = poi.GetOwner().GetID();
		rec.expireTick = System.GetTickCount() + CAMP_LIFETIME_MS;
		Camps().Insert(rec);
		FFRX_EnsureChecker();

		// The note points NEAR the camp, not exactly on it (imprecise).
		float prec = 40.0 + JWK.Random.RandFloat01() * 320.0;
		vector noteRef = pos;
		noteRef[0] = noteRef[0] + prec * (JWK.Random.RandFloat01() * 2 - 1);
		noteRef[2] = noteRef[2] + prec * (JWK.Random.RandFloat01() * 2 - 1);
		SCR_MapEntity.GetGridPos(noteRef, gridX, gridZ);

		Print(string.Format("[FFRX][Cache] Enemy camp mission spawned near grid %1-%2 (active %3).",
			gridX, gridZ, s_iActive), LogLevel.NORMAL);
		return true;
	}

	protected static void FFRX_EnsureChecker()
	{
		if (s_bCheckerStarted) return;
		s_bCheckerStarted = true;
		GetGame().GetCallqueue().CallLater(FFRX_TickCamps, CHECK_INTERVAL_MS, true);
	}

	// Every CHECK_INTERVAL_MS: for each tracked camp, free the slot if it is gone (the
	// player cleared it -> entity deleted), or dissolve it if its lifetime elapsed.
	protected static void FFRX_TickCamps()
	{
		int now = System.GetTickCount();
		int i = 0;
		while (i < Camps().Count())
		{
			FFRX_CampRecord rec = Camps()[i];
			IEntity ent = GetGame().GetWorld().FindEntityByID(rec.id);

			if (!ent)
			{
				// Cleared / already gone -> free the slot.
				if (s_iActive > 0) s_iActive = s_iActive - 1;
				Camps().Remove(i);
				continue;
			}

			if (now >= rec.expireTick)
			{
				JWK_DynamicPoiControllerComponent poi =
					JWK_CompTU<JWK_DynamicPoiControllerComponent>.FindIn(ent);
				if (poi && !poi.IsCleared())
					poi.DespawnPoi();
				if (s_iActive > 0) s_iActive = s_iActive - 1;
				Camps().Remove(i);
				Print("[FFRX][Cache] Enemy camp dissolved (untouched, expired).", LogLevel.NORMAL);
				continue;
			}

			i = i + 1;
		}
	}
}
