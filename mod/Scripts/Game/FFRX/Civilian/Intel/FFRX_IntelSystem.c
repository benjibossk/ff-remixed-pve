// FF - More Civilian Discussion
// Intel system (Phase 2 -- "conflit inconnu au depart"). TEXT-ONLY: intel is given as
// GRID COORDINATES the player must read and remember -- NO map markers/circles, so the
// blank map stays blank and recon actually matters.
//
// Two sources:
//   - Civilian "ask enemy presence" (MCD_ConvoAskEnemyPresenceCivilianNode): a sharing
//     civilian occasionally tips the nearest enemy checkpoint's grid coords, shown as a
//     hint (the player notes the numbers). -> TipNearestCheckpoint.
//   - Cache-note item (base-game SCR_CacheNoteComponent, adapted in FFRX_CacheNote):
//     its description shows the grid coords of a random enemy location. -> RandomEnemyGrid.
//
// ASCII only in strings.

class FFRX_IntelSystem
{
	// Civilian tip tuning. Kept low + one-shot per civilian (MCD_PresenceRegistry) so
	// intel is EARNED, not spammed.
	static const float TIP_CHANCE   = 0.3;    // chance a shared answer also drops a tip
	static const float TIP_MAX_DIST = 3000.0; // civilians only know checkpoints this near
	static const float HVT_TIP_CHANCE = 0.15; // chance a tip is instead a lead on an enemy officer (KillHVT job)
	static const float MINE_TIP_CHANCE = 0.25; // chance a shared tip warns of a nearby enemy MINEFIELD (map marker)

	// D7 -- source reliability. Civilian RUMOR is imprecise and sometimes flat WRONG
	// (a false lead: a plausible but empty grid = a wasted trip). Precision = coord
	// noise radius (m); false chance = probability the whole tip is bogus. (The found
	// cache DOCUMENT is more reliable -- it points at a real spawned camp, no false lead.)
	static const float CIV_FALSE_CHANCE = 0.20;  // ~1 in 5 civilian tips is a false lead
	static const float CIV_PRECISION    = 150.0; // civilian tips are +/- ~150 m

	// Officer/HVT documents = the RELIABLE source (D7): precise, never a false lead.
	static const float OFF_FALSE_CHANCE = 0.0;
	static const float OFF_PRECISION    = 25.0;

	// Civilian tip: the nearest ENEMY checkpoint's grid coords + direction, as a hint.
	// Returns true if one was tipped. Server only.
	static bool TipNearestCheckpoint(int playerId, vector fromPos, float maxDist)
	{
		World world = GetGame().GetWorld();
		if (!world) return false;

		array<EntityID> cps = JWK_IndexSystem.Get(world).GetAll(JWK_CheckpointEntity);
		if (!cps) return false;

		JWK_CheckpointEntity best;
		float bestD = maxDist;
		foreach (EntityID id : cps) {
			JWK_CheckpointEntity cp = JWK_CheckpointEntity.Cast(world.FindEntityByID(id));
			if (!cp || !cp.IsSpawnedIn()) continue;
			float d = vector.Distance(fromPos, cp.GetOrigin());
			if (d < bestD) {
				bestD = d;
				best = cp;
			}
		}

		if (!best) return false;

		SendGridTip(playerId, best.GetOrigin(), "Checkpoint ennemi",
			CIV_FALSE_CHANCE, CIV_PRECISION);
		return true;
	}

	// Reveal a RANDOM active DARC mission's grid coords as a text tip. DARC missions
	// spawn randomly but their map icons are hidden (FFRX_DarcIntegration) -- intel is
	// how the player learns where they are. Returns true if one was revealed. Server only.
	static bool TipRandomMission(int playerId)
	{
		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (!gm || !gm.missionFrame)
			return false;

		array<ref SDRC_Mission> list = gm.missionFrame.m_MissionList;
		if (!list || list.IsEmpty())
			return false;

		array<SDRC_Mission> active = {};
		foreach (SDRC_Mission m : list) {
			if (m && m.IsActive())
				active.Insert(m);
		}
		if (active.IsEmpty())
			return false;

		SDRC_Mission pick = active[Math.RandomInt(0, active.Count())];
		SendGridTip(playerId, pick.GetPos(),
			"Activite ennemie signalee (" + pick.GetTitle() + ")",
			CIV_FALSE_CHANCE, CIV_PRECISION);
		return true;
	}

