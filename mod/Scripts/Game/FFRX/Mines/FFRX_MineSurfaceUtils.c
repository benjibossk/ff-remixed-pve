// FF - REMIXED - PVE
// Surface helpers for mine placement (hard-road detection + burial). Ported from FFMI_SurfaceUtils
// (credit "Kex" for SurfaceIsHardRoad). Self-contained.

class FFRX_MineSurfaceUtils
{
	static const float BURY_DEPTH        = 0.05;  // default / AT (tall mine)
	static const float BURY_DEPTH_AP     = 0.02;  // AP (small flat mine -> barely buried, stays spottable)
	static const float BURIED_THRESHOLD  = 0.015;

	// True if the surface under 'pos' is a hard road/bridge (asphalt, concrete, cobble...).
	static bool SurfaceIsHardRoad(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world) return false;

		pos[1] = world.GetSurfaceY(pos[0], pos[2]);

		TraceParam params = new TraceParam();
		params.Flags = TraceFlags.WORLD;
		params.Start = pos + 0.01 * vector.Up;
		params.End   = pos - 0.01 * vector.Up;
		world.TraceMove(params, null);

		ResourceName material = params.TraceMaterial;
		if (material.IsEmpty()) return false;

		return (material.IndexOf("Road_Asph")     >= 0 ||
		        material.IndexOf("Road_Cobb")     >= 0 ||
		        material.IndexOf("Road_Conc")     >= 0 ||
		        material.IndexOf("Path_Conc")     >= 0 ||
		        material.IndexOf("Bridge_Asph")   >= 0 ||
		        material.IndexOf("Bridge_Conc")   >= 0 ||
		        material.IndexOf("BridgeConc")    >= 0 ||
		        material.IndexOf("ConcreteBridge") >= 0 ||
		        material.IndexOf("Asphalt_Pave")  >= 0 ||
		        material.IndexOf("TilePave")      >= 0 ||
		        material.IndexOf("ConcreteKerb")  >= 0);
	}

	// Placement height: surface, or surface - depth on soft ground when burying is allowed.
	static float ComputePlacementY(vector pos, bool allowBurying, float depth = BURY_DEPTH)
	{
		float surfaceY = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
		if (allowBurying && !SurfaceIsHardRoad(pos))
			return surfaceY - depth;
		return surfaceY;
	}
}
