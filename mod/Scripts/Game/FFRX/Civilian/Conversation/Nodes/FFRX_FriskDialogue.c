// FF - REMIXED - PVE
// Civilian dialogue option "Fouiller" (frisk) -- counter-play to the disguised spies (Pillar 4).
// Choice + action node, modelled on the MCD ask-help choice/node. Frisking the civilian you're
// talking to: a hidden weapon -> spy caught early (you have the drop); clean -> innocent offended
// (-trust). Wired into Talk_AmbientCivilian.conf (ROOT choices + a FRISK node).

// --- Choice: ALWAYS available (before greeting too) -- you can choose to greet OR frisk right away.
[BaseContainerProps(), SCR_BaseContainerCustomTitleFields({"m_sNextNodeId"}, "-> %1 (frisk)")]
class MCD_FriskChoice : JWK_ConversationScriptedPlayerChoice
{
	override bool CanBeDisplayed(JWK_ConversationContext context)
	{
		return true;
	}
}

// --- Node: perform the frisk on the target civilian, feedback, back to the menu.
// Frisking WITHOUT even greeting first is rude -> the civilian is extra annoyed (bigger trust hit).
[BaseContainerProps()]
class MCD_FriskCivilianNode : JWK_ConversationScriptedNode
{
	protected static const float FFRX_COLD_FRISK_PENALTY = -6.0; // extra on top of the base innocent hit
	protected static const float FFRX_COLD_FRISK_ANGER_CHANCE = 0.30; // only sometimes does he turn hostile-attitude

	override void Execute(JWK_ConversationContext context, out string outNextNode)
	{
		outNextNode = "ROOT";

		JWK_CivilianCharacterComponent civ;
		JWK_PlayerControllerComponent playerCtrl;
		IEntity playerEntity;
		if (!MCD_CivilianInteractionHelper.ResolveContext(context, civ, playerCtrl, playerEntity))
			return;

		bool wasGreeted = MCD_GreetRegistry.IsGreeted(civ.GetOwner());

		int r = FFRX_DisguisedSpies.FriskCiv(civ.GetOwner());
		if (r == 1)
		{
			if (playerCtrl) playerCtrl.FFRX_SendIntelHint("Arme cachee ! C'est un espion -- il degaine.");
			return;
		}

		// Innocent. Frisking someone cold (without even a hello) offends them more.
		if (!wasGreeted)
		{
			FFRX_CivIdentityRegistry.Get().AddTrustForEntity(civ.GetOwner(), FFRX_COLD_FRISK_PENALTY);
			// He grumbles most of the time; only sometimes does he really take it badly (attitude flip).
			if (Math.RandomFloat01() < FFRX_COLD_FRISK_ANGER_CHANCE)
			{
				civ.SetResistanceAttitude_S(JWK_EResistanceAttitude.NEGATIVE);
				if (playerCtrl) playerCtrl.FFRX_SendIntelHint("Tu le fouilles sans meme le saluer -- il le prend tres mal (-confiance).");
			}
			else
			{
				if (playerCtrl) playerCtrl.FFRX_SendIntelHint("Tu le fouilles sans meme le saluer -- il grogne (-confiance).");
			}
		}
		else
		{
			if (playerCtrl) playerCtrl.FFRX_SendIntelHint("Civil non arme. Le fouiller pour rien l'a offense (-confiance).");
		}
	}
}
