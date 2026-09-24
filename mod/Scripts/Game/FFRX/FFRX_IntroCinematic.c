// FF - REMIXED - PVE
// Scripted intro cinematic (client-side). Replaces the old hand-keyframed
// CinematicScene with a scripted orbit camera (portable positions).
//
// PHASE 1: on the local player's first spawn this session, a scripted camera
// orbits around the spawn point while slowly approaching, looking at it, for
// ~20s, then hands control back. It also DELAYS FF's "Welcome" splash until the
// cinematic ends (otherwise it covers the screen).
//
// PLAYTIME GATE: on first spawn the client asks the server for its cumulated Fleet
// playtime in seconds (Flt_XpProgression.GetPlaytime, server-only). Below
// INTRO_MAX_PLAYSEC (30 min) the intro plays; older players skip it and the welcome
// opens right away. See the SCR_PlayerController hook + RPCs at the bottom.
// NB: we gate on PLAYTIME, not on XP. Fleet's XP is clamped to the current grade's
// band (see Flt_XpProgression.AddXp), so it stops rising and is not a usable
// measure of seniority; the playtime counter is never capped.
//
// WorldCamera.et is a SCR_CameraBase; we capture the current camera before
// taking over and restore it on stop.

class FFRX_IntroLine
{
	string m_sText;
	float m_fDurationMs;
}

class FFRX_IntroCinematic
{
	protected static const ResourceName CAMERA_PREFAB = "{64B2F8D8059CF474}Prefabs/Cameras/WorldCamera.et";

	// Playtime gate: the intro only plays for players below this cumulated playtime on
	// the server, in seconds. 1800 = 30 min, i.e. exactly the window before Fleet
	// auto-promotes them to Soldat (Flt_XpProgression.AUTO_SOLDAT_SECONDS).
	static const int INTRO_MAX_PLAYSEC = 1800;

	protected static const int   FRAME_MS      = 16;

	protected static const float START_RADIUS  = 300;
	protected static const float END_RADIUS    = 140;
	protected static const float START_HEIGHT  = 70;
	protected static const float END_HEIGHT    = 28;
	protected static const float SWEEP_DEG     = 120;
	protected static const float LOOK_UP       = 2;

	// --- Coordination with the FF welcome splash ---
	// s_bBlocking is set the moment we know the intro will play (BEFORE super in
	// the controller, so FF's welcome trigger sees it) and cleared when the
	// intro ends; s_OnFinished then fires so the welcome can open.
	static bool s_bBlocking;
	protected static ref ScriptInvokerVoid s_OnFinished;

	static void MarkPending()
	{
		s_bBlocking = true;
	}

	static ScriptInvokerVoid GetOnFinished()
	{
		if (!s_OnFinished) s_OnFinished = new ScriptInvokerVoid();
		return s_OnFinished;
	}

	protected static void ReleaseBlock()
	{
		s_bBlocking = false;
		if (s_OnFinished) {
			ScriptInvokerVoid inv = s_OnFinished;
			s_OnFinished = null;
			inv.Invoke();
		}
	}

	// Public: used by the XP gate to release the deferred welcome splash when the intro
	// is SKIPPED (veteran) without ever starting the cinematic.
	static void ForceRelease()
	{
		ReleaseBlock();
	}

	// --- Instance state ---
	protected SCR_CameraBase m_Camera;
	protected CameraBase m_PreviousCamera;
	protected CameraManager m_CameraManager;
	protected vector m_vTarget;
	protected float m_fStartTime;
	protected float m_fStartAngleDeg;
	protected bool m_bRunning;

	// --- On-screen text (P3): sequenced lines with per-line duration ---
	// Ported from the old SD_CinematicOnSpawn config (m_aLines / CinematicTextLine
	// + {PLAYERNAME} token). ASCII for now; accents should come via localization.
	protected static const float TEXT_FADE_MS = 800;
	protected Widget m_wTextRoot;
	protected RichTextWidget m_wText;
	protected ref array<ref FFRX_IntroLine> m_aLines = {};
	protected float m_fTotalMs;

