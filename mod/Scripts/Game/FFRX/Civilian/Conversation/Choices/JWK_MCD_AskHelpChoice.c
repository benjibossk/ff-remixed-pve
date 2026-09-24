// Choice "Demander de l'aide" — visible uniquement pour un civil DÉJÀ salué ET
// AMICAL (POSITIVE). Un civil amical peut lâcher un objet (voir MCD_ConvoAskHelpCivilianNode),
// mais trop insister l'agace et lui fait perdre son amitié.

[BaseContainerProps(), SCR_BaseContainerCustomTitleFields({"m_sNextNodeId"}, "-> %1 (ask help)")]
class MCD_AskHelpChoice : JWK_ConversationScriptedPlayerChoice
{
	override bool CanBeDisplayed(JWK_ConversationContext context)
	{
		if (!MCD_GreetRegistry.IsGreeted(context.GetTarget().GetOwner()))
			return false;

		JWK_CivilianCharacterComponent civ = JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(
			context.GetTarget().GetOwner()
		);
		if (!civ)
			return false;

		return (civ.GetResistanceAttitude_S() == JWK_EResistanceAttitude.POSITIVE);
	}
}
