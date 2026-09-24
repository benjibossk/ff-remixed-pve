// FF - REMIXED - PVE
// Death screen (black overlay + respawn countdown).
//
// WHY: our auto-spawn (FFRX_AutoSpawn) disables FF's spawn-selection map and holds
// the player "dead" for FFRX_DEATH_DEPLOY_DELAY_MS before deploying a new body.
// During that window the vanilla flow shows NOTHING -> testers couldn't tell they
// were dead or how long the wait was. This restores a clear death screen.
//
// Visual reused (with permission) from IronBear's ConflictEscalation death screen:
// a vignette/darken overlay + centered "dead" icon + message + countdown text.
// The DRIVER is FF-native and self-contained (ConflictEscalation's own driver is
// tied to its Campaign/Capture&Hold/injury systems, unusable here).
//
// Design notes:
// - Pure CLIENT UI. A dedicated server has no workspace -> everything no-ops there.
// - Event trigger is a light 250 ms poll of the LOCAL controlled character's
//   IsDead() (started once from SCR_PlayerController.OnControlledEntityChanged).
//   Polling is used over the death ScriptInvoker because life state crosses the
//   client/server replication boundary and the invoker is not reliably raised on
//   the owning client.
// - ASCII only in the on-screen strings (Enforce desyncs on UTF-8 accents).
//
// SETUP (one-time, Benji): Register & Import UI/layouts/HUD/FFRX_DeathScreen.layout
// in the Workbench, then paste its GUID into FFRX_DEATHSCREEN_LAYOUT below.

class FFRX_DeathScreen
{
	static const ResourceName FFRX_DEATHSCREEN_LAYOUT =
		"{CCD35EE276D34835}UI/layouts/HUD/FFRX_DeathScreen.layout";

	// Base-game blur panel (same one FFRX_IntroCinematic uses) -> soft, blurred backdrop.
	static const ResourceName FFRX_BLUR_LAYOUT =
		"{352322057FD78AE1}UI/layouts/WidgetLibrary/BaseElements/WLib_Blur.layout";

	protected static ref FFRX_DeathScreen s_Instance;
	protected bool m_bRunning;
	protected bool m_bShown;
	protected int m_iDeathStartMs;

	// --- diagnostics (temporary) ---
	protected bool m_bLastDead;
	protected int m_iTickCount;

	protected Widget m_wRoot;
	protected TextWidget m_wTime;

	static FFRX_DeathScreen Get()
	{
		if (!s_Instance) s_Instance = new FFRX_DeathScreen();
		return s_Instance;
	}

	// Start the poll loop once (local client only). Safe to call repeatedly.
	void EnsureRunning()
	{
		if (m_bRunning) { Print("[FFRX][Death] EnsureRunning: already running"); return; }
		if (!GetGame().GetWorkspace()) { Print("[FFRX][Death] EnsureRunning: no workspace (dedicated server) -> skip"); return; }
		Print("[FFRX][Death] EnsureRunning: STARTING 250ms death poll");
		m_bRunning = true;
		GetGame().GetCallqueue().CallLater(Tick, 250, true);
	}

