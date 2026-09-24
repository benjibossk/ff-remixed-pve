[BaseContainerProps()]
class MCD_ConvoAskEnemyPresenceCivilianNode : JWK_ConversationScriptedNode
{
	[Attribute(desc: "Node reached when civilian confirms enemies are currently nearby.")]
	string m_sNearbyNode;

	[Attribute(desc: "Node reached when civilian says the area is clear.")]
	string m_sClearNode;

	[Attribute(desc: "Node reached when civilian refuses to answer.")]
	string m_sRefusedNode;

	override void Execute(JWK_ConversationContext context, out string outNextNode)
	{
		Print("[FF][MCD] AskPresenceNode.Execute CALLED");

		JWK_CivilianCharacterComponent civ;
		JWK_PlayerControllerComponent playerCtrl;
		IEntity playerEntity;
		if (!MCD_CivilianInteractionHelper.ResolveContext(context, civ, playerCtrl, playerEntity))
			return;

		// Anti-spam: a civilian answers the presence question only ONCE. Re-asking
		// (spamming the wheel) just gets the refusal -- no fresh roll, no farmed intel.
		if (MCD_PresenceRegistry.HasAsked(civ.GetOwner()))
		{
			outNextNode = m_sRefusedNode;
			return;
		}
		MCD_PresenceRegistry.SetAsked(civ.GetOwner());

		JWK_GameSettingsCache settings = JWK.GameSettingsCache();
		JWK_EResistanceAttitude attitude = civ.GetResistanceAttitude_S();
		bool willShare = false;

		switch (attitude)
		{
			case JWK_EResistanceAttitude.POSITIVE:
				willShare = true;
				break;

			case JWK_EResistanceAttitude.NEGATIVE:
			{
				float reportRoll = JWK.Random.RandFloat01();
				bool willReport = (reportRoll < settings.m_fMCD_PresenceReportChance);
				Print("[FF][MCD] AskPresenceNode: NEGATIVE | willReport=" + willReport);

				if (willReport)
				{
					MCD_CivilianInteractionHelper.ApplyHeat(playerEntity, JWK_WantedHeatComponent.HEAT_CONVINCE_ATTEMPT);
					MCD_CivilianInteractionHelper.CheckAndApplyEnemyAlert("ASK_PRESENCE_NEGATIVE", civ, playerEntity);
					MCD_CivilianInteractionHelper.TryCallMilitaryPolice_S(civ, playerEntity);
				}

				willShare = false;
				break;
			}

			default:
			{
				float shareRoll = JWK.Random.RandFloat01();
				willShare = (shareRoll < settings.m_fMCD_PresenceNeutralShareChance);
				Print("[FF][MCD] AskPresenceNode: NEUTRAL | willShare=" + willShare);
				break;
			}
		}

		if (!willShare)
		{
			outNextNode = m_sRefusedNode;
			return;
		}

		// Phase 2 intel: a sharing civilian occasionally tips enemy activity as GRID
		// coords (text only). How readily they share now depends on their TRUST in you
		// (base % x trust factor) -- a civilian who likes you talks more. See FFRX_IntelSystem.
		float tipChance = FFRX_IntelSystem.TIP_CHANCE * FFRX_CivIdentityRegistry.Get().TrustScalePos(civ.GetOwner());
		if (JWK.Random.RandFloat01() < tipChance)
		{
			int tipPlayerId = playerCtrl.GetOwnerPlayerId();
			// Sometimes the civilian points you at an enemy OFFICER -> a hunt (KillHVT job).
			// Otherwise: reveal an active DARC mission, else the nearest checkpoint.
			bool launchedHvt = JWK.Random.RandFloat01() < FFRX_IntelSystem.HVT_TIP_CHANCE
				&& FFRX_IntelSystem.LaunchKillHvt(playerEntity);
			if (!launchedHvt && !FFRX_IntelSystem.TipRandomMission(tipPlayerId))
			{
				// Sometimes the civilian warns about a nearby enemy MINEFIELD (drops a map
				// marker) instead of pointing at a checkpoint.
				bool tippedMines = JWK.Random.RandFloat01() < FFRX_IntelSystem.MINE_TIP_CHANCE
					&& FFRX_IntelSystem.TipNearestMinefield(tipPlayerId, playerEntity.GetOrigin(), FFRX_IntelSystem.TIP_MAX_DIST);
				if (!tippedMines)
					FFRX_IntelSystem.TipNearestCheckpoint(
						tipPlayerId,
						playerEntity.GetOrigin(),
						FFRX_IntelSystem.TIP_MAX_DIST
					);
			}
		}

		bool enemiesNearby = MCD_CivilianInteractionHelper.HasEnemySoldiersNearby(
			civ.GetOwner(), playerEntity, 150.0
		);
		Print("[FF][MCD] AskPresenceNode: spatial check | enemiesNearby=" + enemiesNearby);

		if (enemiesNearby)
		{
			MCD_CivilianInteractionHelper.ApplyHeat(playerEntity, JWK_WantedHeatComponent.HEAT_CONVINCE_ATTEMPT);
			MCD_CivilianInteractionHelper.CheckAndApplyEnemyAlert("ASK_PRESENCE_NEARBY", civ, playerEntity);
			outNextNode = m_sNearbyNode;
		}
		else
		{
			outNextNode = m_sClearNode;
		}
	}
}
