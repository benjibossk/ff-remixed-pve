[BaseContainerProps()]
modded class JWK_ConversationConvertCivilianChoice
{
	override bool CanBeDisplayed(JWK_ConversationContext context)
	{
		if (!MCD_GreetRegistry.IsGreeted(context.GetTarget().GetOwner()))
			return false;

		JWK_CivilianCharacterComponent civ = JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(
			context.GetTarget().GetOwner()
		);
		if (!civ) return false;
		if (civ.GetResistanceAttitude_S() == JWK_EResistanceAttitude.NEGATIVE)
			return false;

		return super.CanBeDisplayed(context);
	}
}
