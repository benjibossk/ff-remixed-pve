[BaseContainerProps()]
class MCD_ConvoGreetCivilianNode : JWK_ConversationScriptedNode
{
	[Attribute(desc: "Node reached when civilian is a supporter (POSITIVE attitude).")]
	string m_sPositiveNode;

	[Attribute(desc: "Node reached when civilian is neutral.")]
	string m_sNeutralNode;

	[Attribute(desc: "Node reached when civilian is hostile (NEGATIVE attitude).")]
	string m_sNegativeNode;

	protected static const ResourceName EXPLOSION_PREFAB = "{564D57EA34A75775}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Medium.et";

	override void Execute(JWK_ConversationContext context, out string outNextNode)
	{
		Print("[FF][MCD] GreetNode.Execute CALLED");

		JWK_CivilianCharacterComponent civ;
		JWK_PlayerControllerComponent playerCtrl;
		IEntity playerEntity;
		if (!MCD_CivilianInteractionHelper.ResolveContext(context, civ, playerCtrl, playerEntity))
			return;

		MCD_GreetRegistry.SetGreeted(civ.GetOwner(), true);

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		JWK_EResistanceAttitude attitude = civ.GetResistanceAttitude_S();

		// Un civil encore INDÉCIS (ni supporter ni hostile) voit sa réaction tirée
		// au sort à la première salutation, puis PERSISTÉE via l'attitude FF.
		// Les civils déjà POSITIVE/NEGATIVE gardent leur attitude → cohérence
		// entre toutes les conversations (fix Johnny + retours joueurs).
		bool undecided = (attitude != JWK_EResistanceAttitude.POSITIVE
		               && attitude != JWK_EResistanceAttitude.NEGATIVE);
		bool freshlyDecided = false;

		// On ne tire QU'UNE FOIS par civil : si la réaction a déjà été tirée
		// (même un neutre), on ne re-tire pas — sinon spammer "saluer" permettrait
		// de re-tenter sa chance jusqu'à tomber sur amical/hostile.
		if (undecided && cache && !MCD_GreetRegistry.HasRolledGreet(civ.GetOwner()))
		{
			MCD_GreetRegistry.SetRolledGreet(civ.GetOwner());

			float roll     = JWK.Random.RandFloat01();
			float friendly = cache.m_fMCD_GreetFriendlyChance;
			float hostile  = cache.m_fMCD_GreetHostileChance;
			Print("[FF][MCD] GreetNode: roll=" + roll + " friendly=" + friendly + " hostile=" + hostile);

			if (roll < friendly)
			{
				attitude = JWK_EResistanceAttitude.POSITIVE;
				civ.SetResistanceAttitude_S(attitude);
				freshlyDecided = true;
			}
			else if (roll < friendly + hostile)
			{
				attitude = JWK_EResistanceAttitude.NEGATIVE;
				civ.SetResistanceAttitude_S(attitude);
				freshlyDecided = true;
			}
			// sinon : reste neutre — figé par SetRolledGreet, ne re-tirera plus.
		}

		Print("[FF][MCD] GreetNode: attitude=" + attitude + " freshlyDecided=" + freshlyDecided);

		switch (attitude)
		{
			case JWK_EResistanceAttitude.POSITIVE:
				outNextNode = m_sPositiveNode;
				break;

			case JWK_EResistanceAttitude.NEGATIVE:
			{
				// Conséquences hostiles (alerte, bombe, appel MP, hearts&minds)
				// uniquement lors de la réaction initiale, pour ne pas les
				// re-déclencher à chaque nouvelle salutation d'un civil déjà hostile.
				if (freshlyDecided)
				{
					MCD_CivilianInteractionHelper.ApplyHeat(playerEntity, JWK_WantedHeatComponent.HEAT_CONVINCE_ATTEMPT);
					MCD_CivilianInteractionHelper.AlterNearestHeartsAndMinds(civ, 0, 1);
					MCD_CivilianInteractionHelper.CheckAndApplyEnemyAlert("GREET", civ, playerEntity);
					MCD_CivilianInteractionHelper.TryCallMilitaryPolice_S(civ, playerEntity);
					TrySpawnBomb(civ.GetOwner(), cache);
				}
				outNextNode = m_sNegativeNode;
				break;
			}

			default:
				outNextNode = m_sNeutralNode;
				break;
		}
	}

	protected void TrySpawnBomb(IEntity civEntity, JWK_GameSettingsCache cache)
	{
		if (!civEntity || !cache) return;

		float bombChance = cache.m_fMCD_GreetBombChance;
		if (bombChance <= 0.0) return;

		float roll = JWK.Random.RandFloat01();
		Print("[FF][MCD] GreetNode.TrySpawnBomb: chance=" + bombChance + " roll=" + roll);
		if (roll > bombChance) return;

		Resource res = Resource.Load(EXPLOSION_PREFAB);
		if (!res || !res.IsValid()) return;

		EntitySpawnParams spawnParams = new EntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		vector mat[4];
		civEntity.GetTransform(mat);
		spawnParams.Transform = mat;

		GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), spawnParams);
		Print("[FF][MCD] GreetNode.TrySpawnBomb: SPAWNED");
	}
}
