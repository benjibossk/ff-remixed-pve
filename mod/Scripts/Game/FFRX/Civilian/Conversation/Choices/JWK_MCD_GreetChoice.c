// Choice "Greet" — visible UNIQUEMENT tant que le civil n'a pas encore été salué
// dans cette conversation. Une fois salué, la réaction est jouée et les vraies
// options (Convert / Extort / AskPresence) prennent le relais → re-saluer ne sert
// à rien, on cache donc l'option.

[BaseContainerProps(), SCR_BaseContainerCustomTitleFields({"m_sNextNodeId"}, "-> %1 (greet)")]
class MCD_GreetChoice : JWK_ConversationScriptedPlayerChoice
{
	override bool CanBeDisplayed(JWK_ConversationContext context)
	{
		if (MCD_GreetRegistry.IsGreeted(context.GetTarget().GetOwner()))
			return false;
		return super.CanBeDisplayed(context);
	}
}
