// FF - REMIXED - PVE
// De-solo-ify: force FF into SANDBOX mode (Story Mode OFF) permanently.
//
// FF's "Story Mode" checkbox (New Game menu, default ON) is the campaign/sandbox
// toggle. Its only gameplay effect is in JWK_QuestManagerComponent.NewGameStart_S:
//   storyMode ON  -> autostarts the scripted onboarding chain
//                    (INTRO_RESISTANCE -> POSTERS -> DOGTAGS -> FOB)
//   storyMode OFF -> autostarts the "SANDBOX" quest and marks those 4 intro
//                    quests as already completed (skips the solo tutorial).
// The repeatable jobs (ClearCamp, HarassMilbase, KillHVT, DeliverItem, Jailbreak)
// are unaffected either way.
//
// For a coop PVE conversion we never want the solo-campaign onboarding, so we
// pin IsStoryMode() to false regardless of what the launch UI / autostart config
// says. This is the only consumer of the flag, so forcing it is side-effect-free.
modded class JWK_GameConfigComponent
{
	override bool IsStoryMode()
	{
		return false;
	}
}
