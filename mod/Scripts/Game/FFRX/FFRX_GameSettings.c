// FF - REMIXED - PVE
// Nos reglages dans le menu FF : onglet "FF REMIXED - PVE" (escouade par defaut),
// difficulte IA en direct, specialistes AT/AA, jammers, + les reglages MCD replies ici.
//
// HISTORIQUE : ce fichier portait a l'origine la "Ville de depart" (brique C). Le
// reglage a ete RETIRE le 2026-09-08 -- le design est passe a un demarrage sur FOB
// (0 partisan partout, cf. FFRX_DefaultFob / FFRX_SingleStartingTown) et plus aucun
// code ne lisait la valeur : l'admin choisissait une ville sans le moindre effet.
// Le placement se fait desormais en jeu via la commande admin #placefob.
// La cle d'enum FFRX_STARTING_TOWN reste en place (cle de persistance, cf. plus bas).
//
// Injection is done by overriding the config READERS (GetGroups/GetSetting) and
// NOT the component, and by restating [BaseContainerProps(configRoot: true)] on
// the modded config (omitting it nulls the component's m_Config binding ->
// crash). Cache reads defensively ("".ToInt() throws).

class FFRX_Settings
{
	static const string GROUP_NAME = "FF REMIXED - PVE";
}

// New persistent setting keys. Appended (never reorder existing keys).
// Includes the MCD civilian-discussion keys (folded in from the old MCD addon).
modded enum JWK_EGameSetting
{
	FFRX_STARTING_TOWN,
	FFRX_DEFAULT_SQUAD,

	// --- MCD (More Civilian Discussion) settings, folded into FF-REMIXED ---
	MCD_CONVERT_CHANCE,
	MCD_CONVERT_HOSTILE_CHANCE,
	MCD_CONVERT_ARMED_CHANCE,

	MCD_EXTORT_HOSTILE_CHANCE,

	// ASK ENEMY PRESENCE
	MCD_PRESENCE_NEUTRAL_SHARE_CHANCE,
	MCD_PRESENCE_REPORT_CHANCE,

	// GREET - initial civilian reaction to the greeting (random roll)
	MCD_GREET_FRIENDLY_CHANCE,
	MCD_GREET_HOSTILE_CHANCE,

	// GREET - hostile civilian: chance to blow himself up
	MCD_GREET_BOMB_CHANCE,

	// GREET - hostile civilian with enemies nearby: alert + wanted
	MCD_GREET_HOSTILE_ALERT_RADIUS,
	MCD_GREET_HOSTILE_ALERT_HEAT,

	// Hostile civilian: chance to call the military police (QRF)
	MCD_CIV_CALL_MP_CHANCE,

	// CONVERT - jackpot: enthusiastic civilian + 2 supporter friends
	MCD_CONVERT_JACKPOT_CHANCE,

	// ASK HELP - friendly civilian: gift chance + patience cost per request
	MCD_GIFT_CHANCE,
	MCD_PATIENCE_COST,

	// --- FFRX live enemy AI difficulty (skill per force + global perception) ---
	FFRX_AISKILL_REGULAR,
	FFRX_AISKILL_NAVAL,
	FFRX_AISKILL_KLMK,
	FFRX_AISKILL_SPETSNAZ,
	FFRX_AIPERCEPTION,

	// --- Specialistes AT / AA ennemis: priorite et portee d'engagement des vehicules ---
	FFRX_AT_PRIORITY,
	FFRX_AT_RANGE,
	FFRX_ADAPT_STRENGTH,
	FFRX_ADAPT_HALFLIFE,
	FFRX_ADAPT_FLOOR,
	FFRX_SPEC_DRONE,
	FFRX_SPEC_AA,
	FFRX_SPEC_EW,
	FFRX_SPEC_AT,
	FFRX_NIGHT_PERCEPTION,
	FFRX_NVG_PCT,
	FFRX_NVG_ELITE_MULT,
	FFRX_VEST_PCT,

	// --- Niveau des escouades ennemies adapte a l'anciennete des joueurs en face ---
	FFRX_TIER_ADAPT,
	FFRX_TIER_RADIUS,
	FFRX_TIER_XPREF,

	// --- Equipages de vehicules ennemis: tenir la tourelle au lieu de debarquer ---
	FFRX_VEH_CREW_HOLD,

	// --- Delai de riposte quand l'ennemi est surpris (% du delai vanilla) ---
	FFRX_REACTION_DELAY,

	// --- Vitesse de visee avant tir (% du vanilla) + reflexe au contact ---
	FFRX_AIM_SPEED,
	FFRX_CQB_REFLEX,

	// --- Canon de char: obus explosif sur l'infanterie (chance + delai mini) ---
	FFRX_HE_CHANCE,
	FFRX_HE_COOLDOWN,

	// --- Riposte immediate quand on encaisse un tir (equipages de vehicules compris) ---
	FFRX_RETURN_FIRE,

	// --- Jammers (guerre electronique) ---
	FFRX_JAMMER_VEHICLE_PCT,

	// --- Accoutumance a l'arme : bonus de stabilite gagne en portant la meme arme ---
	FFRX_WEAPON_FAMILIARITY_PCT,

	// --- Assaut : part du groupe ennemi qui va chercher le joueur au contact ---
	FFRX_ASSAULT_PCT,
	FFRX_ASSAULT_MIN_DIST,

	// --- Anti-camping : reponse ennemie contre une position tenue trop longtemps ---
	FFRX_ANTICAMP,

	// --- Voitures civiles piegees (guerre asymetrique) ---
	FFRX_BOOBYTRAP_PCT,
	FFRX_BOOBYTRAP_MAX,

	// --- VBIED : voiture piegee ROULANTE (menace mobile, cf. FFRX_VBIED.c) ---
	FFRX_VBIED_PCT,
	FFRX_VBIED_MAX,

	// --- IED disperses sur les points d'interet de la carte (cf. FFRX_IEDScatter.c) ---
	FFRX_IED_SCATTER_MAX,

	// --- Carcasses de vehicules piegees (cf. FFRX_TrappedWrecks.c) ---
	FFRX_TRAPPED_WRECKS_MAX,

	// --- Discipline de soin : l'IA ne soigne pas a decouvert sous le feu ---
	FFRX_HEAL_DISCIPLINE,

	// --- Genie : vitesse de construction des membres de l'escouade du genie ---
	// AJOUTER LES NOUVELLES CLES *A LA FIN*. La valeur de l'enum EST la cle de
	// stockage : inserer au milieu decalerait toutes les suivantes et les
	// sauvegardes reliraient de mauvaises valeurs (cf. l'episode "Ville de depart").
	FFRX_GENIE_BUILD,

	// --- Sante : bonus des membres de l'escouade medicale ---
	FFRX_MEDIC_HEAL,
	FFRX_MEDIC_SAVE,

	// --- Tracantes : signature visuelle du tir joueur ---
	FFRX_TRACER_BOOST,

	// --- Chantier : construire a la pelle au lieu de faire apparaitre le batiment ---
	FFRX_BUILD_SITE_TIME
}