	void Start(vector targetPos)
	{
		if (m_bRunning) return;

		m_CameraManager = GetGame().GetCameraManager();
		if (!m_CameraManager) {
			Print("[FFRX][Intro] No CameraManager - abort.");
			ReleaseBlock();
			return;
		}

		m_vTarget = targetPos;
		m_fStartTime = GetGame().GetWorld().GetWorldTime();
		m_fStartAngleDeg = 0;
		BuildLines();

		vector startPos = ComputeCameraPos(0);
		IEntity cam = JWK_SpawnUtils.SpawnEntityPrefabLocal(CAMERA_PREFAB, startPos);
		if (!cam) {
			Print("[FFRX][Intro] Camera prefab failed to spawn - abort.");
			ReleaseBlock();
			return;
		}

		m_Camera = SCR_CameraBase.Cast(cam);
		if (!m_Camera) {
			Print(string.Format("[FFRX][Intro] Not a SCR_CameraBase (%1) - abort.", cam.ClassName()));
			SCR_EntityHelper.DeleteEntityAndChildren(cam);
			ReleaseBlock();
			return;
		}

		m_Camera.SetName("FFRX_IntroCam");
		m_PreviousCamera = m_CameraManager.CurrentCamera();
		ApplyCameraTransform(startPos);
		m_CameraManager.SetCamera(m_Camera);

		m_bRunning = true;
		CreateText();
		GetGame().GetCallqueue().CallLater(OnFrame, FRAME_MS, true);

		// Safety net: force a full cleanup after the duration (+2s margin), so the
		// text and blur box can never linger on screen if the frame loop breaks.
		GetGame().GetCallqueue().CallLater(Stop, m_fTotalMs + 2000, false);

		Print(string.Format("[FFRX][Intro] STARTED. target=%1 startPos=%2 prevCam=%3",
			m_vTarget, startPos, m_PreviousCamera != null));
	}

	protected void OnFrame()
	{
		if (!m_bRunning || !m_Camera) {
			Stop();
			return;
		}

		float elapsed = GetGame().GetWorld().GetWorldTime() - m_fStartTime;
		float progress = elapsed / m_fTotalMs;
		if (progress >= 1) {
			Print("[FFRX][Intro] Finished - restoring player camera.");
			Stop();
			return;
		}

		if (m_Camera != m_CameraManager.CurrentCamera())
			m_CameraManager.SetCamera(m_Camera);

		ApplyCameraTransform(ComputeCameraPos(progress));
		UpdateText(elapsed);
	}

	protected vector ComputeCameraPos(float progress)
	{
		float angleDeg = m_fStartAngleDeg + SWEEP_DEG * progress;
		float angle = angleDeg * Math.DEG2RAD;

		float radius = Math.Lerp(START_RADIUS, END_RADIUS, progress);
		float height = Math.Lerp(START_HEIGHT, END_HEIGHT, progress);

		return Vector(
			m_vTarget[0] + Math.Cos(angle) * radius,
			m_vTarget[1] + height,
			m_vTarget[2] + Math.Sin(angle) * radius
		);
	}

	protected void ApplyCameraTransform(vector camPos)
	{
		vector lookAt = m_vTarget;
		lookAt[1] = lookAt[1] + LOOK_UP;

		vector mat[4];
		SCR_Math3D.LookAt(camPos, lookAt, vector.Up, mat);
		mat[3] = camPos;
		m_Camera.SetTransform(mat);
	}

	// Idempotent: safe to call to cut the cinematic short (e.g. death mid-run).
	void Stop()
	{
		GetGame().GetCallqueue().Remove(OnFrame);
		GetGame().GetCallqueue().Remove(Stop); // cancel the safety timer

		bool wasActive = m_bRunning || m_Camera != null;
		m_bRunning = false;

		if (m_CameraManager && m_PreviousCamera)
			m_CameraManager.SetCamera(m_PreviousCamera);

		if (m_Camera) {
			SCR_EntityHelper.DeleteEntityAndChildren(m_Camera);
			m_Camera = null;
		}

		DestroyText();

		if (wasActive) ReleaseBlock();
	}

	// --- Text overlay ---

	protected static const ResourceName BLUR_LAYOUT = "{352322057FD78AE1}UI/layouts/WidgetLibrary/BaseElements/WLib_Blur.layout";

	protected void CreateText()
	{
		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws) return;