	protected void Tick()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc) { HideOverlay(); return; }

		IEntity char = pc.GetControlledEntity();
		bool hasCC = false;
		bool aliveChar = false;
		if (char)
		{
			CharacterControllerComponent cc =
				CharacterControllerComponent.Cast(char.FindComponent(CharacterControllerComponent));
			if (cc)
			{
				hasCC = true;
				if (!cc.IsDead()) aliveChar = true;
			}
		}

		// Show the overlay whenever the player is NOT controlling a LIVING character:
		// a dead corpse OR an unpossessed "waiting to respawn" state. Our auto-spawn
		// UNPOSSESSES the corpse ~1s after death (char becomes null) and holds the
		// player for the death delay -> the poll must NOT treat "no char" as alive,
		// or it would hide the overlay for the rest of the wait (the bug: player saw
		// their corpse with no screen). Only a fresh LIVING character hides it.
		bool showIt = !aliveChar;

		// Log the first couple of ticks + every time the state flips.
		m_iTickCount++;
		if (m_iTickCount <= 2 || showIt != m_bLastDead)
		{
			Print(string.Format("[FFRX][Death] Tick#%1: hasChar=%2 hasCC=%3 aliveChar=%4 show=%5 shown=%6",
				m_iTickCount, char != null, hasCC, aliveChar, showIt, m_bShown));
			m_bLastDead = showIt;
		}

		if (showIt) ShowOverlay();
		else HideOverlay();
	}

	protected void ShowOverlay()
	{
		if (!m_bShown)
		{
			WorkspaceWidget ws = GetGame().GetWorkspace();
			if (!ws) return;

			// Built entirely in CODE (no .layout): the layout-file version created its
			// widgets but the centered text/icon never sized (0-size container) even after
			// forcing the root to fill -> Benji saw only a dim screen, no message. Building
			// the widgets directly (like FFRX_IntroCinematic does, which works) removes all
			// slot-sizing ambiguity.

			// Fullscreen root.
			m_wRoot = ws.CreateWidget(WidgetType.FrameWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS,
				Color.FromInt(0xFFFFFFFF), 60);
			FrameSlot.SetAnchorMin(m_wRoot, 0, 0);
			FrameSlot.SetAnchorMax(m_wRoot, 1, 1);
			FrameSlot.SetOffsets(m_wRoot, 0, 0, 0, 0);

			// Fullscreen BLUR backdrop (soft, out-of-focus scene) behind everything.
			Widget blur = ws.CreateWidgets(FFRX_BLUR_LAYOUT, m_wRoot);
			if (blur)
			{
				FrameSlot.SetAnchorMin(blur, 0, 0);
				FrameSlot.SetAnchorMax(blur, 1, 1);
				FrameSlot.SetOffsets(blur, 0, 0, 0, 0);
			}

			// Darken panel on top of the blur (ImageWidget with no texture = solid tint).
			ImageWidget dark = ImageWidget.Cast(ws.CreateWidget(WidgetType.ImageWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(0x00000000), 61, m_wRoot));
			if (dark)
			{
				FrameSlot.SetAnchorMin(dark, 0, 0);
				FrameSlot.SetAnchorMax(dark, 1, 1);
				FrameSlot.SetOffsets(dark, 0, 0, 0, 0);
				dark.SetColor(new Color(0, 0, 0, 0.6));
			}

			// Centered column: "VOUS ETES MORT" + countdown.
			Widget col = ws.CreateWidget(WidgetType.VerticalLayoutWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(0x00000000), 62, m_wRoot);
			FrameSlot.SetAnchorMin(col, 0.5, 0.5);
			FrameSlot.SetAnchorMax(col, 0.5, 0.5);
			FrameSlot.SetAlignment(col, 0.5, 0.5);
			FrameSlot.SetSizeToContent(col, true);

			TextWidget dead = TextWidget.Cast(ws.CreateWidget(WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(0xFFF20E0E), 63, col));
			if (dead)
			{
				dead.SetText("VOUS ETES MORT");
				dead.SetExactFontSize(52);
				dead.SetColor(new Color(0.95, 0.06, 0.06, 1));
			}

			m_wTime = TextWidget.Cast(ws.CreateWidget(WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(0xFFFFFFFF), 63, col));
			if (m_wTime)
			{
				m_wTime.SetExactFontSize(30);
				m_wTime.SetColor(new Color(1, 1, 1, 1));
			}

			Print(string.Format("[FFRX][Death] ShowOverlay: overlay built in code, timeWidget=%1", m_wTime != null));
			m_iDeathStartMs = NowMs();
			m_bShown = true;
		}

		if (!m_wTime) return;

		int total = JWK_PlayerControllerComponent.FFRX_DEATH_DEPLOY_DELAY_MS;
		int remain = (total - (NowMs() - m_iDeathStartMs)) / 1000;
		if (remain > 0)
			m_wTime.SetText(string.Format("Reapparition dans %1 s", remain));
		else
			m_wTime.SetText("Reapparition...");
	}

	protected void HideOverlay()
	{
		if (!m_bShown) return;
		m_bShown = false;
		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
			m_wTime = null;
		}
	}

	protected int NowMs()
	{
		World w = GetGame().GetWorld();
		if (!w) return 0;
		return (int)(w.GetWorldTime());
	}
}

// NOTE: the SCR_PlayerController hook that arms this death-watch lives in
// FFRX_IntroCinematic.c's modded SCR_PlayerController.OnControlledEntityChanged
// (Enforce allows only ONE modded block per class per addon, and the intro already
// mods that class). It calls FFRX_DeathScreen.Get().EnsureRunning() after super.
