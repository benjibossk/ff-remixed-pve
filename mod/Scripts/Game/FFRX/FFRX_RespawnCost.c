// FF - REMIXED - PVE
// Respawn economy: cheap respawns so the campaign doesn't end too fast (defeat =
// "can no longer spawn", the vanilla FF game-over), with an anti-suicide penalty.
//
// Rule (Benji): a resource (FOB/MOB supplies) respawn costs 1, BUT if the player
// already respawned less than 3 minutes ago (rapid deaths / suicide farming) it
// costs the penalty instead. Reconnection is unaffected: it restores the saved
// character via LoadCharacter and never goes through PlayerAboutToRespawn, so it
// never charges supplies.
//
// FF charges via JWK_ResourceRespawnLocationComponent.GetChargedValue(), used for
// BOTH the availability check (HasResources) and the actual charge (TakeResources).
// GetChargedValue() has no player parameter, so we stash the requesting player id
// just before it runs (respawn is processed sequentially on the server).
class FFRX_RespawnCost
{
	static const float SUICIDE_WINDOW_MS = 180000; // 3 min
	static const int   CHEAP_COST        = 1;      // normal respawn price
	static const int   SUICIDE_COST       = 20;    // price if you re-died < 3 min ago

	// Set right before GetChargedValue() is evaluated (see the modded overrides).
	static int s_CurrentPlayerId;

	// Server-side: last time each player was charged for a respawn.
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	static ref map<int, float> s_LastRespawn;

	static map<int, float> LastRespawn()
	{
		if (!s_LastRespawn)
			s_LastRespawn = new map<int, float>();

		return s_LastRespawn;
	}

	static int CostFor(int playerId)
	{
		if (playerId <= 0) return CHEAP_COST;

		float last;
		if (LastRespawn().Find(playerId, last)) {
			float now = GetGame().GetWorld().GetWorldTime();
			if (now - last < SUICIDE_WINDOW_MS) return SUICIDE_COST;
		}

		return CHEAP_COST;
	}

	static void MarkRespawn(int playerId)
	{
		if (playerId <= 0) return;
		LastRespawn().Set(playerId, GetGame().GetWorld().GetWorldTime());
	}
}

modded class JWK_ResourceRespawnLocationComponent
{
	// The real per-player respawn price (cheap, or the anti-suicide penalty).
	override int GetChargedValue()
	{
		return FFRX_RespawnCost.CostFor(FFRX_RespawnCost.s_CurrentPlayerId);
	}

	override bool CanPlayerRespawn(
		SCR_PlayerController controller,
		out JWK_EPlayerRespawnUnavailableReason outReason
	) {
		if (controller) FFRX_RespawnCost.s_CurrentPlayerId = controller.GetPlayerId();
		return super.CanPlayerRespawn(controller, outReason);
	}

	override bool PlayerAboutToRespawn(SCR_PlayerController controller)
	{
		if (controller) FFRX_RespawnCost.s_CurrentPlayerId = controller.GetPlayerId();

		bool ok = super.PlayerAboutToRespawn(controller);

		// Record the respawn time only once actually charged/committed, so the
		// next respawn within the window pays the penalty.
		if (ok && controller) FFRX_RespawnCost.MarkRespawn(controller.GetPlayerId());

		return ok;
	}
}
