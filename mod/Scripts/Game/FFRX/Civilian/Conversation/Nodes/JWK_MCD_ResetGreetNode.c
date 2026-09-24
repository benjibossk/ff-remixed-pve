// Reset du GreetRegistry au début de chaque conversation civile.

[BaseContainerProps(), SCR_BaseContainerCustomTitleFields({"m_sNodeId"}, "reset greet: %1")]
class MCD_ResetGreetNode : JWK_ConversationScriptedNode
{
	override void Execute(JWK_ConversationContext context, out string outNextNode)
	{
		IEntity civEntity = context.GetTarget().GetOwner();
		if (civEntity)
		{
			Print("[FF][MCD] ResetGreetNode - resetting greet for: " + civEntity.GetID());
			MCD_GreetRegistry.Reset(civEntity);
		}
	}
}