		// Full-screen root; without its own 0..1 anchors it stays a tiny top-left
		// box and children anchor to nothing (text ended up top + overflowing).
		m_wTextRoot = ws.CreateWidget(WidgetType.FrameWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS,
			Color.FromInt(Color.WHITE), 50);
		FrameSlot.SetAnchorMin(m_wTextRoot, 0, 0);
		FrameSlot.SetAnchorMax(m_wTextRoot, 1, 1);
		FrameSlot.SetOffsets(m_wTextRoot, 0, 0, 0, 0);

		// Blur panel behind the text (base-game blur layout), lower-centre band.
		Widget blur = ws.CreateWidgets(BLUR_LAYOUT, m_wTextRoot);
		if (blur) {
			FrameSlot.SetAnchorMin(blur, 0.18, 0.76);
			FrameSlot.SetAnchorMax(blur, 0.82, 0.93);
			FrameSlot.SetOffsets(blur, 0, 0, 0, 0);
		}

		// Wrapped, centred text on top.
		Widget t = ws.CreateWidget(WidgetType.RichTextWidgetTypeID,
			WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS,
			new Color(1, 1, 1, 1), 51, m_wTextRoot);
		m_wText = RichTextWidget.Cast(t);
		if (!m_wText) return;

		m_wText.SetExactFontSize(26);
		FrameSlot.SetAnchorMin(m_wText, 0.2, 0.78);
		FrameSlot.SetAnchorMax(m_wText, 0.8, 0.92);
		FrameSlot.SetOffsets(m_wText, 8, 8, 8, 8);
		m_wText.SetOpacity(0);
	}

	protected void UpdateText(float elapsedMs)
	{
		if (!m_wText || m_aLines.IsEmpty()) return;

		// Find the current line by walking cumulative durations.
		float cursor = 0;
		foreach (FFRX_IntroLine line : m_aLines) {
			float local = elapsedMs - cursor;
			if (local < line.m_fDurationMs) {
				float op = 1;
				if (local < TEXT_FADE_MS)
					op = local / TEXT_FADE_MS;
				else if (local > line.m_fDurationMs - TEXT_FADE_MS)
					op = (line.m_fDurationMs - local) / TEXT_FADE_MS;

				m_wText.SetText(line.m_sText);
				m_wText.SetOpacity(Math.Clamp(op, 0, 1));
				return;
			}
			cursor += line.m_fDurationMs;
		}

		// Past the last line.
		m_wText.SetOpacity(0);
	}

	protected void BuildLines()
	{
		m_aLines = {};

		string playerName = "soldat";
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm) {
			string n = pm.GetPlayerName(SCR_PlayerController.GetLocalPlayerId());
			if (!n.IsEmpty()) playerName = n;
		}

		AddLine(string.Format("Bienvenue %1.", playerName), 3500);
		AddLine("Vous rejoignez l'Armee francaise sur un theatre de liberation.", 5000);
		AddLine("Ici, on ne combat jamais seul : reconnaissance, renseignement\net logistique decident de tout.", 6000);
		AddLine("Trouvez l'intel, securisez la logistique, liberez le pays.\nEnsemble.", 5000);
		// RichTextWidget only wraps via a layout (Wrap 1), not from code -> hard
		// line breaks keep the long line inside the screen for now.
		AddLine(
			"Vous arrivez au DEPOT d'accueil (canal radio 30 MHz).\n"
			+ "Recuperez votre equipement, puis appuyez sur P pour\n"
			+ "rejoindre une escouade de combat (ALPHA, BRAVO...).\n"
			+ "Radio : touche G. Besoin d'aide ? Canal 30 MHz.",
			9000);

		m_fTotalMs = 0;
		foreach (FFRX_IntroLine line : m_aLines)
			m_fTotalMs += line.m_fDurationMs;

		if (m_fTotalMs < 1) m_fTotalMs = 1;
	}

	protected void AddLine(string text, float durationMs)
	{
		FFRX_IntroLine line = new FFRX_IntroLine();
		line.m_sText = text;
		line.m_fDurationMs = durationMs;
		m_aLines.Insert(line);
	}

	protected void DestroyText()
	{
		if (m_wText) {
			m_wText.RemoveFromHierarchy();
			m_wText = null;
		}
		if (m_wTextRoot) {
			m_wTextRoot.RemoveFromHierarchy();
			m_wTextRoot = null;
		}
	}

	bool IsRunning()
	{
		return m_bRunning;
	}
}

