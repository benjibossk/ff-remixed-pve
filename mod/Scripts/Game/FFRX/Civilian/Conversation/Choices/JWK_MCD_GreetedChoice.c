[BaseContainerProps(), SCR_BaseContainerCustomTitleFields({"m_sNextNodeId"}, "-> %1 (greeted only)")]
class MCD_GreetedChoice : JWK_ConversationScriptedPlayerChoice
{
	override bool CanBeDisplayed(JWK_ConversationContext context)
	{
		if (!MCD_GreetRegistry.IsGreeted(context.GetTarget().GetOwner()))
			return false;

		// AskPresence seulement si POSITIVE ou NEUTRAL
		JWK_CivilianCharacterComponent civ = JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(
			context.GetTarget().GetOwner()
		);
		if (!civ) return false;

		return (civ.GetResistanceAttitude_S() != JWK_EResistanceAttitude.NEGATIVE);
	}
}