	// Reliable "officer documents" tip (precise, no false lead -- the D7 reliable source):
	// reveal an active DARC mission, else the nearest enemy checkpoint. Server only.
	// The officer's PAPERS, as text to be written on a physical document found on
	// his body (FFRX_EnemyIntelDrop). Returns "" if there is nothing to point at.
	// Same target selection as TipReliable, but nothing is pushed to a screen.
	static string BuildReliableDocText(vector fromPos)
	{
		// Prefer an active DARC mission.
		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gm && gm.missionFrame && gm.missionFrame.m_MissionList)
		{
			array<SDRC_Mission> active = {};
			foreach (SDRC_Mission m : gm.missionFrame.m_MissionList)
			{
				if (m && m.IsActive())
					active.Insert(m);
			}
			if (!active.IsEmpty())
			{
				SDRC_Mission pick = active[Math.RandomInt(0, active.Count())];
				return BuildGridTipText(pick.GetPos(), pick.GetTitle(),
					OFF_FALSE_CHANCE, OFF_PRECISION);
			}
		}

		// Fallback: nearest enemy checkpoint (officers know farther than civilians).
		World world = GetGame().GetWorld();
		if (!world) return "";
		array<EntityID> cps = JWK_IndexSystem.Get(world).GetAll(JWK_CheckpointEntity);
		if (!cps) return "";

		JWK_CheckpointEntity best;
		float bestD = TIP_MAX_DIST * 3;
		foreach (EntityID id : cps)
		{
			JWK_CheckpointEntity cp = JWK_CheckpointEntity.Cast(world.FindEntityByID(id));
			if (!cp || !cp.IsSpawnedIn()) continue;
			float d = vector.Distance(fromPos, cp.GetOrigin());
			if (d < bestD) { bestD = d; best = cp; }
		}
		if (!best) return "";

