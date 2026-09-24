// FF - REMIXED - PVE
// Category filter bar for the arsenal / shop menu.
//
// A row of category buttons at the TOP-LEFT of the shop (under the title); clicking one
// filters the item grid to that category. Only categories actually PRESENT in this shop's
// inventory are shown (a weapon arsenal shows only weapon categories, etc.). Reuses the
// shop's own view-filter pipeline (CheckViewFiltersInclude, also driven by the search box).
// Classification via FFRX_ArsenalCategory (SCR_EArsenalItemType from the faction catalog).

// Click handler for a category button (pure code, no custom layout needed).
class FFRX_ShopCatButtonHandler : ScriptedWidgetEventHandler
{
	JWK_ShopInterfaceUIComponent m_Shop;
	int m_iMask;

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Shop)
			m_Shop.FFRX_SelectCategory(m_iMask);
		return true;
	}
}

modded class JWK_ShopInterfaceUIComponent
{
	// Position of the category bar within rootFrame, just below the "Shop" title/orange
	// stripe. Bump Y up/down if it overlaps the title or the grid; lower X to shift the
	// row further left.
	protected static const float FFRX_CAT_BAR_X = 40;
	protected static const float FFRX_CAT_BAR_Y = 200;

	protected int m_iFFRXCat = 0; // selected category mask (0 = show all)
	protected ref array<Widget> m_aFFRXCatBtns = {};
	protected ref array<TextWidget> m_aFFRXCatTexts = {};
	protected ref array<int> m_aFFRXCatMasks = {};

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);
		FFRX_BuildCategoryBar(w);
	}

	// The shop rebuilds its grid by keeping only items for which this returns true
	// (called from ReinitItemsView, also triggered by the search box). We add the
	// category gate on top of the base (search) filter.
	override bool CheckViewFiltersInclude(JWK_ShopContextItem item)
	{
		if (!super.CheckViewFiltersInclude(item))
			return false;

		if (m_iFFRXCat != 0 && item && !FFRX_ArsenalCategory.Matches(item.prefab, m_iFFRXCat))
			return false;

		return true;
	}

	// After the shop (re)loads its full item model, hide the category buttons whose
	// type is not present in this arsenal -> a weapon crate shows only weapon tabs, etc.
	override void ReinitItemsModel(map<ResourceName, int> inventoryList)
	{
		super.ReinitItemsModel(inventoryList);
		FFRX_UpdateCatVisibility();
	}

	void FFRX_SelectCategory(int mask)
	{
		m_iFFRXCat = mask;
		FFRX_RefreshCatHighlight();
		ReinitItemsView();
	}

	protected void FFRX_UpdateCatVisibility()
	{
		if (m_aFFRXCatBtns.IsEmpty())
			return;

		// OR of the arsenal types present in the current inventory.
		int present = 0;
		foreach (JWK_ShopContextItem it : m_CurrentItemsModel)
		{
			if (it)
				present |= FFRX_ArsenalCategory.GetItemType(it.prefab);
		}

		bool selectedGone = false;
		foreach (int i, Widget btn : m_aFFRXCatBtns)
		{
			if (!btn)
				continue;

			int mask = m_aFFRXCatMasks[i];
			bool vis = (mask == 0) || ((mask & present) != 0); // "Tout" (0) always visible
			btn.SetVisible(vis);

			if (!vis && mask == m_iFFRXCat)
				selectedGone = true;
		}

		// If the active category no longer has any items, fall back to "Tout".
		if (selectedGone)
		{
			m_iFFRXCat = 0;
			FFRX_RefreshCatHighlight();
			ReinitItemsView();
		}
	}

	protected void FFRX_BuildCategoryBar(Widget w)
	{
		if (!m_aFFRXCatMasks.IsEmpty()) // already built for this instance
			return;

		Widget root = m_wRoot;
		if (!root)
			root = w;
		if (!root)
			return;

		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
			return;

		// Anchor at the shop's outer FrameWidget ("rootFrame") -- this renders reliably
		// (the earlier attempt to parent into a named inner widget failed: "Frame0" is
		// ambiguous and matched a grid-card sub-widget, so the bar vanished). We keep the
		// proven floating placement but sit BELOW the orange title stripe: the "Shop"
		// title band is ~y128 tall, so y=200 clears it into the empty left band above the
		// item grid. Tune FFRX_CAT_BAR_Y if it lands on the title or the cards.
		Widget frame = root;
		while (frame && frame.GetName() != "rootFrame")
			frame = frame.GetParent();

		bool floating = frame != null;
		Widget anchor = frame;
		if (!anchor)
			anchor = root;

		Widget bar = ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID,
			WidgetFlags.VISIBLE, Color.FromInt(0x00000000), 0, anchor);
		if (!bar)
			return;

		if (floating)
		{
			// Top-left, on the empty band just below the "Shop" title / orange stripe.
			FrameSlot.SetAnchorMin(bar, 0, 0);
			FrameSlot.SetAnchorMax(bar, 0, 0);
			FrameSlot.SetSizeToContent(bar, true);
			FrameSlot.SetPos(bar, FFRX_CAT_BAR_X, FFRX_CAT_BAR_Y);
		}

		array<ref FFRX_CategoryDef> cats = FFRX_ArsenalCategory.GetCategories();
		foreach (FFRX_CategoryDef cat : cats)
		{
			ButtonWidget btn = ButtonWidget.Cast(ws.CreateWidget(WidgetType.ButtonWidgetTypeID,
				WidgetFlags.VISIBLE, Color.FromInt(0x33000000), 0, bar));
			if (!btn)
				continue;

			TextWidget txt = TextWidget.Cast(ws.CreateWidget(WidgetType.TextWidgetTypeID,
				WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, Color.FromInt(0xFFCCCCCC), 0, btn));
			if (txt)
			{
				txt.SetText(cat.m_sLabel + "  ");
				txt.SetExactFontSize(16);
			}

			FFRX_ShopCatButtonHandler h = new FFRX_ShopCatButtonHandler();
			h.m_Shop = this;
			h.m_iMask = cat.m_iMask;
			btn.AddHandler(h);

			m_aFFRXCatBtns.Insert(btn);
			m_aFFRXCatTexts.Insert(txt);
			m_aFFRXCatMasks.Insert(cat.m_iMask);
		}

		FFRX_RefreshCatHighlight();
	}

	protected void FFRX_RefreshCatHighlight()
	{
		foreach (int i, TextWidget txt : m_aFFRXCatTexts)
		{
			if (!txt)
				continue;

			if (m_aFFRXCatMasks[i] == m_iFFRXCat)
				txt.SetColor(Color.FromInt(0xFFE58C2E)); // active = orange
			else
				txt.SetColor(Color.FromInt(0xFFCCCCCC)); // inactive = grey
		}
	}
}
