// FF - More Civilian Discussion
// Adapts the base-game "cache note" item (Campaign_CacheNote_Base.et: notebook mesh +
// SCR_CacheNoteComponent + SCR_CacheNoteUIInfo) to Freedom Fighters.
//
// The base component fills its (replicated) lines from SCR_CacheManagerComponent -- a
// CONFLICT system that FF doesn't have -> in FF the note stays blank. We fill it
// ourselves: on the server, if the Conflict manager left the note empty, write the
// GRID coordinates of a random enemy-controlled location (with random precision -- the
// note is "more or less accurate", D7). The base SCR_CacheNoteUIInfo then shows those
// lines as the item's inspect DESCRIPTION -- no custom action or map circle needed.
//
// Use the base prefab (or a clone) as the FF intel document. ASCII in strings.
[ComponentEditorProps(category: "GameScripted/Inventory", description: "Item component that requests and displays cache locations")]
modded class SCR_CacheNoteComponent
{
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner); // Conflict path (no-op in FF: no SCR_CacheManagerComponent)

		if (!Replication.IsServer())
			return;
		if (!GetNoteLines().IsEmpty())
			return; // Conflict already filled it -> leave as is

		// A radio dispatch collected at a friendly radio site reuses this same item
		// (see FFRX_RadioIntel): it arms the text right before spawning the note, so
		// THIS note carries the intercepted report instead of pointing at a camp.
		string dispatch = FFRX_RadioIntel.ConsumeDocument();
		if (dispatch != "")
		{
			FFRX_FillFromDispatch(dispatch);
			return;
		}

		FFRX_FillFromFF();
	}

	// Writes the intercepted report as note lines. The report is authored for a
	// full-screen card, so we split on newlines and drop empty ones.
	protected void FFRX_FillFromDispatch(string dispatch)
	{
		array<string> raw = {};
		dispatch.Split("\n", raw, false);

		array<string> lines = GetNoteLines();
		foreach (string l : raw)
		{
			string trimmed = l.Trim();
			if (trimmed != "")
				lines.Insert(trimmed);
		}

		if (lines.IsEmpty())
			lines.Insert("Depeche illisible.");

		Replication.BumpMe();
	}

	protected void FFRX_FillFromFF()
	{
		int gridX;
		int gridZ;

		// Spawn a REAL guarded supply cache and point the note at it. If the cache cap
		// is reached (or no enemy known), fall back to a plain coord tip (no cache).
		if (!FFRX_CacheSpawner.SpawnGuardedCache(gridX, gridZ))
		{
			if (!FFRX_IntelSystem.RandomEnemyGrid(gridX, gridZ))
				return;
		}

		array<string> lines = GetNoteLines();
		lines.Insert("Camp ennemi signale");
		lines.Insert(string.Format("Grille %1 - %2", gridX, gridZ));
		Replication.BumpMe();
	}
}