		return BuildGridTipText(best.GetOrigin(), "Checkpoint ennemi",
			OFF_FALSE_CHANCE, OFF_PRECISION);
	}

	// LEGACY spoken variant, kept for any caller that still wants a hint. The officer
	// path now drops a document instead (see FFRX_EnemyIntelDrop).
	static bool TipReliable(int playerId, vector fromPos)
	{
		// Prefer an active DARC mission.
		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gm && gm.missionFrame && gm.missionFrame.m_MissionList)
		{
			array<SDRC_Mission> active = {};
			foreach (SDRC_Mission m : gm.missionFrame.m_MissionList)
			{
				if (m && m.IsActive())
					active.Insert(m);
			}
			if (!active.IsEmpty())
			{
				SDRC_Mission pick = active[Math.RandomInt(0, active.Count())];
				SendGridTip(playerId, pick.GetPos(),
					"Documents sur l'officier (" + pick.GetTitle() + ")",
					OFF_FALSE_CHANCE, OFF_PRECISION);
				return true;
			}
		}

		// Fallback: nearest enemy checkpoint (officers know farther than civilians).
		World world = GetGame().GetWorld();
		if (!world) return false;
		array<EntityID> cps = JWK_IndexSystem.Get(world).GetAll(JWK_CheckpointEntity);
		if (!cps) return false;

		JWK_CheckpointEntity best;
		float bestD = TIP_MAX_DIST * 3;
		foreach (EntityID id : cps)
		{
			JWK_CheckpointEntity cp = JWK_CheckpointEntity.Cast(world.FindEntityByID(id));
			if (!cp || !cp.IsSpawnedIn()) continue;
			float d = vector.Distance(fromPos, cp.GetOrigin());
			if (d < bestD) { bestD = d; best = cp; }
		}
		if (!best) return false;

		SendGridTip(playerId, best.GetOrigin(), "Documents sur l'officier (checkpoint ennemi)",
			OFF_FALSE_CHANCE, OFF_PRECISION);
		return true;
	}

	// A civilian tip that's actually a lead on an enemy OFFICER -> launch FF's native
	// KillEnemyHvt job (spawns an HVT at a military base for the player to hunt). FF's
	// own job UI notifies the player + adds the journal entry. Server only.
	static bool LaunchKillHvt(IEntity playerEntity)
	{
		JWK_JobManagerComponent jobs = JWK.GetJobs();
		if (!jobs)
			return false;

		JWK_JobGeneratorContext ctx = new JWK_JobGeneratorContext();
		ctx.m_PlayerEntity = playerEntity;
		JWK_QuestEntity quest = jobs.GenerateJob_S(ctx, "JOB_KILL_ENEMY_HVT");
		return quest != null;
	}

	// Grid coords of a RANDOM enemy-controlled location, with random precision (offset
	// so it's "more or less accurate" -- D7 unreliability). Used by the FF cache note.
	// Returns false if no enemy location is known. Server only.
	static bool RandomEnemyGrid(out int gridX, out int gridZ)
	{
		World world = GetGame().GetWorld();
		if (!world) return false;

		array<GenericComponent> fcs = JWK_IndexSystem.Get(world).GetAllGC(JWK_FactionControlComponent);
		if (!fcs) return false;

		array<vector> enemy = {};
		foreach (GenericComponent gc : fcs) {
			JWK_FactionControlComponent fc = JWK_FactionControlComponent.Cast(gc);
			if (!fc || !fc.GetOwner() || !fc.Affiliation()) continue;
			if (fc.GetFactionRole() != JWK_EFactionRole.ENEMY) continue;
			enemy.Insert(fc.GetOwner().GetOrigin());
		}
		if (enemy.IsEmpty()) return false;

		vector tp = enemy[Math.RandomInt(0, enemy.Count())];

		// Random precision: the document may be spot-on or off by a few hundred metres.
		float prec = 40.0 + JWK.Random.RandFloat01() * 320.0; // ~40..360 m error
		vector noisy = tp;
		noisy[0] = noisy[0] + prec * (JWK.Random.RandFloat01() * 2 - 1);
		noisy[2] = noisy[2] + prec * (JWK.Random.RandFloat01() * 2 - 1);

		SCR_MapEntity.GetGridPos(noisy, gridX, gridZ);
		return true;
	}

	// Send a grid tip for truePos with a reliability: chance it's a FALSE lead (a random
	// plausible grid -> wasted trip), otherwise the true pos with precision noise. Server only.
	// Quality multiplier applied to BOTH the false-lead chance and the coordinate
	// noise, driven by how much of the radio-tower network we hold.
	//
	// Fiction: our operators cross-check what a civilian says against intercepted
	// enemy traffic. With no network, a rumour stays a rumour; with full coverage,
	// they can confirm it and pin it down.
	//
	// Reoccupation exposes the level (0-6) at 0/25/40/55/70/85% coverage. Each
	// level cuts noise and false leads by 12%: at full coverage a civilian tip
	// goes from +/-150 m and 1-in-5 wrong, to ~+/-42 m and ~1-in-18 wrong.
	// Returns 1.0 (unchanged behaviour) if Reoccupation is absent.
	static float NetworkQualityFactor()
	{
		FF_ReoccupationManager mgr = FF_ReoccupationManager.GetExisting();
		if (!mgr)
			return 1.0;

		int level = Math.ClampInt(mgr.GetCurrentIntelNetworkLevel(), 0, 6);
		return 1.0 - 0.12 * level;
	}

	// Builds the grid-coordinate line (with false-lead roll + noise applied). Shared
	// by the spoken tips (SendGridTip) and the written documents (BuildReliableDocText),
	// so a rumour and a captured paper are rolled by the exact same rules.
	static string BuildGridTipText(vector truePos, string label, float falseChance, float precision)
	{
		float quality = NetworkQualityFactor();
		falseChance = falseChance * quality;
		precision = precision * quality;

		vector refPos = truePos;
		if (JWK.Random.RandFloat01() < falseChance)
			refPos = FFRX_RandomMapPoint(truePos);

		refPos[0] = refPos[0] + precision * (JWK.Random.RandFloat01() * 2 - 1);
		refPos[2] = refPos[2] + precision * (JWK.Random.RandFloat01() * 2 - 1);

		int gx, gz;
		SCR_MapEntity.GetGridPos(refPos, gx, gz);
		return string.Format("%1 : grille %2 - %3.", label, gx, gz);
	}

	protected static void SendGridTip(int playerId, vector truePos, string label, float falseChance, float precision)
	{
		// A SPOKEN tip: this is what a civilian says to your face during the
		// conversation, so a hint is the right rendering -- it is not a document.
		// Written intel goes through BuildGridTipText + a physical item instead.
		SendHint(playerId, BuildGridTipText(truePos, label, falseChance, precision));
	}

	// A random point somewhere on the map (for false leads). Falls back to the true pos
	// if the world entity isn't available.
	protected static vector FFRX_RandomMapPoint(vector fallback)
	{
		JWK_WorldEntity we = JWK_WorldEntity.Get();
		if (!we)
			return fallback;

		vector off  = we.GetMapOffset();
		vector size = we.GetMapSize();
		vector p = fallback;
		p[0] = off[0] + JWK.Random.RandFloat01() * size[0];
		p[2] = off[2] + JWK.Random.RandFloat01() * size[2];
		return p;
	}

	// Server -> owner client hint (FF feedback style).
	protected static void SendHint(int playerId, string text)
	{
		JWK_PlayerControllerComponent jpc = JWK.GetPlayerController(playerId);
		if (jpc) jpc.FFRX_SendIntelHint(text);
	}

	// Server -> owner client: drop a local MAP MARKER (minefield danger zone).
	protected static void SendMarker(int playerId, vector pos)
	{
		JWK_PlayerControllerComponent jpc = JWK.GetPlayerController(playerId);
		if (jpc) jpc.FFRX_MarkMinefield(pos);
	}

	// Reveal the nearest enemy MINEFIELD (an FFMI AP-scatter zone) within maxDist as a text grid
	// hint AND a real map marker. Unlike other intel (text only), a minefield is a danger zone the
	// player must SEE to avoid it / send the Genie to clear it. Reliable (no false leads) -- a
	// civilian warning you about mines wouldn't send you to a safe spot. Server only.
	static bool TipNearestMinefield(int playerId, vector fromPos, float maxDist)
	{
		array<vector> centers = {};
		FFRX_MineRegistry.GetMinefieldCenters(centers);
		if (centers.IsEmpty())
			return false;

		vector best;
		float bestSq = maxDist * maxDist;
		bool found = false;
		foreach (vector c : centers)
		{
			float dSq = vector.DistanceSqXZ(fromPos, c);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = c;
				found = true;
			}
		}
		if (!found)
			return false;

		int gx, gz;
		SCR_MapEntity.GetGridPos(best, gx, gz);
		SendHint(playerId, string.Format("Champ de mines ennemi signale : grille %1 - %2. Le Genie doit deminer.", gx, gz));
		SendMarker(playerId, best);
		return true;
	}

	// 8-way French compass direction from -> to (Reforger: +Z North, +X East).
	protected static string BearingWord(vector from, vector to)
	{
		float ang = Math.Atan2(to[0] - from[0], to[2] - from[2]) * Math.RAD2DEG;
		if (ang < 0) ang = ang + 360;
		int sector = (int)((ang + 22.5) / 45) % 8;
		if (sector == 1) return "au nord-est";
		if (sector == 2) return "a l'est";
		if (sector == 3) return "au sud-est";
		if (sector == 4) return "au sud";
		if (sector == 5) return "au sud-ouest";
		if (sector == 6) return "a l'ouest";
		if (sector == 7) return "au nord-ouest";
		return "au nord";
	}
}
