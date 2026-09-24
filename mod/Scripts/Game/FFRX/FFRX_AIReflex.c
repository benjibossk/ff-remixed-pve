// FF - REMIXED - PVE : reactivite de l'IA au tir.
//
// CREDITS -- les deux leviers ci-dessous ont ete identifies en lisant "AIReflexFire"
// de cjcn (Workshop 6A34762D41CD50CC). Le mod n'est pas une dependance : il applique des
// valeurs figees a toute l'IA, alors que nous voulons des curseurs reglables en jeu et un
// ciblage des ennemis la ou c'est possible. Le code ci-dessous est le notre, mais le merite
// d'avoir trouve ces deux defauts revient a cjcn.
//
// Voir aussi FFRX_AIDifficulty.c (credits CRX Enfusion A.I. / ATiM-).
//
// ---------------------------------------------------------------------------------------
// LEVIER 1 -- temps de visee (SCR_AIWeaponTypeHandlingConfig)
//
//   m_fBaseStabilizationTime  defaut 0.4 s  : temps de stabilisation avant CHAQUE tir
//   m_fBaseRejectionTime      defaut 1.0 s  : delai avant d'abandonner une tentative de tir
//
// C'est different du delai de FFRX_AIDifficulty (SCR_AIAttackBehavior.InitWaitTime), qui
// n'intervient qu'UNE fois, a l'entree en combat. Ces deux valeurs-ci s'appliquent en boucle
// a chaque tir : c'est la principale raison pour laquelle l'IA parait molle une fois engagee.
//
// Portee : le config est partage, donc le reglage agit sur TOUTE l'IA (alliee comprise).
// C'est assume - nos IA alliees sont aussi molles que les ennemies.
//
// LEVIER 2 -- reflexe au contact
//
// En deplacement, le comportement de mouvement l'emporte sur le comportement d'attaque : une
// IA qui croise un ennemi a bout portant continue de courir au lieu de tirer. On donne un
// score ecrasant a l'attaque et on met le mouvement a zero quand un ennemi vient d'etre vu
// de tres pres. Reserve aux ennemis (FFRX_IsEnemyAI).
// ---------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Remembers the authored values so the multiplier stays idempotent when re-applied live.
// ⚠️ L'ATTRIBUT DE CLASSE DOIT ETRE REDECLARE.
//
// La classe d'origine porte :
//   [BaseContainerProps(configRoot: true), SCR_BaseContainerCustomTitleEnum(EWeaponType, "m_eWeaponType")]
//
// Un `modded class` qui l'omet PERD la deserialisation de config du type. Le compilateur
// ne le dit pas franchement : il rend une cinquantaine d'erreurs
// "Too many instructions per function" et "Incompatible parameter" sur des fichiers du
// JEU DE BASE et de FF (JWK_ConvoyAIDeployer, JWK_ShopContext...), puis
// "Can't compile Game script module!". Aucune ne cite ce fichier -- diagnostic
// totalement trompeur (cf. la regle generale dans le skill freedom-fighters-modding :
// toujours redeclarer un [BaseContainerProps]/[ComponentEditorProps] de classe quand on
// la mod).
[BaseContainerProps(configRoot: true), SCR_BaseContainerCustomTitleEnum(EWeaponType, "m_eWeaponType")]
modded class SCR_AIWeaponTypeHandlingConfig
{
	float m_fFFRX_AuthoredStabilization = -1;
	float m_fFFRX_AuthoredRejection     = -1;
}