modded class JWK_GameSettingsCache
{
	int m_iFFRXDefaultSquad = 1;

	// --- MCD cached values (folded in) ---
	// CONVERT CIVILIAN
	float m_fMCD_ConvertChance;
	float m_fMCD_ConvertHostileChance;
	float m_fMCD_ConvertArmedChance;
	// EXTORT CIVILIAN
	float m_fMCD_ExtortHostileChance;
	// ASK ENEMY PRESENCE
	float m_fMCD_PresenceNeutralShareChance;
	float m_fMCD_PresenceReportChance;
	// GREET - initial reaction: friendly / hostile probabilities (rest = neutral)
	float m_fMCD_GreetFriendlyChance;
	float m_fMCD_GreetHostileChance;
	// GREET - hostile civilian: chance to blow himself up
	float m_fMCD_GreetBombChance;
	// GREET - hostile civilian with enemies nearby: alert + wanted
	float m_fMCD_GreetHostileAlertRadius;
	float m_fMCD_GreetHostileAlertHeat;
	// Hostile civilian: chance to call the military police (QRF)
	float m_fMCD_CivCallMpChance;
	// CONVERT - jackpot: enthusiastic civilian who brings 2 friends (3 supporters total)
	float m_fMCD_ConvertJackpotChance;
	// ASK HELP - friendly civilian gift chance; patience cost per request
	float m_fMCD_GiftChance;
	float m_fMCD_PatienceCost;

	// --- FFRX AI difficulty: skill 0..100 per force; perception as a factor (1.0 = default) ---
	float m_fFFRX_AISkillRegular  = 70;
	float m_fFFRX_AISkillNaval    = 75;
	float m_fFFRX_AISkillKLMK     = 85;
	float m_fFFRX_AISkillSpetsnaz = 100;
	float m_fFFRX_AIPerception    = 1.4;
	// AT/AA specialists: score bonus added to vehicle/aircraft targets (0 = vanilla),
	// and max distance at which they still consider a vehicle target (vanilla = 700 m).
	float m_fFFRX_ATPriority      = 200;
	float m_fFFRX_ATRange         = 1200;
	// Menace adaptative (cf. FFRX_AdaptiveThreat.c).
	float m_fFFRX_AdaptStrength   = 35;   // pourcent max de groupes renforces ; 0 = systeme coupe
	float m_fFFRX_AdaptHalfLife   = 20;   // minutes ; demi-vie des jauges de zone
	// Socle : part de specialistes meme en zone FROIDE. Sans lui la menace est purement
	// reactive, donc le premier helicoptere du serveur ne rencontre jamais de sol-air.
	float m_fFFRX_AdaptFloor      = 15;   // pourcent ; 0 = purement reactif (ancien comportement)
	// Frequence des equipes specialistes, en pourcent des groupes d'infanterie
	// ennemis produits (cf. FFRX_SpecialistWeights.c). Somme = part totale.
	float m_fFFRX_SpecDrone       = 8;
	float m_fFFRX_SpecAA          = 6;
	float m_fFFRX_SpecEW          = 4;
	float m_fFFRX_SpecAT          = 2;
	// Perception de l'ennemi SANS optique NV la nuit, en %% de sa vue de jour.
	// 100 = pas de malus (systeme neutralise). Cf. FFRX_NightVision.c.
	float m_fFFRX_NightPerception = 55;
	// Part des ennemis equipes d'une optique NV, et multiplicateur pour l'elite.
	float m_fFFRX_NVGPct          = 8;
	float m_fFFRX_NVGEliteMult    = 4;
	// Part des combattants ennemis porteurs d'un gilet suicide (FFRX_SuicideVest.c).
	float m_fFFRX_VestPct         = 3;
	// Adaptation du niveau des escouades a l'anciennete des joueurs (FFRX_SquadTier.c).
	float m_fFFRX_TierAdapt       = 50;
	float m_fFFRX_TierRadius      = 800;
	float m_fFFRX_TierXpRef       = 3000;
	float m_fFFRX_GenieBuild      = 250;  // % de la vitesse normale, pour l'escouade du genie
	float m_fFFRX_MedicHeal       = 200;  // % de regeneration rendue par un soin de l'escouade medicale
	float m_fFFRX_MedicSave       = 35;   // % de chance qu'un medecin ne consomme pas son pansement
	float m_fFFRX_TracerBoost     = 60;   // % de perception en plus pour l'ennemi qui voit une tracante
	float m_fFFRX_BuildSiteTime   = 100;  // % du temps de montage d'un chantier ; 0 = batiment instantane (FF)
	// Enemy vehicle crews: 1 = hold the turret instead of dismounting, 0 = vanilla.
	float m_fFFRX_VehCrewHold     = 1;
	// Multiplier applied to the enemy's pre-fire delay (1.0 = vanilla, 0 = instant).
	float m_fFFRX_ReactionDelay   = 0.5;
	// Multiplier on AI aim stabilization / rejection time (1.0 = vanilla 0.4s / 1.0s).
	float m_fFFRX_AimSpeed        = 0.4;
	// Distance (m) under which a spotted enemy overrides any move order. 0 = off.
	float m_fFFRX_CqbReflex       = 15;
	// Tank cannon vs infantry: chance (%) and minimum seconds between two HE shells. 0 = off.
	float m_fFFRX_HeChance        = 30;
	float m_fFFRX_HeCooldown      = 25;
	// 1 = an AI that takes a hit turns on the shooter at once (vehicle crews included).
	float m_fFFRX_ReturnFire      = 1;
	float m_fFFRX_JammerVehiclePct = 30;   // % of enemy vehicles that carry an active jammer
	// Reduction MAXIMALE du tremblement quand une arme est parfaitement maitrisee.
	// 10 % : volontairement modeste -- un bonus fort creerait deux classes de joueurs,
	// les anciens intouchables et les recrues impuissantes. 0 = mecanique desactivee.
	float m_fFFRX_WeaponFamiliarityPct = 10;

	// Part du groupe ennemi envoyee a l'assaut quand il accroche un joueur a distance.
	// 50 % : la moitie avance pendant que l'autre appuie. Au-dela de ~70 % l'appui
	// disparait et l'assaut devient une charge suicidaire. 0 = mecanique desactivee.
	float m_fFFRX_AssaultPct     = 50;
	// En deca de cette distance on laisse le combat a distance se jouer normalement.
	float m_fFFRX_AssaultMinDist = 80;

	// Anti-camping : 1 = un drone ennemi finit par venir sur une position tenue trop
	// longtemps, 0 = mecanique desactivee.
	float m_fFFRX_AntiCamp = 1;

	// Voitures civiles piegees : chance qu'un vehicule abandonne candidat soit rige,
	// et nombre maximum de pieges actifs en meme temps. 0 % = systeme desactive.
	float m_fFFRX_BoobyTrapPct = 35;
	float m_fFFRX_BoobyTrapMax = 6;

	// VBIED (voiture piegee roulante). Volontairement BAS par defaut : c'est une menace
	// scenarisee, pas un aleas de fond. Une par heure et par joueur, c'est deja marquant.
	float m_fFFRX_VbiedPct = 12;
	float m_fFFRX_VbiedMax = 1;

	// IED poses a l'avance sur les lieux de la carte. 20 sur une carte entiere : on en croise
	// sans que ce soit systematique, ce qui est le but (le danger doit etre CREDIBLE, pas certain).
	float m_fFFRX_IedScatterMax = 20;

	// Carcasses piegees. Le systeme n'en piege de toute facon que ~18 % de celles qu'il trouve :
	// ce plafond est une securite sur les cartes qui en comptent beaucoup.
	float m_fFFRX_TrappedWrecksMax = 12;

	// 1 = l'IA cesse de soigner un camarade tant qu'elle est sous menace (elle se met a
	// couvert et reprend ensuite), 0 = comportement vanilla.
	float m_fFFRX_HealDiscipline = 1;

	// Read a float from the container, return the default if absent or 0.
	protected float MCD_GetFloatOrDefault(JWK_GameSettingsContainer container, JWK_EGameSetting setting, float defaultValue)
	{
		string raw = container.GetValue(setting);
		if (raw.IsEmpty()) return defaultValue;
		float val = raw.ToFloat();
		if (val == 0 && defaultValue != 0) return defaultValue;
		return val;
	}

	//! Variante qui ACCEPTE le zero comme une valeur voulue.
	//!
	//! POURQUOI ELLE EXISTE. MCD_GetFloatOrDefault ci-dessus remplace un 0 par le defaut
	//! des que celui-ci n'est pas nul. C'est un filet contre les valeurs non semees, mais
	//! il rend IMPOSSIBLE de couper un systeme : mettre "Gilets suicide" a 0 relisait 3,
	//! "Menace adaptative - intensite" a 0 relisait 35, etc. Tous les reglages documentes
	//! "0 = desactive" etaient donc inoperants a 0 -- constate en relisant le code, pas en
	//! jeu, car le symptome est muet : le curseur affiche bien 0 et le systeme tourne.
	//!
	//! Ici on ne teste que l'ABSENCE (chaine vide), ce qui suffit : FFRX_Seed pose deja
	//! une valeur pour chacun de nos reglages au demarrage.
	protected float MCD_GetFloatAllowZero(JWK_GameSettingsContainer container, JWK_EGameSetting setting, float defaultValue)
	{
		string raw = container.GetValue(setting);
		if (raw.IsEmpty()) return defaultValue;
		return raw.ToFloat();
	}

	override void Update(JWK_GameSettingsContainer container)
	{
		super.Update(container);

		// FFRX_STARTING_TOWN n'est plus lu : le reglage a ete retire de l'UI (cf.
		// FFRX_StartingTownGroup). La cle et sa valeur par defaut restent en place pour
		// ne pas decaler les cles de persistance suivantes.

		string rawSquad = container.GetValue(JWK_EGameSetting.FFRX_DEFAULT_SQUAD);
		int squad = 1;
		if (!rawSquad.IsEmpty()) squad = rawSquad.ToInt();
		m_iFFRXDefaultSquad = Math.ClampInt(squad, 1, 64);

		// De-solo: force player resting OFF regardless of the menu setting. Rest is
		// the ONLY trigger for time-skip in FF (JWK_PlayerRestManagerComponent ->
		// SkipTime_S), so this kills both Rest AND TimeSkip in one go.
		m_bEnablePlayerResting = false;

		// French army reskin: FF's resistance respawn RANDOMLY replaces 1-2 of
		// {headwear, shirt, pants} with items pulled from the ambient(civilian) /
		// enemy / support loadouts (JWK_ResistanceRespawnLoadoutHandler.RandomizeMixins).
		// That is why a spawned FIA soldier gets a random CIVILIAN CAP / t-shirt (or
		// enemy gear). A regular army must never do that -> force all three mixins OFF.
		m_bLoadoutAmbientMixins = false;
		m_bLoadoutEnemyMixins   = false;
		m_bLoadoutSupportMixins = false;

		// Every respawn issues a personal radio, so players can use the squad radio
		// (channel 39) right away. Force the resistance respawn loadout to include a
		// RADIO regardless of the menu setting. FF picks the radio from the player
		// faction catalog by common item type RADIO -> we override the AMF Thales
		// ER328 (Radio_ER328.et {DC1B220C66D47F85}) to CommonItemType "RADIO" so it
		// is the one issued. NOTE: the radio is a SLOT_VEST item, so the character
		// needs a vest/radio slot to actually carry it (verify in game).
		m_RespawnResistanceLoadoutOptions.m_aIncludeItems[JWK_ERespawnLoadoutItem.RADIO] = true;

		// Vehicle lock feature (JWK_OwnershipAccessComponent.IsFeatureEnabled) is gated on
		// this per-server setting -> force it ON so procured vehicles can be locked to their
		// owner (FFRX_VehicleLock). Without it, SetLocked_S is ignored on vehicles.
		m_bAllowLockVics = true;

		// --- MCD reads (percent in the menu -> 0..1 here; 0 means "use default") ---
		m_fMCD_ConvertChance              = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_CONVERT_CHANCE,                25) / 100.0, 0.0, 1.0);
		m_fMCD_ConvertHostileChance       = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_CONVERT_HOSTILE_CHANCE,        50) / 100.0, 0.0, 1.0);
		m_fMCD_ConvertArmedChance         = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_CONVERT_ARMED_CHANCE,          90) / 100.0, 0.0, 1.0);

		m_fMCD_ExtortHostileChance        = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_EXTORT_HOSTILE_CHANCE,         50) / 100.0, 0.0, 1.0);

		m_fMCD_PresenceNeutralShareChance = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_PRESENCE_NEUTRAL_SHARE_CHANCE, 50) / 100.0, 0.0, 1.0);
		m_fMCD_PresenceReportChance       = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_PRESENCE_REPORT_CHANCE,        50) / 100.0, 0.0, 1.0);

		// Initial reaction: friendly 30% / hostile 20% / neutral 50% by default
		m_fMCD_GreetFriendlyChance        = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_GREET_FRIENDLY_CHANCE,         30) / 100.0, 0.0, 1.0);
		m_fMCD_GreetHostileChance         = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_GREET_HOSTILE_CHANCE,          20) / 100.0, 0.0, 1.0);

		m_fMCD_GreetBombChance            = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_GREET_BOMB_CHANCE,             20) / 100.0, 0.0, 1.0);

		m_fMCD_GreetHostileAlertRadius    = MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_GREET_HOSTILE_ALERT_RADIUS, 80);
		m_fMCD_GreetHostileAlertHeat      = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_GREET_HOSTILE_ALERT_HEAT,     80) / 100.0, 0.0, 1.0);

		m_fMCD_CivCallMpChance            = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_CIV_CALL_MP_CHANCE,            30) / 100.0, 0.0, 1.0);

		// Jackpot: default 5% - rare but not impossible
		m_fMCD_ConvertJackpotChance       = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_CONVERT_JACKPOT_CHANCE,         5) / 100.0, 0.0, 1.0);

		// Gift: 40% chance per request. Patience cost 34 (~3 requests before fed up, on 100).
		m_fMCD_GiftChance                 = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_GIFT_CHANCE,                    40) / 100.0, 0.0, 1.0);
		m_fMCD_PatienceCost               = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.MCD_PATIENCE_COST,                  34),        1.0, 100.0);

		// --- FFRX AI difficulty (skill 0..100 per force; perception % -> factor) ---
		m_fFFRX_AISkillRegular  = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_AISKILL_REGULAR,   70), 0, 100);
		m_fFFRX_AISkillNaval    = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_AISKILL_NAVAL,     75), 0, 100);
		m_fFFRX_AISkillKLMK     = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_AISKILL_KLMK,      85), 0, 100);
		m_fFFRX_AISkillSpetsnaz = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_AISKILL_SPETSNAZ, 100), 0, 100);
		m_fFFRX_AIPerception    = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_AIPERCEPTION,     140), 50, 300) / 100.0;
		m_fFFRX_ATPriority      = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_AT_PRIORITY,        200), 0, 400);
		m_fFFRX_ATRange         = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_AT_RANGE,         1200), 700, 2000);
		m_fFFRX_AdaptStrength   = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_ADAPT_STRENGTH,   35), 0, 70);
		m_fFFRX_AdaptHalfLife   = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_ADAPT_HALFLIFE,   20), 5, 60);
		m_fFFRX_AdaptFloor      = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_ADAPT_FLOOR,      15), 0, 100);
		m_fFFRX_SpecDrone       = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_SPEC_DRONE,       8), 0, 40);
		m_fFFRX_SpecAA          = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_SPEC_AA,          6), 0, 40);
		m_fFFRX_SpecEW          = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_SPEC_EW,          4), 0, 40);
		m_fFFRX_SpecAT          = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_SPEC_AT,          2), 0, 40);
		m_fFFRX_NightPerception = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_NIGHT_PERCEPTION, 55), 20, 100);
		m_fFFRX_NVGPct          = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_NVG_PCT,          8), 0, 100);
		m_fFFRX_NVGEliteMult    = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_NVG_ELITE_MULT,   4), 1, 10);
		m_fFFRX_TierAdapt       = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_TIER_ADAPT,      50), 0, 100);
		m_fFFRX_TierRadius      = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_TIER_RADIUS,    800), 200, 2000);
		m_fFFRX_TierXpRef       = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_TIER_XPREF,    3000), 500, 20000);
		m_fFFRX_VestPct         = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_VEST_PCT,         3), 0, 25);
		// 100 = pas de bonus (c'est le "desactive" ici, pas 0) -> GetFloatOrDefault convient.
		m_fFFRX_GenieBuild      = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_GENIE_BUILD,   250), 100, 500);
		m_fFFRX_MedicHeal       = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_MEDIC_HEAL,    200), 100, 400);
		m_fFFRX_MedicSave       = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_MEDIC_SAVE,     35), 0, 100);
		m_fFFRX_TracerBoost     = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_TRACER_BOOST,  60), 0, 200);
		m_fFFRX_BuildSiteTime   = Math.Clamp(MCD_GetFloatAllowZero(container, JWK_EGameSetting.FFRX_BUILD_SITE_TIME, 100), 0, 400);
		// 0 means "vanilla" here, so read it raw (MCD_GetFloatOrDefault would treat 0 as absent).
		string rawCrew = container.GetValue(JWK_EGameSetting.FFRX_VEH_CREW_HOLD);
		if (rawCrew.IsEmpty())
			m_fFFRX_VehCrewHold = 1;
		else
			m_fFFRX_VehCrewHold = Math.Clamp(rawCrew.ToFloat(), 0, 1);

		// Same: 0 is a meaningful value (instant fire), so read it raw.
		string rawDelay = container.GetValue(JWK_EGameSetting.FFRX_REACTION_DELAY);
		if (rawDelay.IsEmpty())
			m_fFFRX_ReactionDelay = 0.5;
		else
			m_fFFRX_ReactionDelay = Math.Clamp(rawDelay.ToFloat(), 0, 100) / 100.0;

		string rawAim = container.GetValue(JWK_EGameSetting.FFRX_AIM_SPEED);
		if (rawAim.IsEmpty())
			m_fFFRX_AimSpeed = 0.4;
		else
			m_fFFRX_AimSpeed = Math.Clamp(rawAim.ToFloat(), 5, 100) / 100.0;

		// 0 = feature off, so raw read again.
		string rawCqb = container.GetValue(JWK_EGameSetting.FFRX_CQB_REFLEX);
		if (rawCqb.IsEmpty())
			m_fFFRX_CqbReflex = 15;
		else
			m_fFFRX_CqbReflex = Math.Clamp(rawCqb.ToFloat(), 0, 50);

		string rawHe = container.GetValue(JWK_EGameSetting.FFRX_HE_CHANCE);
		if (rawHe.IsEmpty())
			m_fFFRX_HeChance = 30;
		else
			m_fFFRX_HeChance = Math.Clamp(rawHe.ToFloat(), 0, 100);

		m_fFFRX_HeCooldown = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_HE_COOLDOWN, 25), 5, 120);

		string rawRf = container.GetValue(JWK_EGameSetting.FFRX_RETURN_FIRE);
		if (rawRf.IsEmpty())
			m_fFFRX_ReturnFire = 1;
		else
			m_fFFRX_ReturnFire = Math.Clamp(rawRf.ToFloat(), 0, 1);
		m_fFFRX_JammerVehiclePct = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_JAMMER_VEHICLE_PCT, 30), 0, 100);
		m_fFFRX_WeaponFamiliarityPct = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_WEAPON_FAMILIARITY_PCT, 10), 0, 50);
		m_fFFRX_AssaultPct     = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_ASSAULT_PCT,       50), 0, 80);
		m_fFFRX_BoobyTrapPct   = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_BOOBYTRAP_PCT,    35), 0, 100);
		m_fFFRX_BoobyTrapMax   = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_BOOBYTRAP_MAX,     6), 0, 30);
		m_fFFRX_VbiedPct       = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_VBIED_PCT,       12), 0, 100);
		m_fFFRX_VbiedMax       = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_VBIED_MAX,        1), 0, 5);
		m_fFFRX_IedScatterMax  = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_IED_SCATTER_MAX, 20), 0, 80);
		m_fFFRX_TrappedWrecksMax = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_TRAPPED_WRECKS_MAX, 12), 0, 40);
		m_fFFRX_HealDiscipline = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_HEAL_DISCIPLINE,  1), 0, 1);
		m_fFFRX_AssaultMinDist = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_ASSAULT_MIN_DIST,  80), 40, 400);
		m_fFFRX_AntiCamp       = Math.Clamp(MCD_GetFloatOrDefault(container, JWK_EGameSetting.FFRX_ANTICAMP, 1), 0, 1);

		// Push the new values to every living enemy AI immediately (live tuning).
		SCR_AICombatComponent.FFRX_ReapplyAll();
	}
}

