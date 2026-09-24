// FF - REMIXED - PVE
// Dev command "#spawncache": spawn a guarded enemy-camp cache (JOB_ENEMY_CAMP) and
// report its grid coords -- test the cache system without waiting for a civilian gift.
[BaseContainerProps()]
class FFRX_SpawnCacheCommand : ScrServerCommand
{
	override string GetKeyword() { return "spawncache"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		int gridX;
		int gridZ;
		if (!FFRX_CacheSpawner.SpawnGuardedCache(gridX, gridZ))
			return ScrServerCmdResult("Echec (cap de caches atteint ou aucun slot de camp libre).", EServerCmdResultType.ERR);

		return ScrServerCmdResult(
			string.Format("Camp ennemi spawne pres de la grille %1 - %2 (approche pour le faire streamer).", gridX, gridZ),
			EServerCmdResultType.OK
		);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
