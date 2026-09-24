[BaseContainerProps()]
modded class JWK_ConversationConvertCivilianNode
{
	protected static const string JACKPOT_NODE = "RESULT_JACKPOT";

	override void Execute(JWK_ConversationContext context, out string outNextNode)
	{
		Print("[FF][MCD] ConvertNode.Execute CALLED");

		if (!MCD_GreetRegistry.IsGreeted(context.GetTarget().GetOwner()))
		{
			outNextNode = "ROOT";
			return;
		}

		JWK_CivilianCharacterComponent civ;
		JWK_PlayerControllerComponent playerCtrl;
		IEntity playerEntity;
		if (!MCD_CivilianInteractionHelper.ResolveContext(context, civ, playerCtrl, playerEntity))
			return;

		if (civ.IsConvertAttempted_S()) return;
		civ.SetConvertAttempted_S(true);

		JWK_GameSettingsCache settings = JWK.GameSettingsCache();
		JWK_EResistanceAttitude attitude = civ.GetResistanceAttitude_S();

		MCD_CivilianInteractionHelper.ApplyHeat(playerEntity, JWK_WantedHeatComponent.HEAT_CONVINCE_ATTEMPT);
		MCD_CivilianInteractionHelper.CheckAndApplyEnemyAlert("CONVERT", civ, playerEntity);

		if (attitude == JWK_EResistanceAttitude.POSITIVE)
		{
			if (playerCtrl) playerCtrl.ShowFeedback(JWK_EFeedback.CIVILIAN_SUPPORTER_ALREADY_SUPPORTS);
			outNextNode = m_sNeedlessNode;
			return;
		}

		if (attitude == JWK_EResistanceAttitude.NEGATIVE)
		{
			if (playerCtrl) playerCtrl.ShowFeedback(JWK_EFeedback.CIVILIAN_SUPPORTER_IS_HOSTILE);

			if (JWK.Random.RandFloat01() < settings.m_fMCD_ConvertArmedChance)
				MCD_CivilianInteractionHelper.TurnMilitarilyHostile(civ);

			outNextNode = m_sHostileNode;
			return;
		}

		// NEUTRAL -- outcome now depends on this civilian's TRUST in you (base % x trust factor).
		float convertChance = settings.m_fMCD_ConvertChance * FFRX_CivIdentityRegistry.Get().TrustScalePos(civ.GetOwner());
		float roll = JWK.Random.RandFloat01();
		bool isSuccess = (roll < convertChance);
		Print("[FF][MCD] ConvertNode: roll=" + roll + " chance=" + convertChance + " success=" + isSuccess);

		if (!isSuccess)
		{
			// A distrustful civilian is likelier to turn hostile when you push them.
			float hostileChance = settings.m_fMCD_ConvertHostileChance * FFRX_CivIdentityRegistry.Get().TrustScaleNeg(civ.GetOwner());
			if (JWK.Random.RandFloat01() < hostileChance)
				outNextNode = m_sHostileNode;
			else
				outNextNode = m_sFailureNode;
			return;
		}

		// SUCCESS
		// RPG trust: winning this civilian over builds a strong, lasting relationship.
		FFRX_CivIdentityRegistry.Get().AddTrustForEntity(civ.GetOwner(), 12.0);

		float jackpotRoll = JWK.Random.RandFloat01();
		bool isJackpot = (jackpotRoll < settings.m_fMCD_ConvertJackpotChance);
		Print("[FF][MCD] ConvertNode: jackpot=" + isJackpot);

		if (isJackpot)
		{
			Print("[FF][MCD] ConvertNode: JACKPOT! +3 supporters");
			MCD_CivilianInteractionHelper.AlterNearestHeartsAndMinds(civ, 3, 0);
			if (playerCtrl) playerCtrl.ShowFeedback(JWK_EFeedback.CIVILIAN_SUPPORTER_CONVERTED_OK);
			outNextNode = JACKPOT_NODE;
		}
		else
		{
			MCD_CivilianInteractionHelper.AlterNearestHeartsAndMinds(civ, 1, 0);
			if (playerCtrl) playerCtrl.ShowFeedback(JWK_EFeedback.CIVILIAN_SUPPORTER_CONVERTED_OK);
			outNextNode = m_sSuccessNode;
		}
	}
}
