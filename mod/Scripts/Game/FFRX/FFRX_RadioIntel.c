// FF - REMIXED - PVE -- radio intel: Reoccupation's reports must be EARNED.
//
// WHY
// Reoccupation produces genuinely good intelligence reports, and their quality
// already scales with the share of radio towers you captured (levels 0-6 at
// 0/25/40/55/70/85% coverage). That part fits our design: it IS earned.
// What did not fit was the delivery -- a full-screen card pushed to every player
// on the server, wherever they stand, for free. Pillar 1 of GAME_DESIGN says
// information is won, not given.
//
// WHAT WE DO
// We keep their content and change the access. A report produced by the network
// is held PENDING instead of being displayed. To read it, a player must travel
// to a radio site he controls and stay near it for a few seconds ("putting on
// the headset"). The report is then delivered to that player alone, as a
// discreet hint, and consumed -- whoever made the trip has to relay it to the
// squad. An unread report expires (D7: intel perishes) and only the most recent
// one is ever pending.
//
// WHY PRESENCE AND NOT A USER ACTION
// Enfusion has no API to graft a UserAction onto an existing entity; it would
// mean overriding FF's radio-site prefab, which is fragile (and drags in the
// unbaked-entity-ID resave problem). Presence + dwell gives the same "go there
// and work for it" gameplay, touches no prefab, and survives FF updates.
//
// Server-side only (WorldSystemLocation.Server). Capture happens in
// FFRX_SilenceNotifications (the single dispatch choke point).
//
// NOTE: ASCII only (Enforce compiler desyncs on UTF-8 accents).

class FFRX_RadioIntel
{
	// --- tuning ---
	static const float LISTEN_RADIUS_M  = 30.0;   // how close to the radio site
	static const float LISTEN_SECONDS   = 8.0;    // dwell time before it is handed over
	static const float EXPIRY_SECONDS   = 1800.0; // 30 min: an unread report goes stale

	// The pending report. Only one at a time -- a newer report replaces the older
	// one, so a player who comes back after an hour gets the current picture, not
	// a stack of obsolete dispatches.
	protected static string s_sPendingText;
	protected static float  s_fPendingAge;     // seconds since it was intercepted

	// Which Reoccupation events count as "produced by the radio network".
	// Everything else stays silenced (see FFRX_SilenceNotifications).
	static bool IsRadioNetworkEvent(int eventType)
	{
		if (eventType == FFRO_ENotificationEvent.INTEL_CASUAL)
			return true;
		if (eventType == FFRO_ENotificationEvent.INTEL_THREAT)
			return true;
		if (eventType == FFRO_ENotificationEvent.RADIO_NETWORK_MILESTONE)
			return true;

		return false;
	}

	// Called from the dispatch choke point before the queue is drained.
	// Keeps the LAST radio-network report found in the batch.
	static void CaptureFromQueue(array<ref FF_QueuedNotification> queue)
	{
		if (!queue)
			return;

		foreach (FF_QueuedNotification n : queue)
		{
			if (!n)
				continue;
			if (!IsRadioNetworkEvent(n.m_iEventType))
				continue;
			if (n.m_sText == "")
				continue;

			s_sPendingText = n.m_sText;
			s_fPendingAge = 0.0;
		}
	}

	static bool HasPending()
	{
		return s_sPendingText != "";
	}

	static void ClearPending()
	{
		s_sPendingText = "";
		s_fPendingAge = 0.0;
	}

	// Ages the pending report and drops it once stale. Returns true if one is
	// still available afterwards.
	static bool AgeAndCheck(float dt)
	{
		if (s_sPendingText == "")
			return false;

		s_fPendingAge += dt;
		if (s_fPendingAge >= EXPIRY_SECONDS)
		{
			Print("[FFRX][RadioIntel] Pending report expired unread.");
			ClearPending();
			return false;
		}

		return true;
	}

	// --- the report is a PHYSICAL DOCUMENT, not a HUD hint ---
	// Intel must be an object you hold, read, keep and hand over -- same treatment
	// as the cache note dropped by enemy corpses (FFRX_CacheNote). We reuse the
	// base-game note item: SCR_CacheNoteUIInfo renders its lines as the item's
	// inspect DESCRIPTION, so reading it is a deliberate act in the inventory,
	// with no HUD popup and no map marker. A dispatch can also be dropped in a
	// crate or handed to another player -- intel becomes something you carry.
	static const ResourceName DOC_PREFAB =
		"{921CB34046441F46}Prefabs/Items/Misc/Caches/Campaign_CacheNote_Base.et";

	// Armed just before spawning the item, consumed by our SCR_CacheNoteComponent
	// override so THAT note is filled with the dispatch instead of spawning a camp.
	protected static string s_sDocText;

	static void ArmDocument(string text)
	{
		s_sDocText = text;
	}

	// Returns the armed dispatch text and disarms. Empty = this note is not ours.
	static string ConsumeDocument()
	{
		string t = s_sDocText;
		s_sDocText = "";
		return t;
	}