//------------------------------------------------------------------------------------------------
// ⚠️ ATTRIBUT DE CLASSE REDECLARE -- NE PAS RETIRER.
// Un `modded class` qui omet l'attribut de l'original perd sa deserialisation.
// Le compilateur ne le dit PAS : il rend des dizaines de "Too many instructions
// per function" et "Incompatible parameter" sur des fichiers du JEU DE BASE et de
// FF (JWK_ConvoyAIDeployer, JWK_ShopContext...), aucun ne citant ce fichier, puis
// "Can't compile Game script module!". Panne du dedie le 2026-09-15.
[ComponentEditorProps(category: "GameScripted/AI", description: "Component for utility AI system calculations")]
modded class SCR_AIConfigComponent
{
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		FFRX_ApplyAimSpeedAll();
	}

	//------------------------------------------------------------------------------------------------
	override SCR_AIWeaponTypeHandlingConfig GetWeaponTypeHandlingConfig(EWeaponType weaponType)
	{
		SCR_AIWeaponTypeHandlingConfig cfg = super.GetWeaponTypeHandlingConfig(weaponType);
		FFRX_ApplyAimSpeed(cfg);
		return cfg;
	}

	//------------------------------------------------------------------------------------------------
	void FFRX_ApplyAimSpeedAll()
	{
		if (m_aWeaponTypeHandlingConfig)
		{
			foreach (SCR_AIWeaponTypeHandlingConfig cfg : m_aWeaponTypeHandlingConfig)
				FFRX_ApplyAimSpeed(cfg);
		}

		FFRX_ApplyAimSpeed(m_DefaultWeaponTypeHandlingConfig);
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_ApplyAimSpeed(SCR_AIWeaponTypeHandlingConfig cfg)
	{
		if (!cfg)
			return;

		// Snapshot the authored values once; everything after is derived from them, so
		// changing the slider at runtime never compounds.
		if (cfg.m_fFFRX_AuthoredStabilization < 0)
		{
			cfg.m_fFFRX_AuthoredStabilization = cfg.m_fBaseStabilizationTime;
			cfg.m_fFFRX_AuthoredRejection     = cfg.m_fBaseRejectionTime;
		}

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return;

		float factor = cache.m_fFFRX_AimSpeed;
		cfg.m_fBaseStabilizationTime = cfg.m_fFFRX_AuthoredStabilization * factor;
		cfg.m_fBaseRejectionTime     = cfg.m_fFFRX_AuthoredRejection * factor;
	}
}

//------------------------------------------------------------------------------------------------
//! Shared test: an enemy AI that has just seen a hostile at point-blank range.
class FFRX_Reflex
{
	static const float TARGET_FRESH_S = 3.0;   //!< older sightings do not count as "contact"
	static const float ATTACK_SCORE_BONUS = 1300.0;

	//------------------------------------------------------------------------------------------------
	static bool HasCloseContact(SCR_AIUtilityComponent utility)
	{
		if (!utility)
			return false;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache || cache.m_fFFRX_CqbReflex <= 0)
			return false;

		SCR_AICombatComponent combat = utility.m_CombatComponent;
		if (!combat || !combat.FFRX_IsEnemyAI())
			return false;

		// Garde-fous repris d'AIReflexFire 0.1.5 (cjcn), qui les a ajoutes apres coup --
		// notre premiere version, calquee sur la 0.1.4, avait les memes defauts :
		//
		// 1) Ordre de CESSEZ-LE-FEU du groupe : sans ce test, le reflexe de contact force
		//    l'IA a ouvrir le feu malgre l'ordre. C'est le plus genant des trois : une
		//    embuscade en HOLD_FIRE se declenchait toute seule des qu'on passait a 15 m.
		//    Lecture via FFRX_SafeCombatMode : le GetCombatMode() du jeu de base deref
		//    GetGroupUtilityComponent() sans le tester et jette une VM Exception
		//    ('m_eCombatModeActual') quand le groupe est en cours de spawn/despawn.
		//    Etat illisible -> on n'active pas le reflexe.
		EAIGroupCombatMode mode;
		if (!FFRX_AssaultDirector.FFRX_SafeCombatMode(combat.GetAiAgent(), mode))
			return false;

		if (mode == EAIGroupCombatMode.HOLD_FIRE)
			return false;

		// 2) Unites SANS arme a feu : on modde des classes d'IA globales, donc tout ce qui
		//    partage cette IA est touche. Une unite au corps-a-corps se retrouverait figee
		//    a distance de contact, ni assez pres pour frapper, ni capable de tirer.
		if (combat.GetCurrentWeaponType() == EWeaponType.WT_NONE)
			return false;

		BaseTarget target = combat.GetLastSeenEnemy();
		if (!target || target.GetTimeSinceSeen() > TARGET_FRESH_S)
			return false;

		float dist = target.GetDistance();
		return dist > 0 && dist <= cache.m_fFFRX_CqbReflex;
	}
}

