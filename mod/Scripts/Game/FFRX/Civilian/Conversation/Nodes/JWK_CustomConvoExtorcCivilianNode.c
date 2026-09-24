[BaseContainerProps()]
modded class JWK_ConversationExtortCivilianNode
{
	override void Execute(JWK_ConversationContext context, out string outNextNode)
	{
		Print("[FF][MCD] ExtortNode.Execute CALLED");

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

		JWK_EResistanceAttitude attitudeBefore = civ.GetResistanceAttitude_S();

		if (attitudeBefore != JWK_EResistanceAttitude.NEGATIVE)
		{
			civ.SetResistanceAttitude_S(JWK_EResistanceAttitude.NEGATIVE);
			MCD_CivilianInteractionHelper.AlterNearestHeartsAndMinds(civ, 0, 1, true, JWK_EOverTimeModifier.THREAT_CIVILIAN_EXTORTED);
		}

		// RPG trust: extorting this civilian damages your relationship with them (persistent).
		FFRX_CivIdentityRegistry.Get().AddTrustForEntity(civ.GetOwner(), -15.0);

		JWK_GameSettingsCache settings = JWK.GameSettingsCache();

		// A distrustful civilian is far likelier to fight back when extorted (base % x inverse trust).
		float hostileChance = settings.m_fMCD_ExtortHostileChance * FFRX_CivIdentityRegistry.Get().TrustScaleNeg(civ.GetOwner());
		if (JWK.Random.RandFloat01() < hostileChance)
		{
			if (MCD_CivilianInteractionHelper.TurnMilitarilyHostile(civ))
			{
				MCD_CivilianInteractionHelper.GiveRandomHostileWeapon(civ.GetOwner());
				FFRX_CivIdentityRegistry.Get().AddTrustForEntity(civ.GetOwner(), -25.0);
			}
		}

		int money = civ.GetMoney_S();

		if (money == 0)
		{
			if (playerCtrl) playerCtrl.ShowFeedback(JWK_EFeedback.CIVILIAN_EXTORTION_NO_MONEY);
			outNextNode = m_sFailureNode;
		}
		else
		{
			JWK.GetPlayerProfile(context.GetPlayer().GetOwnerPlayerId()).AddMoney_S(money);
			civ.SetMoney_S(0);
			if (playerCtrl) playerCtrl.ShowFeedback(JWK_EFeedback.CIVILIAN_EXTORTION_EXTORTED);
			outNextNode = m_sSuccessNode;
		}

		MCD_CivilianInteractionHelper.ApplyHeat(playerEntity, JWK_WantedHeatComponent.HEAT_CIV_EXTORTION);
		MCD_CivilianInteractionHelper.CheckAndApplyEnemyAlert("EXTORT", civ, playerEntity);
		MCD_CivilianInteractionHelper.TryCallMilitaryPolice_S(civ, playerEntity);
	}
}
