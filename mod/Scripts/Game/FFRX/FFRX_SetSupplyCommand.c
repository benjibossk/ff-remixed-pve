// FF - REMIXED - PVE
// Admin chat command "#setsupply <n>": set the supply level of the NEAREST FF
// building/vehicle that has a logistics storage (FOB, factory, depot, ...).
//
// Why a command and not the Zeus edit slider: FF replaces the Game Master EDIT
// attribute list with its own (only LockState/Money/Heat -- no supplies), so the
// vanilla "Supplies" slider isn't evaluated on FF entities, and adding one to FF's
// list risks the same shared-config collision as the settings menu. A server command
// is collision-free and reliable.
//
// The amount is clamped to the storage's capacity (a bare FOB with no depot has
// capacity 0 -> nothing to set; build/keep a LogisticsStorage for capacity).
//
// Usage in-game chat (admin):  #setsupply 5000
[BaseContainerProps()]
class FFRX_SetSupplyCommand : ScrServerCommand
{
	override string GetKeyword() { return "setsupply"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		// argv[0] is the keyword ("setsupply"); the amount is argv[1].
		if (argv.Count() < 2)
			return ScrServerCmdResult("Usage: #setsupply <montant>", EServerCmdResultType.ERR);

		int amount = argv[1].ToInt();
		if (amount < 0) amount = 0;

		IEntity adminEnt = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!adminEnt)
			return ScrServerCmdResult("Incarne un personnage pres du batiment cible.", EServerCmdResultType.ERR);
		vector pos = adminEnt.GetOrigin();

		JWK_LogisticsStorageControllerComponent best = FindNearestStorage(pos);
		if (!best || !best.GetOwner())
			return ScrServerCmdResult("Aucun batiment de stockage (avec capacite) a proximite.", EServerCmdResultType.ERR);

		int max = best.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES);
		int cur = best.GetResources(JWK_ELogisticsResourceType.SUPPLIES);

		int applied = amount;
		if (max > 0 && applied > max) applied = max;

		if (applied > cur)
			best.AddResources(JWK_ELogisticsResourceType.SUPPLIES, applied - cur);
		else if (applied < cur)
			best.TakeResources(JWK_ELogisticsResourceType.SUPPLIES, cur - applied);

		int after = best.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
		float dist = vector.Distance(pos, best.GetOwner().GetOrigin());

		return ScrServerCmdResult(
			string.Format("%1 : supplies %2 -> %3 (max %4, a %5 m)",
				NameOf(best.GetOwner()), cur, after, max, (int)dist),
			EServerCmdResultType.OK
		);
	}

	// Nearest STORAGE building that actually has supply capacity (max > 0). A radius
	// query (not the area-controller index) so it catches physical storage buildings
	// -- and the capacity filter skips a bare FOB (max 0) which is what returned
	// "0 -> 0" before.
	protected ref array<JWK_LogisticsStorageControllerComponent> m_aScan;

	protected JWK_LogisticsStorageControllerComponent FindNearestStorage(vector pos)
	{
		m_aScan = new array<JWK_LogisticsStorageControllerComponent>();
		World world = GetGame().GetWorld();
		if (world)
			world.QueryEntitiesBySphere(pos, 200, ScanStorage, null, EQueryEntitiesFlags.ALL);

		JWK_LogisticsStorageControllerComponent best;
		float bestD = float.MAX;
		foreach (JWK_LogisticsStorageControllerComponent st : m_aScan) {
			if (!st || !st.GetOwner()) continue;
			if (st.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES) <= 0) continue; // need capacity
			float d = vector.Distance(pos, st.GetOwner().GetOrigin());
			if (d < bestD) {
				bestD = d;
				best = st;
			}
		}
		return best;
	}

	protected bool ScanStorage(IEntity e)
	{
		JWK_LogisticsStorageControllerComponent st =
			JWK_CompTU<JWK_LogisticsStorageControllerComponent>.FindIn(e);
		if (st) m_aScan.Insert(st);
		return true; // keep querying
	}

	protected string NameOf(IEntity e)
	{
		if (!e) return "?";
		JWK_NamedLocationComponent nl = JWK_CompTU<JWK_NamedLocationComponent>.FindIn(e);
		if (nl && nl.GetName() != "") return nl.GetName();
		return e.GetName();
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult("Commande joueur uniquement (#setsupply en jeu).", EServerCmdResultType.OK);
	}
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
