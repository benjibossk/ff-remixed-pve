// Node "Demander de l'aide" — logique patience/don.
// Réservé aux civils amicaux (POSITIVE). Chaque demande consomme de la patience :
//  - patience restante > 0  → chance de recevoir un objet (catalogue ITEM de la faction joueur)
//  - patience épuisée       → le civil se lasse : redevient NEUTRE (perd son amitié) et le dit
[BaseContainerProps()]
class MCD_ConvoAskHelpCivilianNode : JWK_ConversationScriptedNode
{
	[Attribute(desc: "Node reached when the civilian gives an item.")]
	string m_sGiftNode;

	[Attribute(desc: "Node reached when the civilian helps but gives nothing this time.")]
	string m_sNoGiftNode;

	[Attribute(desc: "Node reached when the civilian is fed up and stops supporting.")]
	string m_sAnnoyedNode;

	override void Execute(JWK_ConversationContext context, out string outNextNode)
	{
		Print("[FF][MCD] AskHelpNode.Execute CALLED");

		JWK_CivilianCharacterComponent civ;
		JWK_PlayerControllerComponent playerCtrl;
		IEntity playerEntity;
		if (!MCD_CivilianInteractionHelper.ResolveContext(context, civ, playerCtrl, playerEntity))
			return;

		// Filet de sécurité : le choix n'est censé s'afficher que pour un amical.
		if (civ.GetResistanceAttitude_S() != JWK_EResistanceAttitude.POSITIVE)
		{
			outNextNode = m_sNoGiftNode;
			return;
		}

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		float cost       = 34.0;
		float giftChance = 0.4;
		if (cache)
		{
			cost       = cache.m_fMCD_PatienceCost;
			giftChance = cache.m_fMCD_GiftChance;
		}

		// A civilian who trusts you helps more readily (base % x trust factor).
		giftChance = giftChance * FFRX_CivIdentityRegistry.Get().TrustScalePos(civ.GetOwner());

		float remaining = MCD_PatienceRegistry.Consume(civ.GetOwner(), cost);
		Print("[FF][MCD] AskHelpNode: patience remaining=" + remaining + " (cost=" + cost + ")");

		if (remaining <= 0.0)
		{
			// Trop insisté → le civil se lasse : redevient neutre (perd son amitié).
			// L'option "Demander de l'aide" disparaît (elle exige POSITIVE).
			civ.SetResistanceAttitude_S(JWK_EResistanceAttitude.NEUTRAL);
			// RPG trust: pestering a civilian until they give up costs you their goodwill.
			FFRX_CivIdentityRegistry.Get().AddTrustForEntity(civ.GetOwner(), -5.0);
			Print("[FF][MCD] AskHelpNode: patience epuisee -> civil redevient NEUTRE");
			outNextNode = m_sAnnoyedNode;
			return;
		}

		float roll = JWK.Random.RandFloat01();
		bool gave  = false;
		if (roll < giftChance)
		{
			// Small chance the gift is an INTEL DOCUMENT (cache note with grid coords)
			// rather than a normal item -> a civilian source of intel (Pilier 2).
			if (JWK.Random.RandFloat01() < 0.15)
				gave = MCD_CivilianInteractionHelper.GiveIntelDocument(playerEntity);
			else
				gave = MCD_CivilianInteractionHelper.GiveRandomFactionItem(playerEntity);
		}

		Print("[FF][MCD] AskHelpNode: roll=" + roll + " chance=" + giftChance + " gave=" + gave);
		if (gave)
		{
			// RPG trust: a civilian who helps you warms to you a little.
			FFRX_CivIdentityRegistry.Get().AddTrustForEntity(civ.GetOwner(), 2.0);
			outNextNode = m_sGiftNode;
		}
		else
			outNextNode = m_sNoGiftNode;
	}
}