[BaseContainerProps(configRoot: true)]
modded class JWK_GameSettingsConfig
{
	// STRONG static ref: GetGroups' out array is array<...> (non-owning), so a
	// group not held elsewhere is GC'd immediately -> the UI dereferences a null
	// group -> "NULL pointer ... m_sDisplayName". Keep the group alive here.
	protected static ref JWK_GameSettingsGroupConfig s_FFRXGroup;
	protected static ref JWK_GameSettingsGroupConfig s_MCDGroup; // folded from MCD
	protected static ref JWK_GameSettingsGroupConfig s_FFRXAIGroup; // live AI difficulty

	override void GetGroups(array<JWK_GameSettingsGroupConfig> outResult)
	{
		super.GetGroups(outResult);
		outResult.Insert(FFRX_StartingTownGroup());
		outResult.Insert(MCD_SettingsGroup());
		outResult.Insert(FFRX_AIGroup());
	}

	override JWK_BaseGameSettingConfig GetSetting(JWK_EGameSetting setting)
	{
		JWK_BaseGameSettingConfig found = super.GetSetting(setting);
		if (found) return found;

		JWK_GameSettingsGroupConfig group = FFRX_StartingTownGroup();
		if (group)
			foreach (JWK_BaseGameSettingConfig cfg : group.m_aSettings)
				if (cfg.m_iSetting == setting) return cfg;

		JWK_GameSettingsGroupConfig mcdGroup = MCD_SettingsGroup();
		if (mcdGroup)
			foreach (JWK_BaseGameSettingConfig mcfg : mcdGroup.m_aSettings)
				if (mcfg.m_iSetting == setting) return mcfg;

		JWK_GameSettingsGroupConfig aiGroup = FFRX_AIGroup();
		if (aiGroup)
			foreach (JWK_BaseGameSettingConfig acfg : aiGroup.m_aSettings)
				if (acfg.m_iSetting == setting) return acfg;

		return null;
	}

	// A runtime-changeable slider with plain (non-localized) French labels.
	protected static JWK_BaseGameSettingConfig FFRX_AISlider(
		JWK_EGameSetting setting, string name, string hint,
		float min, float max, float step, string format)
	{
		JWK_SpinBoxGameSettingWidgetConfig widget = new JWK_SpinBoxGameSettingWidgetConfig();
		widget.m_aOptions = FFRX_BuildSpinOptions(min, max, step, format);

		JWK_BaseGameSettingConfig cfg = new JWK_BaseGameSettingConfig();
		cfg.m_iSetting            = setting;
		cfg.m_bAllowRuntimeChange = true; // live
		cfg.m_sDisplayName        = name;
		cfg.m_sDescription        = hint;
		cfg.m_iDefaultWidget      = JWK_EGameSettingDefaultWidget.SPINBOX;
		cfg.m_WidgetConfig        = widget;
		return cfg;
	}

	protected static JWK_GameSettingsGroupConfig FFRX_AIGroup()
	{
		if (s_FFRXAIGroup) return s_FFRXAIGroup;

		JWK_GameSettingsGroupConfig group = new JWK_GameSettingsGroupConfig();
		group.m_sDisplayName        = "FF REMIXED - Difficulte IA";
		group.m_sDefaultHint        = "Skill (precision de tir) par force + perception ennemie. Modifiable en jeu.";
		group.m_bAllowRuntimeChange = true;
		group.m_aSettings           = {};

		string skillHint = "0=nul, 20=recrue, 50=normal, 70=veteran, 80=expert, 100=parfait.";
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_AISKILL_REGULAR,  "Skill - Reguliers", skillHint, 0, 100, 5, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_AISKILL_NAVAL,    "Skill - Naval",     skillHint, 0, 100, 5, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_AISKILL_KLMK,     "Skill - KLMK",      skillHint, 0, 100, 5, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_AISKILL_SPETSNAZ, "Skill - Spetsnaz",  skillHint + " (100 par defaut)", 0, 100, 5, "%1"));
		// min 0 (not 50): an un-seeded value reads as 0, and the spinbox refuses a value
		// below its minimum -> "Wrong parameter value" crash. The cache floors it to 50
		// (0.5x) anyway (see Update), so 0 on the slider is harmless.
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_AIPERCEPTION,     "Perception ennemie", "Vitesse/portee de detection. 100=normal, 300=x3.", 0, 300, 10, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_AT_PRIORITY, "AT/AA - priorite vehicules",
			"Bonus de priorite donne aux vehicules/aeronefs pour les porteurs de lance-roquettes ennemis. 0 = comportement vanilla, 120 = ils lachent l'infanterie pour tirer le vehicule.", 0, 400, 10, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_AT_RANGE, "AT/AA - portee vehicules",
			"Distance max a laquelle un porteur de lanceur considere encore un vehicule. Vanilla = 700 m.", 700, 2000, 50, "%1m"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_ADAPT_STRENGTH, "Menace adaptative - intensite",
			"Part maximale des groupes ennemis qui recoivent un servant antichar ou sol-air dans une zone ou les joueurs ont frappe au blinde / survole en helico. 0 = systeme desactive, 35 = defaut, 70 = reponse tres marquee.", 0, 70, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_ADAPT_HALFLIFE, "Menace adaptative - memoire",
			"Temps au bout duquel une zone oublie la moitie de la pression subie. 5 min = l'ennemi reagit vite et oublie vite, 60 min = il tient rancune longtemps. 20 = defaut.", 5, 60, 5, "%1 min"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_ADAPT_FLOOR, "Menace adaptative - socle",
			"Part de specialistes antichar / sol-air presente meme dans une zone ou les joueurs n'ont encore rien frappe. 0 = purement reactif (le premier helicoptere ne rencontre aucune defense), 15 = defaut, 40 = l'ennemi est equipe en permanence.", 0, 60, 5, "%1%%"));
		// Frequence des equipes specialistes. Ces pourcentages s'appliquent a CHAQUE groupe
		// d'infanterie ennemi produit (patrouilles de ville, garnisons, camps, QRF, vagues).
		// Leur somme est la part totale de specialistes ; le reste est ordinaire.
		string specHint = "Pourcentage des groupes ennemis remplaces par cette equipe. Mettre les quatre a 0 = tirage vanilla.";
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_SPEC_DRONE, "Equipes - drone %",
			"Operateurs FPV + Mavic avec escorte. " + specHint, 0, 40, 1, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_SPEC_AA, "Equipes - sol-air %",
			"Servant Igla avec escorte : dangereux pour les helicos. " + specHint, 0, 40, 1, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_SPEC_EW, "Equipes - guerre elec. %",
			"Porteur de brouilleur : coupe les drones autour de lui. " + specHint, 0, 40, 1, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_SPEC_AT, "Equipes - antichar lourd %",
			"Servant Javelin avec escorte : dangereux pour les blindes. " + specHint, 0, 40, 1, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_NIGHT_PERCEPTION, "Vue de l'ennemi la nuit %",
			"Le moteur fait voir l'IA aussi bien a 3h qu'a midi. Ce reglage lui retire de la perception entre 21h et 5h -- SAUF aux rares porteurs d'optique de vision nocturne, qui gardent leur vue de jour. 100 = desactive, 55 = defaut, 20 = presque aveugle.", 20, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_NVG_PCT, "Optique NV - part des ennemis %",
			"Proportion de soldats ennemis equipes d'une optique de vision nocturne : eux seuls gardent leur vue de jour apres 21h. 0 = personne (toute l'IA est aveugle la nuit), 8 = defaut.", 0, 100, 1, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_NVG_ELITE_MULT, "Optique NV - facteur elite",
			"Multiplie la part ci-dessus pour les KLMK et les Spetsnaz, bien mieux dotes que la troupe. Avec 8 %% et x4 : 8 %% chez les reguliers, 32 %% chez l'elite. 1 = aucune difference.", 1, 10, 1, "x%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_TIER_ADAPT, "Escouades adaptees aux joueurs %",
			"Les escouades ennemies qui apparaissent PRES de joueurs anciens penchent vers l'aguerri et l'elite ; pres de nouveaux venus, vers les bleus. L'anciennete est mesuree sur l'XP de temps de jeu. Le niveau est fixe a la naissance du groupe et n'est jamais recalcule : l'arrivee d'un ancien ne durcit pas retroactivement les patrouilles deja en place. 0 = tirage au sort pur (15/60/20/5), 50 = defaut, 100 = adaptation maximale.", 0, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_TIER_RADIUS, "Escouades adaptees - rayon",
			"Distance autour du groupe qui nait dans laquelle on regarde qui joue. Aucun joueur a portee = aucune adaptation, la repartition d'origine s'applique. 800 m = defaut.", 200, 2000, 100, "%1m"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_TIER_XPREF, "Escouades adaptees - XP de reference",
			"XP a partir duquel un joueur est considere comme pleinement aguerri (adaptation au maximum). L'XP monte de 10 par minute jouee : 3000 = environ 5 h de jeu. Baisser rend l'ennemi dur plus tot, monter reserve l'elite aux tres anciens.", 500, 20000, 500, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_VEST_PCT, "Gilets suicide %",
			"Part des combattants ennemis portant un gilet explosif. Ils chargent le joueur a moins de 70 m et explosent au contact -- ou s'ils meurent en pleine course. Rien ne les distingue avant qu'ils ne se mettent a courir. 0 = desactive, 3 = defaut.", 0, 25, 1, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_VEH_CREW_HOLD, "Equipages tiennent la tourelle",
			"1 = un servant de tourelle ennemi reste a son poste au lieu de debarquer quand la cible sort de son cone de tir. 0 = comportement vanilla.", 0, 1, 1, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_REACTION_DELAY, "Delai de riposte %",
			"Temps que l'ennemi surpris met avant d'ouvrir le feu. 100 = vanilla (jusqu'a ~1s a 300m), 50 = deux fois plus vif, 0 = instantane.", 0, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_AIM_SPEED, "Temps de visee %",
			"Temps de stabilisation avant chaque tir. 100 = vanilla (0.4s + 1s de rejet), 40 = beaucoup plus vif. ATTENTION: agit sur TOUTE l'IA, alliee comprise.", 5, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_CQB_REFLEX, "Reflexe au contact (m)",
			"Sous cette distance, un ennemi repere fait lacher l'ordre de deplacement : l'IA s'arrete et tire au lieu de continuer a courir. 0 = desactive.", 0, 50, 5, "%1m"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_HE_CHANCE, "Obus sur infanterie %",
			"Chance qu'un canon de char tire un obus sur de l'infanterie au lieu de la mitrailleuse. Toujours declenche si plusieurs ennemis groupes. 0 = jamais.", 0, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_HE_COOLDOWN, "Delai mini entre obus",
			"Temps minimum entre deux obus tires sur de l'infanterie, pour qu'un char ne vide pas sa soute.", 5, 120, 5, "%1s"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_RETURN_FIRE, "Riposte immediate au tir recu",
			"1 = celui qui encaisse un tir se retourne aussitot vers le tireur et passe en alerte maximale. Corrige surtout les equipages: quand un char prend une roquette, la victime est le VEHICULE, donc l'equipage n'etait meme pas prevenu. 0 = comportement vanilla.", 0, 1, 1, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_JAMMER_VEHICLE_PCT, "Jammer vehicules %", "Chance qu'un vehicule ennemi ait un jammer anti-drone (0 = aucun).", 0, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_WEAPON_FAMILIARITY_PCT, "Accoutumance arme %", "Reduction maximale du tremblement quand on garde la meme arme (0 = desactive).", 0, 50, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_ASSAULT_PCT, "Assaut : part du groupe %",
			"Quand un groupe ennemi accroche un joueur a distance, part du groupe qui va le chercher pendant que le reste appuie. Evite le tir au pigeon a longue portee. 0 = desactive.", 0, 80, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_ASSAULT_MIN_DIST, "Assaut : distance mini",
			"En deca de cette distance, pas d'assaut : le combat a distance se joue normalement.", 40, 400, 10, "%1m"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_ANTICAMP, "Anti-camping (drone)",
			"1 = tirer longtemps depuis la meme position finit par attirer un drone ennemi. Se declenche sur la DUREE de tir depuis un meme endroit, pas sur le nombre de tirs : bouger suffit a l'eviter. 0 = desactive.", 0, 1, 1, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_BOOBYTRAP_PCT, "Voitures piegees %",
			"Chance qu'une voiture civile abandonnee en territoire ennemi soit piegee (mine AP enterree dessous). Le detecteur de mines ACE la repere. 0 = desactive.", 0, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_GENIE_BUILD, "Genie - vitesse de construction %",
			"Rendement de construction des membres d'une escouade marquee \"genie\" dans ffrx-groups.json : sacs de sable, bunkers, batiments FF. 100 = comme tout le monde (desactive), 250 = defaut (2,5x plus vite). Ne change RIEN au cout en ravitaillement, seulement au temps de montage.", 100, 500, 25, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_BUILD_SITE_TIME, "Chantier - temps de montage %",
			"Construire pose un CHANTIER a monter a la pelle au lieu de faire apparaitre le batiment. La duree est proportionnelle au cout du batiment : a 100 %, 1 point de chantier pour 10 supplies, soit 12 coups pour un bunker et 150 pour un depot de vehicules (une pelle donne 10 points, un sapeur 2,5 fois plus). Seul le genie peut OUVRIR un chantier ; tout le monde peut pelleter dessus. 0 = comportement FF d'origine, batiment instantane.", 0, 400, 25, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_TRACER_BOOST, "Tracantes - l'ennemi repere mieux %",
			"Perception gagnee par un ennemi qui a une LIGNE DE VUE sur un joueur tirant a la tracante, pendant 12 s. Le moteur ignore completement les tracantes : sans ce reglage, une balle lumineuse ne coute rien. Effet DOUBLE la nuit, ou une tracante se voit de tres loin. 0 = desactive, 60 = defaut.", 0, 200, 10, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_MEDIC_HEAL, "Sante - efficacite du soin %",
			"Regeneration rendue par un pansement pose par un membre d'une escouade marquee \"medic\" dans ffrx-groups.json. Meme geste, meilleur resultat : on a interet a se faire soigner par JULIETT plutot que par son voisin. 100 = comme tout le monde (desactive), 200 = defaut.", 100, 400, 25, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_MEDIC_SAVE, "Sante - pansement economise %",
			"Chance qu'un medecin ne consomme PAS son pansement en soignant : il sait doser. Rend sa trousse durable sans lui donner de materiel gratuit au depart. 0 = desactive, 35 = defaut.", 0, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_HEAL_DISCIPLINE, "Discipline de soin IA",
			"1 = l'IA ne soigne plus un camarade tant qu'elle est sous menace : elle se met a couvert et soigne une fois au calme. 0 = comportement vanilla (soin en plein champ de tir).", 0, 1, 1, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_BOOBYTRAP_MAX, "Voitures piegees : max",
			"Nombre maximum de voitures piegees actives en meme temps sur la carte.", 0, 30, 1, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_VBIED_PCT, "Voiture beliere (VBIED) %",
			"Chance, toutes les 4 min et par joueur en territoire ennemi ou en zone contestee, qu'une voiture civile piegee parte le percuter. Contrairement a la voiture piegee posee, celle-ci ROULE : on la voit venir et on peut l'arreter au tir. 0 = desactive.", 0, 100, 5, "%1%%"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_IED_SCATTER_MAX, "IED sur les lieux : max",
			"Nombre d'IED poses A L'AVANCE sur les points d'interet de la carte (emplacements du level design + checkpoints ennemis), jamais en territoire tenu par la resistance. Contrairement aux autres menaces, ceux-la n'apparaissent pas autour du joueur : le danger appartient au LIEU, ce qui rend l'approche d'un site inconnu tendue. Sur un site routier c'est un IED enterre (il faut rouler dessus), ailleurs une poubelle ou une marmite (5-7 m). Espaces de 60 m minimum. 0 = desactive.", 0, 80, 5, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_TRAPPED_WRECKS_MAX, "Carcasses piegees : max",
			"Nombre maximum d'epaves de decor piegees sur la carte (charge discrete a cote de la carcasse, rayon 7 m -- assez pour punir qui vient FOUILLER, pas un vehicule qui passe sur la route d'a cote). Seule une minorite des epaves trouvees est piegee : le but est de rendre chaque carcasse SUSPECTE, pas d'en faire une regle. Jamais en territoire tenu par la resistance. 0 = desactive (le balayage de la carte est alors saute entierement).", 0, 40, 2, "%1"));
		group.m_aSettings.Insert(FFRX_AISlider(JWK_EGameSetting.FFRX_VBIED_MAX, "Voiture beliere : max",
			"Nombre maximum de voitures belieres lancees en meme temps. Volontairement bas : deux qui convergent, c'est une embuscade impossible a lire.", 0, 5, 1, "%1"));

		s_FFRXAIGroup = group;
		return s_FFRXAIGroup;
	}

	// Build spinbox options min..max by step, labelled via `format` ("%1%%","%1","%1m"...).
	// We use a SPINBOX (not a JWK SLIDER): the JWK slider widget crashes in this menu
	// (JWK_SliderGameSettingsEntryUIComponent indexes its element list out of range ->
	// "Wrong parameter value"). The starting-town SPINBOX proves spinboxes are safe here.
	protected static ref array<ref JWK_SpinBoxGameSettingsOption> FFRX_BuildSpinOptions(float min, float max, float step, string format)
	{
		array<ref JWK_SpinBoxGameSettingsOption> options = {};
		int v = min;
		while (v <= max)
		{
			JWK_SpinBoxGameSettingsOption opt = new JWK_SpinBoxGameSettingsOption();
			opt.m_sValue = v.ToString();
			opt.m_sLabel = string.Format(format, v);
			options.Insert(opt);
			v = v + step;
		}
		return options;
	}

	// locName -> "#JWK-GameSetting-MCD-<locName>" (+ "-Description").
	protected static JWK_BaseGameSettingConfig MCD_Slider(
		JWK_EGameSetting setting, string locName, string format,
		float min = 0, float max = 100, float step = 1)
	{
		JWK_SpinBoxGameSettingWidgetConfig widget = new JWK_SpinBoxGameSettingWidgetConfig();
		widget.m_aOptions = FFRX_BuildSpinOptions(min, max, step, format);

		string key = "#JWK-GameSetting-MCD-" + locName;

		JWK_BaseGameSettingConfig cfg = new JWK_BaseGameSettingConfig();
		cfg.m_iSetting            = setting;
		cfg.m_bAllowRuntimeChange = true;
		cfg.m_sDisplayName        = key;
		cfg.m_sDescription        = key + "-Description";
		cfg.m_iDefaultWidget      = JWK_EGameSettingDefaultWidget.SPINBOX;
		cfg.m_WidgetConfig        = widget;
		return cfg;
	}

	protected static JWK_GameSettingsGroupConfig MCD_SettingsGroup()
	{
		if (s_MCDGroup) return s_MCDGroup;

		JWK_GameSettingsGroupConfig group = new JWK_GameSettingsGroupConfig();
		group.m_sDisplayName        = "More Civilian Discussion";
		group.m_sDefaultHint        = "Reglages des interactions civiles (MCD).";
		group.m_bAllowRuntimeChange = true;
		group.m_aSettings           = {};

		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_CONVERT_CHANCE,                "ConvertChance",              "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_CONVERT_HOSTILE_CHANCE,        "ConvertHostileChance",       "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_CONVERT_ARMED_CHANCE,          "ConvertArmedChance",         "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_CONVERT_JACKPOT_CHANCE,        "ConvertJackpotChance",       "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_EXTORT_HOSTILE_CHANCE,         "ExtortHostileChance",        "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_PRESENCE_NEUTRAL_SHARE_CHANCE, "PresenceNeutralShareChance", "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_PRESENCE_REPORT_CHANCE,        "PresenceReportChance",       "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_GREET_FRIENDLY_CHANCE,         "GreetFriendlyChance",        "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_GREET_HOSTILE_CHANCE,          "GreetHostileChance",         "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_GREET_BOMB_CHANCE,             "GreetBombChance",            "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_GREET_HOSTILE_ALERT_RADIUS,    "GreetHostileAlertRadius",    "%1m", 0, 200, 10));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_GREET_HOSTILE_ALERT_HEAT,      "GreetHostileAlertHeat",      "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_CIV_CALL_MP_CHANCE,            "CivCallMpChance",            "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_GIFT_CHANCE,                   "GiftChance",                 "%1%%"));
		group.m_aSettings.Insert(MCD_Slider(JWK_EGameSetting.MCD_PATIENCE_COST,                 "PatienceCost",               "%1", 0, 100, 1)); // min 0 (not 1): un-seeded=0 must be valid or the spinbox crashes; cache floors it to 1

		s_MCDGroup = group;
		return s_MCDGroup;
	}

	// Returns a RETAINED group (meme instance dans une session de menu -> UI-safe).
	//
	// ⚠️ Le reglage "Ville de depart (spawn)" A ETE RETIRE de ce groupe le 2026-09-08.
	// Il ne servait PLUS A RIEN : le design a change (brique A place desormais 0
	// partisan dans TOUTES les villes et on demarre sur une FOB, cf. FFRX_DefaultFob),
	// et plus aucun code ne lisait m_iFFRXStartingTown -- verifie par recherche sur
	// tout l'addon. L'admin voyait donc une liste de villes qui ne changeait rien :
	// un reglage qui ment est pire que pas de reglage. Le choix du point de depart se
	// fait maintenant en jeu avec la commande admin #placefob, qui deplace la FOB sans
	// redemarrer le serveur.
	//
	// ⚠️ La CLE d'enum JWK_EGameSetting.FFRX_STARTING_TOWN est volontairement CONSERVEE.
	// Les valeurs d'enum sont les cles de persistance : la supprimer decalerait
	// FFRX_DEFAULT_SQUAD et tous les reglages MCD d'un cran, et les sauvegardes
	// existantes reliraient les mauvaises valeurs. On retire l'entree de l'UI, pas la cle.
	protected static JWK_GameSettingsGroupConfig FFRX_StartingTownGroup()
	{
		if (s_FFRXGroup)
			return s_FFRXGroup;

		// --- Default squad spinbox (names from the groups config file, since the
		// groups themselves are only created at game start) ---
		array<ref JWK_SpinBoxGameSettingsOption> squadOptions = {};
		array<string> squadNames = FFRX_GroupsManager.GetConfiguredGroupNames();
		if (squadNames && !squadNames.IsEmpty()) {
			foreach (int i, string name : squadNames) {
				JWK_SpinBoxGameSettingsOption opt = new JWK_SpinBoxGameSettingsOption();
				opt.m_sValue = string.Format("%1", i + 1);
				opt.m_sLabel = name;
				squadOptions.Insert(opt);
			}
		} else {
			for (int i = 1; i <= 20; i++) {
				JWK_SpinBoxGameSettingsOption opt = new JWK_SpinBoxGameSettingsOption();
				opt.m_sValue = string.Format("%1", i);
				opt.m_sLabel = string.Format("Escouade %1", i);
				squadOptions.Insert(opt);
			}
		}

		JWK_SpinBoxGameSettingWidgetConfig squadWidget = new JWK_SpinBoxGameSettingWidgetConfig();
		squadWidget.m_aOptions = squadOptions;

		JWK_BaseGameSettingConfig squadSetting = new JWK_BaseGameSettingConfig();
		squadSetting.m_iSetting = JWK_EGameSetting.FFRX_DEFAULT_SQUAD;
		squadSetting.m_bAllowRuntimeChange = false;
		squadSetting.m_sDisplayName = "Escouade par defaut";
		squadSetting.m_sDescription = "L'escouade dans laquelle les nouveaux joueurs sont places au spawn.";
		squadSetting.m_iDefaultWidget = JWK_EGameSettingDefaultWidget.SPINBOX;
		squadSetting.m_WidgetConfig = squadWidget;

		JWK_GameSettingsGroupConfig group = new JWK_GameSettingsGroupConfig();
		group.m_sDisplayName = FFRX_Settings.GROUP_NAME;
		group.m_sDefaultHint = "Reglages PVE (FF REMIXED).";
		group.m_bAllowRuntimeChange = false; // whole group is setup-only
		group.m_aSettings = {};
		group.m_aSettings.Insert(squadSetting);

		s_FFRXGroup = group;
		return s_FFRXGroup;
	}

}