// NOTE: the matching half of this -- SCR_AIAttackBehavior.CustomEvaluate, which raises the
// attack score at contact range -- lives in FFRX_AIDifficulty.c. Enforce does not allow the
// same class to be re-declared as `modded` twice inside one addon, and that file already
// mods SCR_AIAttackBehavior for the reaction delay.

//------------------------------------------------------------------------------------------------
//! Move orders stand down when an enemy is at contact range. SCR_AIMoveInFormationBehavior does NOT derive from
//! SCR_AIMoveBehaviorBase, so both need the same treatment.
modded class SCR_AIMoveBehaviorBase
{
	override float CustomEvaluate()
	{
		if (FFRX_Reflex.HasCloseContact(m_Utility))
			return 0;

		return super.CustomEvaluate();
	}
}

modded class SCR_AIMoveInFormationBehavior
{
	override float CustomEvaluate()
	{
		if (FFRX_Reflex.HasCloseContact(m_Utility))
			return 0;

		return super.CustomEvaluate();
	}
}

// ====================================================================================================
// Riposte immediate a un tir recu.
//
// Vanilla (SCR_AIDangerReaction_DamageTaken) :
//     if (dangerEvent.GetVictim() != utility.m_OwnerEntity) return false;
//     utility.m_SectorThreatFilter.OnDamageTaken(shooterPos);
//
// Deux problemes :
//  1. LA VICTIME D'UN TIR SUR UN VEHICULE EST LE VEHICULE, pas ses occupants. Un char qui
//     encaisse une roquette ne transmet donc RIEN a son equipage : personne n'est prevenu,
//     l'equipage continue tranquillement -- c'est le "il attend sans rien faire".
//  2. Meme quand la reaction passe, elle se contente d'alimenter un secteur de menace. Ca
//     n'oriente pas l'IA vers le tireur et ca ne la fait pas passer en combat.
//
// On corrige les deux : les occupants d'un vehicule touche sont traites comme victimes, on
// tourne le regard (et donc la tourelle) vers le tireur, et on force l'etat de menace au
// maximum pour que l'IA bascule immediatement en combat -- avec, au passage, le facteur de
// perception d'alerte du moteur (x3), ce qui accelere l'acquisition du tireur.
//
// On NE touche PAS aux cibles assignees par le groupe (SetAssignedTargets) : ce serait ecraser
// la coordination de l'escouade pour un gain incertain.
// ====================================================================================================

// Meme regle que plus haut : la classe d'origine porte [BaseContainerProps()], il faut
// le redeclarer sous peine de casser sa deserialisation (et de relancer la meme cascade
// d'erreurs trompeuses sur des fichiers du jeu de base).
[BaseContainerProps()]
modded class SCR_AIDangerReaction_DamageTaken
{
	override bool PerformReaction(notnull SCR_AIUtilityComponent utility, notnull SCR_AIThreatSystem threatSystem, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache || cache.m_fFFRX_ReturnFire < 0.5 || !dangerEvent)
			return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);

		IEntity owner = utility.m_OwnerEntity;
		IEntity victim = dangerEvent.GetVictim();
		if (!owner || !victim)
			return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);

		// Us, or the vehicle we are riding in.
		bool concerns = (victim == owner) || (owner.GetRootParent() == victim);
		if (!concerns)
			return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);

		IEntity shooter = dangerEvent.GetObject();
		if (!shooter)
			return super.PerformReaction(utility, threatSystem, dangerEvent, dangerEventCount);

		vector shooterPos = shooter.GetOrigin();

		utility.m_SectorThreatFilter.OnDamageTaken(shooterPos);

		// Turn towards the shooter. For a gunner this traverses the turret.
		if (utility.m_LookAction)
			utility.m_LookAction.LookAt(shooterPos, SCR_AILookAction.PRIO_ENEMY_TARGET);

		// Straight to "threatened" so the AI drops whatever it was doing and fights.
		float suppression, shotsFired, injury, endangered;
		threatSystem.GetThreatValues(suppression, shotsFired, injury, endangered);
		threatSystem.SetThreatValues(suppression, shotsFired, injury, 1.0);

		return true;
	}
}