// ----------------------------------------------------------------------------
// Trigger: on the LOCAL player's first character possession this session, gated
// by the player's Fleet playtime-XP (asked from the server, see RPCs below).

modded class SCR_PlayerController
{
	protected ref FFRX_IntroCinematic m_FFRXIntro;
	protected bool m_bFFRXIntroPlayed;
	protected vector m_vFFRXIntroTarget;   // where the intro camera should orbit
	protected bool m_bFFRXAwaitingXp;      // waiting for the server's XP answer

	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		// A character change while the intro is running (death/respawn) cuts it
		// short and cleanly returns control.
		if (m_FFRXIntro && m_FFRXIntro.IsRunning())
			m_FFRXIntro.Stop();

		bool willPlay = m_bIsLocalPlayerController
			&& !m_bFFRXIntroPlayed
			&& to != null
			&& SCR_ChimeraCharacter.Cast(to) != null;

		// Block the FF welcome splash BEFORE super (super fires FF's welcome
		// trigger downstream), so it waits for the XP decision / cinematic. If the
		// XP check then SKIPS the intro (veteran) we release the block immediately.
		if (willPlay)
			FFRX_IntroCinematic.MarkPending();

		super.OnControlledEntityChanged(from, to);

		// Death screen: arm the local client's death watch once it controls a
		// character. Merged here because Enforce allows only ONE modded
		// SCR_PlayerController per addon (see FFRX_DeathScreen).
		if (m_bIsLocalPlayerController)
		{
			Print(string.Format("[FFRX][Death] OnControlledEntityChanged(local): to=%1 -> EnsureRunning", to != null));
			FFRX_DeathScreen.Get().EnsureRunning();

			// Touche J -> menu de construction (FFRX_BuildDirect).
			//
			// POURQUOI ICI ET PAS DANS UN GameSystem CLIENT : mesure faite le 2026-09-17 sur
			// le dedie -- un `GameSystem` en WorldSystemLocation.Client ne s'amorce PAS chez
			// un joueur connecte a un serveur dedie. Ni FFRX_BuildDirectSystem ni
			// FFRX_MainMenuTrimSystem n'ont jamais imprime leur ligne d'amorcage, alors que
			// le fichier etait bien dans le pak publie (verifie) et que ce hook-ci, lui,
			// imprime a chaque fois dans le meme log.
			//
			// On s'accroche donc a un chemin PROUVE plutot qu'a un chemin theorique. Meme
			// raison que le regroupement du death screen ci-dessus.
			//
			// ⚠️ CORRECTION 2026-09-21 : la phrase qui suivait ici -- "Enforce n'autorise
			// qu'UN SEUL `modded class SCR_PlayerController` par addon" -- est FAUSSE, et
			// elle a fait entasser du code dans des fichiers sans rapport. Mesure : 6 blocs
			// `modded class SCR_PlayerController` coexistent dans cet addon, dont DEUX
			// surchargent `OnControlledEntityChanged` (ici et FFRX_VehicleSmokeScreen.c:295),
			// et l'ensemble compile avec 0 erreur. Les `modded class` s'ENCHAINENT, meme au
			// sein d'un addon, tant que chacun appelle `super`.
			// Le regroupement reste justifie ici pour une AUTRE raison, la vraie : ce chemin
			// est PROUVE par les logs, contrairement au GameSystem client qui ne s'amorce pas
			// sur le dedie. Ne pas y voir une contrainte du langage.
			FFRX_BuildDirect.Arm();

			// Memorise l'UID du joueur pour le PROCHAIN ecran de chargement.
			//
			// Mesure du 18/09 : pendant le chargement, aucune session joueur n'existe encore
			// (PlayerController null, 0 joueur connu, GetPlayerIdentityId vide) -- l'ecran ne
			// peut donc pas savoir QUI charge. Ici, en revanche, on est en partie et
			// l'identite est disponible : on l'ecrit dans le profil, et FFRX_LoadingFeed la
			// relira au chargement suivant pour afficher grade et XP.
			FFRX_MeCache.Remember();
		}

		Print(string.Format("[FFRX][Intro] OnControlledEntityChanged local=%1 willPlay=%2 played=%3",
			m_bIsLocalPlayerController, willPlay, m_bFFRXIntroPlayed));

		if (willPlay)
		{
			// One shot per session; the play/skip decision needs the Fleet playtime,
			// which lives server-side only.
			m_bFFRXIntroPlayed = true;
			m_vFFRXIntroTarget = to.GetOrigin();

			if (Replication.IsServer())
			{
				// Listen host / Workbench: playtime is readable locally -> decide now.
				FFRX_ResolveIntroXp(FFRX_LocalFleetPlaytime(), FFRX_LocalIsRanked());
			}
			else
			{
				// Dedicated client: ask the server for our playtime.
				m_bFFRXAwaitingXp = true;
				Print("[FFRX][Intro] Requesting Fleet playtime from server for intro gating...");
				Rpc(FFRX_RpcRequestIntroXp);
				// Safety net: if the server never answers (RPC lost), play the intro
				// after 3s rather than leaving the welcome splash deferred forever.
				GetGame().GetCallqueue().CallLater(FFRX_IntroXpTimeout, 3000, false);
			}
		}
	}

	// Read the local player's cumulated Fleet playtime in seconds (server authority only).
	protected int FFRX_LocalFleetPlaytime()
	{
		int pid = GetPlayerId();
		BackendApi ba = GetGame().GetBackendApi();
		if (ba && pid > 0)
		{
			string uid = ba.GetPlayerIdentityId(pid);
			if (uid != "")
			{
				Flt_XpProgression.Load();
				return Flt_XpProgression.GetPlaytime(uid);
			}
		}
		return 0;
	}

	//! Le joueur a-t-il un grade attribue par l'etat-major / le site ? Serveur seul.
	//!
	//! POURQUOI CE SECOND CRITERE. Le compteur de temps de jeu a ete ajoute APRES coup :
	//! un ancien qui a des centaines d'heures peut avoir un playSec ridicule (constate :
	//! 4999 XP soit un grade eleve, mais 840 s de compteur), et se reprenait la
	//! cinematique de bleu a chaque connexion. Un grade assigne est une preuve
	//! d'anciennete que le compteur, lui, ne connait pas encore.
	protected bool FFRX_LocalIsRanked()
	{
		int pid = GetPlayerId();
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba || pid <= 0)
			return false;

		string uid = ba.GetPlayerIdentityId(pid);
		if (uid == "")
			return false;

		// >= 1 : Soldat ou mieux. 0 (Renegat/Deserteur) et -1 (aucun grade) = bleu.
		return Flt_RankRegistry.GetInstance().GetAssignedRank(uid) >= 1;
	}

	// SERVER: read the calling player's Fleet playtime + grade and answer the owning client.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void FFRX_RpcRequestIntroXp()
	{
		int playSec = FFRX_LocalFleetPlaytime();
		bool ranked = FFRX_LocalIsRanked();
		Print(string.Format("[FFRX][Intro] Server: player %1 Fleet playtime = %2 s, grade attribue = %3 -> answering client.",
			GetPlayerId(), playSec, ranked));
		Rpc(FFRX_RpcRecvIntroXp, playSec, ranked);
	}

	// OWNER CLIENT: got the playtime from the server.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcRecvIntroXp(int playSec, bool ranked)
	{
		if (!m_bFFRXAwaitingXp)
			return; // already resolved by the timeout fallback
		m_bFFRXAwaitingXp = false;
		GetGame().GetCallqueue().Remove(FFRX_IntroXpTimeout);
		FFRX_ResolveIntroXp(playSec, ranked);
	}

	// Decide: play the intro (newcomer) or skip it and release the welcome.
	// Deux portes de sortie, il suffit d'UNE : assez d'heures au compteur, OU un grade
	// attribue. La seconde rattrape les anciens d'avant le compteur de temps de jeu.
	protected void FFRX_ResolveIntroXp(int playSec, bool ranked)
	{
		if (ranked)
		{
			Print(string.Format("[FFRX][Intro] Grade attribue (playtime %1 s) -> SKIP intro; release welcome.", playSec));
			FFRX_IntroCinematic.ForceRelease();
			return;
		}

		if (playSec < FFRX_IntroCinematic.INTRO_MAX_PLAYSEC)
		{
			Print(string.Format("[FFRX][Intro] Playtime %1 s < %2 s et aucun grade -> PLAY intro.", playSec, FFRX_IntroCinematic.INTRO_MAX_PLAYSEC));
			FFRX_StartIntroNow();
		}
		else
		{
			Print(string.Format("[FFRX][Intro] Playtime %1 s >= %2 s -> SKIP intro; release welcome.", playSec, FFRX_IntroCinematic.INTRO_MAX_PLAYSEC));
			FFRX_IntroCinematic.ForceRelease();
		}
	}

	protected void FFRX_IntroXpTimeout()
	{
		if (!m_bFFRXAwaitingXp)
			return;
		m_bFFRXAwaitingXp = false;
		Print("[FFRX][Intro] No XP answer within 3s -> playing intro (fallback).");
		FFRX_StartIntroNow();
	}

	protected void FFRX_StartIntroNow()
	{
		m_FFRXIntro = new FFRX_IntroCinematic();
		m_FFRXIntro.Start(m_vFFRXIntroTarget);
	}

	//! SERVEUR -> client proprietaire : rejoue la cinematique a la demande (#intro).
	//! La cinematique est purement CLIENT (camera locale), donc la commande, elle,
	//! s'execute sur le serveur : il faut ce relais pour atteindre le bon joueur.
	void FFRX_ReplayIntroForOwner()
	{
		Rpc(FFRX_RpcReplayIntro);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void FFRX_RpcReplayIntro()
	{
		FFRX_ReplayIntroLocal();
	}

	//! Rejoue chez le joueur local, quoi qu'il arrive (aucune porte de sortie : c'est
	//! une commande explicite d'admin, pas le declenchement automatique).
	void FFRX_ReplayIntroLocal()
	{
		if (m_FFRXIntro && m_FFRXIntro.IsRunning())
			m_FFRXIntro.Stop();

		IEntity ch = GetControlledEntity();
		if (!ch)
		{
			Print("[FFRX][Intro] #intro : aucun personnage controle -- rien a filmer.", LogLevel.WARNING);
			return;
		}

		m_vFFRXIntroTarget = ch.GetOrigin();
		Print("[FFRX][Intro] #intro : relecture manuelle de la cinematique.");
		FFRX_StartIntroNow();
	}
}

