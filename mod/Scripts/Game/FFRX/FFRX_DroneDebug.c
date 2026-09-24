// FF - REMIXED - PVE
// Diagnostic prints for the DroneAI (NOVA_) enemy drone operators, to see the operator
// lifecycle in the log (does an enemy soldier actually launch a recon / kamikaze drone?).
// The mod itself only prints a couple of internal lines ("No cover found", "Grenade
// dropped!"); we add the key LAUNCH event with the operator type. Server-side.
//
// Remove/comment this file once drones are confirmed working.
modded class NOVA_Drons
{
	override protected IEntity SpawnDroneInAir(vector launchPos)
	{
		IEntity drone = super.SpawnDroneInAir(launchPos);

		string type = "?";
		NOVA_EDronOperatorType t = GetOperatorType();
		if (t == NOVA_EDronOperatorType.RECON)
			type = "RECON (Mavic)";
		else if (t == NOVA_EDronOperatorType.FPV)
			type = "FPV (kamikaze)";

		Print(string.Format("[FFRX][Drone] Operateur %1 -> LANCE un drone (ok=%2) a %3.",
			type, drone != null, launchPos.ToString()), LogLevel.NORMAL);

		return drone;
	}
}
