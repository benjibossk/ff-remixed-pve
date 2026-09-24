// FF - REMIXED - PVE
// Fog of war: players must EARN information through intel (civilian tips, enemy
// corpse documents) — they should NOT get free info from DARC mission popups nor
// from Reoccupation event notifications. So we silence both notification channels
// at their single choke points.
//
// This does NOT touch our own intel hints: those go through FFRX_SendIntelHint ->
// JWK_HintManagerComponent (JWK.GetHint()), a separate channel.
//
//  - DARC : every popup goes through SDRC_HintHelper.ShowHint ->
//    SDRC_RplHintComp.ShowGlobalHint (broadcasts + shows). No-op it = no DARC popup.
//  - Reoccupation : all notifications are dropped EXCEPT the radio-network intel
//    reports, which are earned (you had to capture the towers) and are re-routed
//    to FFRX_RadioIntel to be collected on site. Every notification is queued
//    (Enqueue / EnqueueTargeted /
//    EnqueueTargetedImmediate / EnqueuePatrolContactWithMilestone) and then displayed
//    from the SINGLE choke point FF_NotificationQueueManager.TryDispatchNext(). We no-op
//    that one method (+ clear the queue) -> silences ALL enqueue variants at once, and
//    survives signature changes to them (Reoccupation v4.x renamed/added args to Enqueue,
//    which broke the old per-method override).
//  - Battle HUD : the top-screen banner with the location name + PROGRESS BAR
//    (JWK_BattleInfoDisplay) reveals where a capture / counter-attack is happening.
//    Hidden — players read the flag colour (capture) and hear the siren (counter-attack).
//  - Death-squad HUD : Reoccupation 5.0.0 added a live panel naming the raided
//    settlement + a civilian-support bar + a % of surviving enemy soldiers
//    (FFRO_TerrorSupportDisplayState). That is three pieces of free intel at once —
//    exactly what pillar 1 of GAME_DESIGN forbids. Hidden; the raid must be read
//    from the world (gunfire, fleeing civilians, a survivor's report).

modded class SDRC_RplHintComp
{
	override void ShowGlobalHint(string title, string msg, int dur, SDRC_EMissionIcon icon, SDRC_EHintPosition position = SDRC_EHintPosition.UP_LEFT)
	{
		// FFRX: suppressed — DARC mission info must be discovered via intel, not popped up.
		return;
	}
}

modded class FF_NotificationQueueManager
{
	// Silence at the single dispatch choke point: every enqueue variant funnels here before
	// anything is broadcast. No-op + drain so the queue never grows. Robust to Reoccupation
	// adding/renaming enqueue methods or changing their argument lists (v4.x did just that).
	override protected void TryDispatchNext()
	{
		if (!m_aQueuedNotifications)
			return;

		// Not everything here is free information: the reports produced by the
		// captured radio-tower network are EARNED, and we keep them. They are
		// held pending and must be collected at a friendly radio site instead of
		// being displayed (see FFRX_RadioIntel). Everything else is dropped.
		FFRX_RadioIntel.CaptureFromQueue(m_aQueuedNotifications);

		m_aQueuedNotifications.Clear();
	}
}

modded class FFRO_TerrorSupportDisplayState
{
	// Single choke point for the death-squad panel: both the listen-server path
	// (FFRO_BroadcastTerrorSupport_S) and the client RPC path
	// (RpcDo_FFRO_ReceiveTerrorSupport) call Receive() -> ApplyToHud(), and
	// ApplyToHud() is the only place that ever builds the widgets (EnsureHud).
	// No-op it -> nothing is ever created, no retry is ever scheduled. We keep
	// Receive() intact so the replicated state stays coherent (Reoccupation may
	// read it later); we only refuse to render it.
	override protected static void ApplyToHud()
	{
		return;
	}
}

modded class JWK_BattleInfoDisplay
{
	// FFRX: hide the top-screen battle banner (location + progress bar). It leaks
	// where a capture / counter-attack is happening. UpdateValues (which toggles the
	// child container while a battle is active) is PRIVATE and can't be overridden,
	// so we hide the whole display ROOT here: an invisible root never renders its
	// children, whatever UpdateValues does. Idempotent (safe if called again).
	// (Diegetic cues remain: flag colour for captures, siren for counter-attacks.)
	override protected event void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);
		if (m_wRoot)
			m_wRoot.SetVisible(false);
	}
}