	// Hands the report to one player as a document. Consumes the pending report
	// only if the item actually made it into his inventory.
	static void DeliverTo(int playerId)
	{
		if (s_sPendingText == "")
			return;

		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return;

		SCR_InventoryStorageManagerComponent inv =
			JWK_CompTU<SCR_InventoryStorageManagerComponent>.FindIn(ent);
		if (!inv)
			return;

		ArmDocument(s_sPendingText);
		bool ok = inv.TrySpawnPrefabToStorage(DOC_PREFAB);

		if (!ok)
		{
			// Nothing spawned: disarm so no unrelated note picks the text up later.
			// (On success the note's OnPostInit consumes it -- clearing here would
			// break if the spawn is deferred by a frame.)
			ConsumeDocument();

			// No room: keep the dispatch pending so he can come back with space.
			// This one line is action feedback, not intel.
			JWK_PlayerControllerComponent jpc = JWK.GetPlayerController(playerId);
			if (jpc)
				jpc.FFRX_SendIntelHint("Pas de place pour ranger la depeche.");

			Print(string.Format(
				"[FFRX][RadioIntel] Player %1 has no room for the dispatch, kept pending.",
				playerId
			));
			return;
		}

		Print(string.Format(
			"[FFRX][RadioIntel] Dispatch collected by player %1 after %2s pending.",
			playerId, Math.Round(s_fPendingAge)
		));

		ClearPending();
	}

	// Distance to the closest OPERABLE radio site we control, or -1 if there is
	// none. A destroyed mast decodes nothing, and a site we do not hold is not
	// ours to listen from.
	static float NearestFriendlySiteDistance(vector pos)
	{
		World world = GetGame().GetWorld();
		if (!world)
			return -1.0;

		array<EntityID> sites = JWK_IndexSystem.Get(world).GetAll(JWK_RadioSiteEntity);
		if (!sites)
			return -1.0;

		float best = -1.0;
		foreach (EntityID id : sites)
		{
			JWK_RadioSiteEntity site = JWK_RadioSiteEntity.Cast(world.FindEntityByID(id));
			if (!site)
				continue;
			if (!site.IsOperable())
				continue;

			JWK_FactionControlComponent fc = site.GetFactionControl();
			if (!fc || !fc.IsPlayerFaction())
				continue;

			float d = vector.Distance(pos, site.GetOrigin());
			if (best < 0.0 || d < best)
				best = d;
		}

		return best;
	}

	// True if pos is within listening range of an operable radio site we control.
	static bool IsAtFriendlyRadioSite(vector pos)
	{
		float d = NearestFriendlySiteDistance(pos);
		return d >= 0.0 && d <= LISTEN_RADIUS_M;
	}

	// Dev only (#radiointel): inject a pending report to exercise the pickup path
	// without waiting out Reoccupation's 30-120 min report cycle.
	static void DebugSetPending(string text)
	{
		s_sPendingText = text;
		s_fPendingAge = 0.0;
	}
}

// ----------------------------------------------------------------------------

class FFRX_RadioIntelSystem : GameSystem
{
	protected static const float TICK_S = 2.0;

	protected float m_fAccumulator;
	// playerId -> seconds spent listening at a friendly site, reset on leaving.
	protected ref map<int, float> m_mDwell = new map<int, float>();

	override static void InitInfo(WorldSystemInfo outInfo)
	{
		outInfo
			.SetLocation(WorldSystemLocation.Server)
			.SetAbstract(false)
			.AddPoint(ESystemPoint.FixedFrame);
	}

	override protected void OnUpdate(ESystemPoint point)
	{
		if (point != ESystemPoint.FixedFrame)
			return;

		m_fAccumulator += GetWorld().GetFixedTimeSlice();
		if (m_fAccumulator < TICK_S)
			return;

		float dt = m_fAccumulator;
		m_fAccumulator = 0.0;

		if (JWK_GameModeSystem.S_GetState() != SCR_EGameModeState.GAME)
			return;

		// No report waiting (or it just went stale) -> nothing to listen to.
		if (!FFRX_RadioIntel.AgeAndCheck(dt))
		{
			m_mDwell.Clear();
			return;
		}

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> players = {};
		pm.GetPlayers(players);

		foreach (int playerId : players)
		{
			IEntity ent = pm.GetPlayerControlledEntity(playerId);
			if (!ent)
				continue;

			if (!FFRX_RadioIntel.IsAtFriendlyRadioSite(ent.GetOrigin()))
			{
				// Walked away: the dwell restarts next time.
				if (m_mDwell.Contains(playerId))
					m_mDwell.Remove(playerId);
				continue;
			}

			float dwell = 0.0;
			if (m_mDwell.Contains(playerId))
				dwell = m_mDwell.Get(playerId);

			dwell += dt;
			m_mDwell.Set(playerId, dwell);

			if (dwell >= FFRX_RadioIntel.LISTEN_SECONDS)
			{
				FFRX_RadioIntel.DeliverTo(playerId);
				// Collected -> gone for everyone. Not collected (no room) -> only
				// this player's dwell resets, so he makes space and starts over.
				if (FFRX_RadioIntel.HasPending())
					m_mDwell.Set(playerId, 0.0);
				else
					m_mDwell.Clear();

				return;
			}
		}
	}

	override protected bool ShouldBePaused()
	{
		return true;
	}
}