// ----------------------------------------------------------------------------
//  #intro -- rejouer la cinematique d'accueil (admin), pour la verifier a la
//  demande sans devoir se fabriquer un compte neuf.
// ----------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_IntroCommand : ScrServerCommand
{
	override string GetKeyword() { return "intro"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return ScrServerCmdResult("PlayerManager indisponible.", EServerCmdResultType.ERR);

		SCR_PlayerController ctrl = SCR_PlayerController.Cast(pm.GetPlayerController(playerId));
		if (!ctrl)
			return ScrServerCmdResult("Controleur introuvable.", EServerCmdResultType.ERR);

		// Sur un listen host / Workbench, le serveur EST le client : on joue en direct
		// plutot que de s'envoyer un RPC a soi-meme.
		if (playerId == SCR_PlayerController.GetLocalPlayerId())
			ctrl.FFRX_ReplayIntroLocal();
		else
			ctrl.FFRX_ReplayIntroForOwner();

		return ScrServerCmdResult("Cinematique d'accueil relancee.", EServerCmdResultType.OK);
	}

	// RCON n'a pas de joueur appelant : on ne saurait pas chez QUI jouer la camera.
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("#intro doit etre lance depuis le chat en jeu.", EServerCmdResultType.ERR);
	}

	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}

// ----------------------------------------------------------------------------
// Delay FF's "Welcome to Freedom Fighters" splash until the cinematic ends.

modded class JWK_FirstTimeMenuComponent
{
	override void OnPlayerControlledEntityChanged(int playerId, IEntity from, IEntity to)
	{
		if (playerId == SCR_PlayerController.GetLocalPlayerId()
			&& to && !m_bShown
			&& FFRX_IntroCinematic.s_bBlocking) {
			m_bShown = true;
			FFRX_IntroCinematic.GetOnFinished().Insert(FFRX_OpenWelcomeDeferred);
			Print("[FFRX][Intro] Welcome splash deferred until cinematic ends.");
			return;
		}

		super.OnPlayerControlledEntityChanged(playerId, from, to);
	}

	protected void FFRX_OpenWelcomeDeferred()
	{
		JWK.GetUI().OpenContext(JWK_FirstTimeMenuContext);
	}
}