// ---------------------------------------------------------------------------------------
// Default values for our custom settings. FF requires a preset default per setting or the
// UI reads an empty value (crash / wrong default) -- BasePreset.conf is "mandatory" per
// the FF docs. Instead of OWNING the shared BasePreset.conf, we mod the preset config's
// CreateContainer and seed our defaults after super (only if not already present, so a
// player's saved value always wins). Restate [BaseContainerProps(configRoot: true)] --
// omitting it on a configRoot modded class nulls its binding.
[BaseContainerProps(configRoot: true)]
modded class JWK_GameSettingsPresetConfig
{
	override JWK_GameSettingsContainer CreateContainer()
	{
		JWK_GameSettingsContainer container = super.CreateContainer();

		// FFRX setup
		FFRX_Seed(container, JWK_EGameSetting.FFRX_STARTING_TOWN, "1");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_DEFAULT_SQUAD,  "1");

		// AI difficulty (skill 0..100 per force; perception percent, 100 = normal)
		FFRX_Seed(container, JWK_EGameSetting.FFRX_AISKILL_REGULAR,  "70");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_AISKILL_NAVAL,    "75");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_AISKILL_KLMK,     "85");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_AISKILL_SPETSNAZ, "100");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_AIPERCEPTION,     "140");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_AT_PRIORITY,      "200");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_AT_RANGE,        "1200");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_ADAPT_STRENGTH,  "35");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_ADAPT_HALFLIFE,  "20");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_ADAPT_FLOOR,     "15");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_SPEC_DRONE,      "8");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_SPEC_AA,         "6");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_SPEC_EW,         "4");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_SPEC_AT,         "2");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_NIGHT_PERCEPTION, "55");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_NVG_PCT,          "8");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_NVG_ELITE_MULT,   "4");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_VEST_PCT,         "3");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_GENIE_BUILD,      "250");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_MEDIC_HEAL,       "200");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_MEDIC_SAVE,       "35");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_TRACER_BOOST,     "60");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_BUILD_SITE_TIME,  "100");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_VEH_CREW_HOLD,   "1");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_REACTION_DELAY,  "50");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_AIM_SPEED,       "40");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_CQB_REFLEX,      "15");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_HE_CHANCE,       "30");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_HE_COOLDOWN,     "25");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_RETURN_FIRE,     "1");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_JAMMER_VEHICLE_PCT, "30");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_WEAPON_FAMILIARITY_PCT, "10");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_ASSAULT_PCT,      "50");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_ASSAULT_MIN_DIST, "80");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_ANTICAMP,        "1");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_BOOBYTRAP_PCT,    "35");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_BOOBYTRAP_MAX,    "6");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_VBIED_PCT,       "12");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_VBIED_MAX,        "1");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_IED_SCATTER_MAX, "20");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_TRAPPED_WRECKS_MAX, "12");
		FFRX_Seed(container, JWK_EGameSetting.FFRX_HEAL_DISCIPLINE, "1");

		// MCD civilian discussion (percent chances + patience/radius/heat)
		FFRX_Seed(container, JWK_EGameSetting.MCD_CONVERT_CHANCE,                "25");
		FFRX_Seed(container, JWK_EGameSetting.MCD_CONVERT_HOSTILE_CHANCE,        "50");
		FFRX_Seed(container, JWK_EGameSetting.MCD_CONVERT_ARMED_CHANCE,          "90");
		FFRX_Seed(container, JWK_EGameSetting.MCD_EXTORT_HOSTILE_CHANCE,         "50");
		FFRX_Seed(container, JWK_EGameSetting.MCD_PRESENCE_NEUTRAL_SHARE_CHANCE, "50");
		FFRX_Seed(container, JWK_EGameSetting.MCD_PRESENCE_REPORT_CHANCE,        "50");
		FFRX_Seed(container, JWK_EGameSetting.MCD_GREET_FRIENDLY_CHANCE,         "30");
		FFRX_Seed(container, JWK_EGameSetting.MCD_GREET_HOSTILE_CHANCE,          "20");
		FFRX_Seed(container, JWK_EGameSetting.MCD_GREET_BOMB_CHANCE,             "20");
		FFRX_Seed(container, JWK_EGameSetting.MCD_GREET_HOSTILE_ALERT_RADIUS,    "80");
		FFRX_Seed(container, JWK_EGameSetting.MCD_GREET_HOSTILE_ALERT_HEAT,      "80");
		FFRX_Seed(container, JWK_EGameSetting.MCD_CIV_CALL_MP_CHANCE,            "30");
		FFRX_Seed(container, JWK_EGameSetting.MCD_CONVERT_JACKPOT_CHANCE,        "5");
		FFRX_Seed(container, JWK_EGameSetting.MCD_GIFT_CHANCE,                   "40");
		FFRX_Seed(container, JWK_EGameSetting.MCD_PATIENCE_COST,                 "34");

		return container;
	}

	protected void FFRX_Seed(JWK_GameSettingsContainer container, JWK_EGameSetting setting, string value)
	{
		if (container.GetValue(setting).IsEmpty())
			container.SetValue(setting, value);
	}
}
