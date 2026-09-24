// FF - REMIXED - PVE -- guard against an early IsBattleActive_S() call.
//
// SYMPTOM (one VM exception per settlement, every startup):
//   NULL pointer to instance -- JWK_BattleSubjectComponent::IsBattleActive_S
//   JWK_BattleSubjectComponent.c:136  <- FF core
//   FFRO_FIATownReinforcement.c:144 OnPostInit  <- Reoccupation 5.x
//
// CAUSE: FF's IsBattleActive_S() does
//     JWK.GetBattleManager().GetController()
// with no null guard on the manager. Reoccupation 5.0.0 started calling it from
// JWK_TownMilitaryActivityComponent.OnPostInit, which runs BEFORE the battle
// manager exists -> dereferencing null.
//
// Not our bug, but it aborts Reoccupation's OnPostInit right before
// FFRO_UpdateFIAManpowerFloor(), so the FIA manpower floor is not applied at
// init (it recovers later via SetPopulation_S / GetDesiredPatrolsCount).
//
// FIX: guard the manager in FF's own method rather than patching the caller.
// One choke point, fixes every early caller (present and future), and stays
// valid whatever Reoccupation does next. No behaviour change once the manager
// exists: no manager can only mean no battle.
//
// NOTE: GetAttackingFaction_S() (same file, just below) has the identical
// missing guard. Left alone -- nothing calls it early today. Worth reporting
// both upstream to Johnny Kerner.
//
// NOTE: ASCII only (Enforce compiler desyncs on UTF-8 accents).

modded class JWK_BattleSubjectComponent
{
	override bool IsBattleActive_S()
	{
		JWK_BattleManagerComponent battleManager = JWK.GetBattleManager();
		if (!battleManager)
			return false; // too early: no manager means no battle

		return super.IsBattleActive_S();
	}
}
