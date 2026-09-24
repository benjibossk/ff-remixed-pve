// FF - REMIXED - PVE
// Garage panel cosmetics for the repurposed retrieve (see FFRX_GarageMarker):
// retrieval is now free and just drops a map marker, so we hide the cost rows and
// relabel the button. The Distance row is kept (Benji wants it).
//
// Layout rows (UI/Layouts/Menu/PlayerGarageMenu.layout):
//   RetrievalFee   - money fee row      -> hide
//   CostPerKm      - supplies/km row    -> hide
//   RetrievalCost  - final cost row     -> hide
//   Distance       - distance row       -> keep
modded class JWK_PlayerGaragePanelComponent
{
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		if (SCR_Global.IsEditMode()) return;

		FFRX_HideRow(w, "RetrievalFee");
		FFRX_HideRow(w, "CostPerKm");
		FFRX_HideRow(w, "RetrievalCost");

		if (m_ButtonRetrieve)
			m_ButtonRetrieve.SetLabel("Marquer sur la carte");
	}

	protected void FFRX_HideRow(Widget root, string name)
	{
		Widget row = root.FindAnyWidget(name);
		if (row) row.SetVisible(false);
	}
}
