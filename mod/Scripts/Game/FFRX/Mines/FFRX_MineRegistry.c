// FF - REMIXED - PVE
// REMIXED-owned mine registry (ported from FFMI, which REMIXED no longer depends on -- REMIXED
// now uses ACE Explosives mines with our own auto-placement + faction immunity). Tracks the mines
// WE place (so the faction-immune trigger only applies to ours, not player-placed ACE mines) and
// the minefield centers (revealed by the intel system).

class FFRX_MineRegistry
{
	// Mines placed by REMIXED (key = mine EntityID).
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : Enfusion les hisse tous dans
	// UNE fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko
	// deborde en "Too many instructions per function" sur des fichiers innocents. Un
	// `static = new ...` coute ~6 unites sur les ~1000 disponibles pour l'ensemble des
	// mods (les `const` sont gratuits). On construit donc a la premiere utilisation.
	// Cf. memoire `enfusion-script-compile-ceiling`.
	private static ref map<EntityID, bool> s_mMines;

	private static map<EntityID, bool> Mines()
	{
		if (!s_mMines)
			s_mMines = new map<EntityID, bool>();

		return s_mMines;
	}

	static void Register(IEntity mine)
	{
		if (!mine) return;
		Mines().Set(mine.GetID(), true);
	}

	static void Unregister(IEntity mine)
	{
		if (!mine) return;
		Mines().Remove(mine.GetID());
	}

	static bool IsFFRXMine(IEntity mine)
	{
		if (!mine) return false;
		return Mines().Contains(mine.GetID());
	}

	// --- Minefield centers (defensive AP zones) -- revealed by FFRX_IntelSystem.TipNearestMinefield.
	private static ref array<vector> s_aMinefields;

	private static array<vector> Fields()
	{
		if (!s_aMinefields)
			s_aMinefields = new array<vector>();

		return s_aMinefields;
	}

	static void RegisterMinefield(vector center)
	{
		Fields().Insert(center);
	}

	static void GetMinefieldCenters(notnull array<vector> outCenters)
	{
		foreach (vector c : Fields())
			outCenters.Insert(c);
	}

	static void ClearMinefields()
	{
		Fields().Clear();
	}
}
