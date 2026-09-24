// FF - REMIXED - PVE
// Client-side confirmation dialog for a procurement request (etat-major).
//
// Shown on an etat-major player's screen when a request arrives (or via
// #testproc). Reuses the base MessageOkCancel dialog (OK = Accepter, Cancel =
// Refuser); OK -> approve, Cancel -> refuse, routed back to the server through
// the player controller. A static keep-alive list holds each handler until the
// player answers (the dialog's invokers reference it, but we keep it explicitly
// to be safe against GC).
class FFRX_ProcDialogClient
{
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<ref FFRX_ProcDialogClient> s_aAlive;

	protected static array<ref FFRX_ProcDialogClient> Alive()
	{
		if (!s_aAlive)
			s_aAlive = new array<ref FFRX_ProcDialogClient>();

		return s_aAlive;
	}

	protected JWK_PlayerControllerComponent m_Controller;
	protected int m_iReqId;

	static void Show(JWK_PlayerControllerComponent controller, int reqId, string text)
	{
		SCR_ConfigurableDialogUi dlg = SCR_CommonDialogs.CreateDialog("respawn_confirmation");
		if (!dlg) return;

		dlg.SetTitle("Demande de vehicule");
		dlg.SetMessage(text);

		FFRX_ProcDialogClient h = new FFRX_ProcDialogClient();
		h.m_Controller = controller;
		h.m_iReqId     = reqId;
		Alive().Insert(h);

		dlg.m_OnConfirm.Insert(h.OnConfirm);
		dlg.m_OnCancel.Insert(h.OnCancel);
	}

	void OnConfirm(SCR_ConfigurableDialogUi dlg)
	{
		if (m_Controller) m_Controller.FFRX_ProcRespond(m_iReqId, true);
		Done();
	}

	void OnCancel(SCR_ConfigurableDialogUi dlg)
	{
		if (m_Controller) m_Controller.FFRX_ProcRespond(m_iReqId, false);
		Done();
	}

	protected void Done()
	{
		Alive().RemoveItem(this);
	}
}
