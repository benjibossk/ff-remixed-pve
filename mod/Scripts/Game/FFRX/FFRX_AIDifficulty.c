// FF - REMIXED - PVE : live per-force enemy AI difficulty.
//
// CREDITS -- several behaviours below are re-implementations of ideas identified in
// "CRX Enfusion A.I." by ATiM- (Workshop 5F268647F8A1A1F4). CRX could not be used as a
// dependency here (its 220 scripts push the Game script module past the engine's global
// static-init instruction budget, breaking compilation for the whole project), so the
// behaviours we wanted were re-written from scratch against the vanilla API. No CRX code
// is copied. Credit for spotting these AI shortcomings goes to ATiM-.
// Concerned: turret crews holding position, faster re-engagement of a known target.
//
// Each enemy SCR_AICombatComponent self-applies its force's skill + the global
// perception factor from JWK.GameSettingsCache() on init (new spawns), and can be
// re-applied to ALL living enemies at once when a slider changes (FFRX_ReapplyAll,
// called from the modded JWK_GameSettingsCache.Update in FFRX_GameSettings.c).
//
// Force is classified from the character prefab PATH (Spetsnaz / Naval_Infantry /
// KLMK folder, else Regular). MEI (desert theatre) maps to Regular. Characters that are
// neither USSR nor MEI are ignored (skill untouched).
// Skill float 0..100 -> nearest EAISkill tier; perception is a multiplier (1.0 = default).

enum FFRX_EForce
{
	NONE,
	REGULAR,
	NAVAL,
	KLMK,
	SPETSNAZ
}

modded class SCR_AICombatComponent
{
	// Registry of live enemy combat components, kept in sync via EOnInit / OnDelete
	// (OnDelete exists on this component, so no leaked strong refs).
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<SCR_AICombatComponent> s_FFRX_Registry;

	protected static array<SCR_AICombatComponent> Registry()
	{
		if (!s_FFRX_Registry)
			s_FFRX_Registry = new array<SCR_AICombatComponent>();

		return s_FFRX_Registry;
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner); // sets m_eAISkill = m_eAISkillDefault
		if (Registry().Find(this) < 0)
			Registry().Insert(this);
		FFRX_ApplyDifficulty();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		Registry().RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	static FFRX_EForce FFRX_ClassifyPrefab(string p)
	{
		// Desert theatre: MEI has no sub-forces, it rides the "Reguliers" slider.
		if (p.Contains("MEI"))
			return FFRX_EForce.REGULAR;

		if (!p.Contains("USSR"))
			return FFRX_EForce.NONE;
		if (p.Contains("Spetsnaz"))
			return FFRX_EForce.SPETSNAZ;
		if (p.Contains("Naval"))
			return FFRX_EForce.NAVAL;
		if (p.Contains("KLMK"))
			return FFRX_EForce.KLMK;
		return FFRX_EForce.REGULAR;
	}

	//------------------------------------------------------------------------------------------------
	static EAISkill FFRX_SkillFromFloat(float v)
	{
		if (v >= 90) return EAISkill.CYLON;
		if (v >= 75) return EAISkill.EXPERT;
		if (v >= 60) return EAISkill.VETERAN;
		if (v >= 35) return EAISkill.REGULAR;
		if (v >= 15) return EAISkill.ROOKIE;
		return EAISkill.NOOB;
	}

	//------------------------------------------------------------------------------------------------
	void FFRX_ApplyDifficulty()
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		EntityPrefabData pd = owner.GetPrefabData();
		if (!pd)
			return;

		string p = pd.GetPrefabName();
		FFRX_EForce force = FFRX_ClassifyPrefab(p);
		if (force == FFRX_EForce.NONE)
			return;

		float skillVal = cache.m_fFFRX_AISkillRegular;
		if (force == FFRX_EForce.SPETSNAZ)
			skillVal = cache.m_fFFRX_AISkillSpetsnaz;
		else if (force == FFRX_EForce.NAVAL)
			skillVal = cache.m_fFFRX_AISkillNaval;
		else if (force == FFRX_EForce.KLMK)
			skillVal = cache.m_fFFRX_AISkillKLMK;

		// Niveau de l'escouade : une patrouille de bleus tire moins bien qu'une escouade
		// aguerrie. C'est ce qui donne de la variete entre deux rencontres, y compris sur
		// un theatre ou toutes les unites appartiennent a la meme force (cf.
		// FFRX_SquadTier.c). Clamp : le decalage ne doit pas sortir de l'echelle 0-100.
		int tier = FFRX_SquadTier.TierOf(owner);
		skillVal = Math.Clamp(skillVal + FFRX_SquadTier.SkillOffset(tier), 0, 100);

		SetAISkill(FFRX_SkillFromFloat(skillVal));

		// Do NOT call SetPerceptionFactor() here: it dereferences
		// m_Utility.m_ThreatSystem, which is still null while the AI is being
		// loaded (crash). Set the multiplier field directly instead; the combat
		// component applies it on its own perception updates once running.
		//
		// La nuit vient moduler ce facteur : l'ennemi sans optique de vision nocturne
		// voit moins bien, celui qui en porte garde sa vue de jour. Le moteur ne fait
		// rien de tout cela tout seul (cf. FFRX_NightVision.c). Le rafraichissement au
		// lever/coucher passe par FFRX_ReapplyAll, qui repasse ici.
		// Tirage de l'optique NV : UNE SEULE FOIS par soldat. FFRX_ApplyDifficulty est
		// rappele a chaque bascule jour/nuit et a chaque changement de reglage -- retirer
		// a chaque fois ferait apparaitre et disparaitre les jumelles au fil de la nuit.
		if (!m_bFFRX_NVGRolled)
		{
			m_bFFRX_NVGRolled = true;

			// Deux sources, et la premiere prime : si la DOTATION lui a reellement donne
			// une optique NV (TacticalFlava en equipe deja quelques profils USSR), il voit
			// la nuit, point -- sinon on aurait un soldat portant visiblement une Dedal et
			// aveugle malgre tout. Le tirage au pourcentage ne fait qu'ajouter des porteurs
			// par-dessus ce que les prefabs fournissent deja.
			// On garde le detail des deux sources : savoir combien de porteurs viennent
			// de la DOTATION et combien du tirage est ce qui permet de regler le curseur
			// en connaissance de cause (cf. FFRX_NightVision.Render).
			bool fromLoadout = FFRX_NightVision.HasNightOptic(owner);
			m_bFFRX_HasNVG = fromLoadout || FFRX_NightVision.RollNightOptic(force, tier);
			FFRX_NightVision.NoteRoll(m_bFFRX_HasNVG, fromLoadout);

			// Designe porteur mais sans optique reelle : on lui en pose une, pour qu'elle
			// se voie sur l'arme et se recupere sur le corps. Si l'arme n'a pas de rail
			// compatible, l'objet est detruit et seul le bonus de perception subsiste.
			if (m_bFFRX_HasNVG && !fromLoadout)
				FFRX_NightVision.GrantOptic(owner);

			// Gilet suicide : meme moment, meme principe (un tirage, une fois).
			// Cf. FFRX_SuicideVest.c -- pas de multiplicateur elite ici, c'est l'arme du
			// faible et non celle d'une unite bien equipee.
			bool vest = FFRX_SuicideVest.Roll();
			FFRX_SuicideVest.NoteRoll(vest);
			if (vest)
				FFRX_SuicideVest.Register(owner);
		}

		m_fPerceptionFactor = cache.m_fFFRX_AIPerception * FFRX_NightVision.PerceptionMultiplier(m_bFFRX_HasNVG);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-apply to every living enemy combat component (called on settings change).
	static void FFRX_ReapplyAll()
	{
		for (int i = Registry().Count() - 1; i >= 0; i--)
		{
			SCR_AICombatComponent c = Registry()[i];
			if (c && c.GetOwner())
			{
				c.FFRX_ApplyDifficulty();
				c.FFRX_InvalidateLauncherPriority();
			}
			else
				Registry().Remove(i);
		}
	}

	// ================================================================================================
	// Tracantes : acces au registre + boost temporaire de perception (cf. FFRX_Tracer.c).
	//
	// Le registre est `protected` et doit le rester -- on n'expose qu'une lecture. Passer
	// par le registre evite une requete spatiale : la liste des ennemis vivants existe
	// deja, il suffit de la filtrer par distance.
	// ================================================================================================

	//! Echeance du boost "tracante", en ms moteur. 0 = aucun boost en cours.
	protected float m_fFFRX_TracerUntil_ms;

	//------------------------------------------------------------------------------------------------
	static array<SCR_AICombatComponent> FFRX_Registry()
	{
		return s_FFRX_Registry;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce soldat appartient-il a une force ENNEMIE ? (meme classement que la difficulte :
	//! NONE = allie, civil, ou prefab inconnu -- on n'y touche pas.)
	bool FFRX_IsEnemyCombatant()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		EntityPrefabData pd = owner.GetPrefabData();
		if (!pd)
			return false;

		return FFRX_ClassifyPrefab(pd.GetPrefabName()) != FFRX_EForce.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Une tracante vient d'eclairer la position du tireur : ce soldat le repere plus vite.
	//! Le multiplicateur n'est applique QU'UNE FOIS -- une rafale de mitrailleuse appellerait
	//! sinon cette methode des dizaines de fois et ferait exploser le facteur. Les tirs
	//! suivants ne font que repousser l'echeance, ce qui est le comportement voulu : tant
	//! qu'il tire, il reste eclaire.
	void FFRX_SetTracerBoost(float mult, float untilMs)
	{
		if (m_fFFRX_TracerUntil_ms <= 0)
			m_fPerceptionFactor = m_fPerceptionFactor * mult;

		m_fFFRX_TracerUntil_ms = untilMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Rend sa perception normale si l'echeance est passee. Retourne true si on a rendu la main.
	bool FFRX_ExpireTracerBoost(float nowMs)
	{
		if (m_fFFRX_TracerUntil_ms <= 0)
			return false;
		if (nowMs < m_fFFRX_TracerUntil_ms)
			return false;

		m_fFFRX_TracerUntil_ms = 0;
		// On ne soustrait pas le bonus : on RECALCULE la valeur propre. Une bascule
		// jour/nuit ou un changement de reglage a pu passer entre-temps, auquel cas une
		// soustraction laisserait une valeur fausse pour le reste de la partie.
		FFRX_ApplyDifficulty();
		return true;
	}

	// ================================================================================================
	// AT / AA specialists: make launcher carriers actually prioritise vehicles.
	//
	// Vanilla target score is `offset + slope * distance` per unit type (see
	// InitWeaponTargetSelector): infantry 100/-0.1, unarmored 99/-0.08, medium 150/-0.15,
	// heavy 200/-0.15, aircraft 90/-0.015. Because of the steep vehicle slopes, past a few
	// hundred metres a rifleman outscores a tank, so the AT guy keeps plinking infantry
	// with his rifle instead of firing his RPG.
	//
	// For any enemy carrying a rocket launcher we add a flat bonus to every vehicle/aircraft
	// type and flatten their slope, so an armoured target stays the best option out to the
	// launcher's real range. The selector still refuses targets the carried weapons cannot
	// hurt, so an AT-only soldier will not "select" an aircraft he cannot engage.
	// ================================================================================================

	protected bool m_bFFRX_LauncherBoostApplied;
	protected float m_fFFRX_NextLauncherCheck_ms;

	// Vision nocturne (cf. FFRX_NightVision.c) : tire une fois, puis fige pour la vie du
	// soldat. Le booleen "Rolled" est indispensable -- sans lui, chaque rafraichissement
	// de perception relancerait le de.
	protected bool m_bFFRX_NVGRolled;
	protected bool m_bFFRX_HasNVG;

	//------------------------------------------------------------------------------------------------
	//! True when this AI belongs to the hostile side, whatever the theatre (USSR, MEI, ...).
	//! Right after spawn the faction may not be assigned yet -> returns false, and the periodic
	//! re-check picks it up a few seconds later.
	bool FFRX_IsEnemyAI()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		JWK_FactionManager factions = JWK.GetFactions();
		if (!factions)
			return false;

		return factions.GetEntityRole(owner) == JWK_EFactionRole.ENEMY;
	}

	//! Force the next EvaluateWeaponAndTarget to re-decide (settings changed).
	void FFRX_InvalidateLauncherPriority()
	{
		m_fFFRX_NextLauncherCheck_ms = 0;
		FFRX_UpdateLauncherPriority(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-check periodically: the loadout is not there yet when EOnInit runs, and the AI can
	//! pick up (or use up) a launcher during its life.
	protected void FFRX_UpdateLauncherPriority(bool force = false)
	{
		World world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (!force && now < m_fFFRX_NextLauncherCheck_ms)
			return;

		m_fFFRX_NextLauncherCheck_ms = now + 10000; // 10 s

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return;

		float bonus = cache.m_fFFRX_ATPriority;

		// Launcher carriers, plus anyone manning a turret: a turret gun can actually hurt a
		// vehicle, unlike a rifle, so those gunners should prefer vehicles too. The selector
		// still drops targets the mounted weapon cannot damage.
		bool armedForVehicles = HasWeaponOfType(EWeaponType.WT_ROCKETLAUNCHER);
		if (!armedForVehicles && m_AIInfo)
			armedForVehicles = m_AIInfo.HasUnitState(EUnitState.IN_TURRET);

		bool wantBoost = bonus > 0 && armedForVehicles;

		// Only enemy forces get the buff - friendly/civilian AI keeps vanilla behaviour.
		// Faction role, not the prefab path: this covers USSR on Everon, MEI in the desert
		// theatre and any enemy faction added later, with no per-force list to maintain.
		if (wantBoost && !FFRX_IsEnemyAI())
			wantBoost = false;

		if (!force && wantBoost == m_bFFRX_LauncherBoostApplied)
			return;

		m_bFFRX_LauncherBoostApplied = wantBoost;
		FFRX_ApplyTargetScores(bonus);
		SetTargetSelectionProperties(false);

		if (wantBoost)
		{
			string what = "porteur de lanceur";
			if (m_AIInfo && m_AIInfo.HasUnitState(EUnitState.IN_TURRET))
				what = "servant de tourelle";

			Print("[FFRX][AI] Priorite vehicules activee sur un ennemi (" + what
				+ ") - bonus " + bonus.ToString() + ", portee " + cache.m_fFFRX_ATRange.ToString() + "m.");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_ApplyTargetScores(float bonus)
	{
		if (!m_bFFRX_LauncherBoostApplied)
		{
			// Restore vanilla values (InitWeaponTargetSelector).
			m_WeaponTargetSelector.SetTargetScoreConstants(EAIUnitType.UnitType_VehicleUnarmored,  99.0, -0.08);
			m_WeaponTargetSelector.SetTargetScoreConstants(EAIUnitType.UnitType_VehicleMedium,    150.0, -0.15);
			m_WeaponTargetSelector.SetTargetScoreConstants(EAIUnitType.UnitType_VehicleHeavy,     200.0, -0.15);
			m_WeaponTargetSelector.SetTargetScoreConstants(EAIUnitType.UnitType_Aircraft,          90.0, -0.015);
			return;
		}

		// Flattened slopes so the priority holds at range, not just under 200 m.
		m_WeaponTargetSelector.SetTargetScoreConstants(EAIUnitType.UnitType_VehicleUnarmored,  99.0 + bonus, -0.04);
		m_WeaponTargetSelector.SetTargetScoreConstants(EAIUnitType.UnitType_VehicleMedium,    150.0 + bonus, -0.05);
		m_WeaponTargetSelector.SetTargetScoreConstants(EAIUnitType.UnitType_VehicleHeavy,     200.0 + bonus, -0.05);
		m_WeaponTargetSelector.SetTargetScoreConstants(EAIUnitType.UnitType_Aircraft,          90.0 + bonus, -0.015);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla caps vehicle targets at TARGET_MAX_DISTANCE_VEHICLE (700 m). Launcher carriers
	//! get the configured range instead. Overridden because the engine re-calls this on every
	//! close-combat transition, which would wipe our value.
	override void SetTargetSelectionProperties(bool closeCombat)
	{
		super.SetTargetSelectionProperties(closeCombat);

		if (!m_bFFRX_LauncherBoostApplied)
			return;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return;

		float vehRange = Math.Max(cache.m_fFFRX_ATRange, TARGET_MAX_DISTANCE_VEHICLE);

		if (closeCombat)
		{
			m_WeaponTargetSelector.SetSelectionProperties(TARGET_MAX_LAST_SEEN_DIRECT_ATTACK_CLOSE, TARGET_MAX_LAST_SEEN_INDIRECT_ATTACK_CLOSE, TARGET_MAX_LAST_SEEN_INDIRECT_ATTACK_CLOSE,
				TARGET_MIN_INDIRECT_TRACE_FRACTION_MIN, TARGET_MAX_DISTANCE_INFANTRY, vehRange, TARGET_MAX_TIME_SINCE_ENDANGERED, TARGET_MAX_DISTANCE_DISARMED);
			return;
		}

		m_WeaponTargetSelector.SetSelectionProperties(TARGET_MAX_LAST_SEEN_DIRECT_ATTACK, TARGET_MAX_LAST_SEEN_INDIRECT_ATTACK, TARGET_MAX_LAST_SEEN,
			TARGET_MIN_INDIRECT_TRACE_FRACTION_MIN, TARGET_MAX_DISTANCE_INFANTRY, vehRange, TARGET_MAX_TIME_SINCE_ENDANGERED, TARGET_MAX_DISTANCE_DISARMED);
	}

	//------------------------------------------------------------------------------------------------
	override void EvaluateWeaponAndTarget(out bool outWeaponEvent, out bool outSelectedTargetChanged,
		out BaseTarget outPrevTarget, out BaseTarget outCurrentTarget,
		out bool outRetreatTargetChanged, out bool outCompartmentChanged)
	{
		FFRX_UpdateLauncherPriority();
		super.EvaluateWeaponAndTarget(outWeaponEvent, outSelectedTargetChanged, outPrevTarget, outCurrentTarget,
			outRetreatTargetChanged, outCompartmentChanged);

		if (FFRX_TryHighExplosive())
		{
			outWeaponEvent = true;
			outCurrentTarget = m_SelectedTarget;
		}
	}

	// ================================================================================================
	// Tank cannon vs infantry.
	//
	// The engine's weapon selector picks the coaxial MG against infantry and keeps the main gun
	// for armour -- sensible, but it means a tank parked in front of the players is only ever a
	// machine gun. We occasionally override that and send an HE shell instead.
	//
	// Trigger: the vanilla pass already chose a MACHINEGUN (so the target IS soft -- no need to
	// query unit types), the gunner sits in a turret that also has a cannon, and either
	//   * several enemies are grouped in the same target cluster  -> always worth a shell, or
	//   * a dice roll passes (FFRX_HE_CHANCE).
	// A cooldown (FFRX_HE_COOLDOWN) keeps a tank from emptying its ammo rack on infantry.
	//
	// Implementation: re-run the selector with MACHINEGUN blacklisted, which makes it fall back
	// to the cannon whatever its actual weapon type is, then apply the result the same way the
	// vanilla pass does. If the selector finds nothing, we simply keep the MG choice.
	// ================================================================================================

	// ⚠️ Pas d'initialiseur immediat sur un champ statique (meme avec des valeurs) : ils
	// sont hisses dans UNE fonction d'init partagee par vanilla et TOUS les mods, dont le
	// buffer de 64 Ko deborde en "Too many instructions per function" sur des fichiers
	// innocents. Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<EWeaponType> s_aFFRX_BlacklistMG;

	protected static array<EWeaponType> BlacklistMG()
	{
		if (!s_aFFRX_BlacklistMG)
		{
			s_aFFRX_BlacklistMG = new array<EWeaponType>();
			s_aFFRX_BlacklistMG.Insert(EWeaponType.WT_MACHINEGUN);
		}

		return s_aFFRX_BlacklistMG;
	}
	protected static const int HE_CLUSTER_MIN = 3;      //!< grouped enemies that always deserve a shell
	protected static const float HE_REROLL_MS = 4000;   //!< do not re-roll the dice every evaluation

	protected float m_fFFRX_NextHE_ms;

	//------------------------------------------------------------------------------------------------
	protected bool FFRX_TryHighExplosive()
	{
		// Only when the vanilla pass settled on a machine gun, from a turret.
		if (!m_SelectedWeaponComp || !m_SelectedTarget)
			return false;

		if (m_SelectedWeaponComp.GetWeaponType() != EWeaponType.WT_MACHINEGUN)
			return false;

		if (!m_AIInfo || !m_AIInfo.HasUnitState(EUnitState.IN_TURRET))
			return false;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache || cache.m_fFFRX_HeChance <= 0)
			return false;

		if (!FFRX_IsEnemyAI())
			return false;

		// NOTE: deliberately no "does it have a cannon of type X" pre-check. Modded tanks do not
		// all type their main gun the same way (WT_AUTOCANNON is not guaranteed), so instead we
		// just ask the selector for its best non-MG option below and back off if there is none.
		// Slightly more expensive, but it works with any vehicle from any addon.

		World world = GetGame().GetWorld();
		if (!world)
			return false;

		float now = world.GetWorldTime();
		if (now < m_fFFRX_NextHE_ms)
			return false;

		array<IEntity> assignedTargets;
		if (m_TargetClusterState && m_TargetClusterState.m_Cluster && m_TargetClusterState.m_Cluster.m_aEntities)
			assignedTargets = m_TargetClusterState.m_Cluster.m_aEntities;
		else
			assignedTargets = m_aAssignedTargets;

		bool grouped = assignedTargets && assignedTargets.Count() >= HE_CLUSTER_MIN;
		if (!grouped && Math.RandomFloat01() * 100.0 > cache.m_fFFRX_HeChance)
		{
			// Lost the roll: wait a bit before rolling again, otherwise the dice would be
			// thrown on every evaluation and the shot would become near-certain.
			m_fFFRX_NextHE_ms = now + HE_REROLL_MS;
			return false;
		}

		if (!assignedTargets)
			assignedTargets = {};

		bool ok = m_WeaponTargetSelector.SelectWeaponAndTarget(assignedTargets,
			ASSIGNED_TARGETS_SCORE_INCREMENT, ENDANGERING_TARGETS_SCORE_INCREMENT,
			true, weaponTypesBlacklist: BlacklistMG());

		if (!ok)
			return false;

		BaseWeaponComponent weaponComp;
		BaseMagazineComponent magazineComp;
		int muzzleId;
		m_WeaponTargetSelector.GetSelectedWeapon(weaponComp, muzzleId, magazineComp);

		// Nothing better than the MG after all -> leave the vanilla choice alone.
		if (!weaponComp || weaponComp == m_SelectedWeaponComp)
		{
			// Logged: this is THE case to look at if HE shells never happen. It means the
			// turret has no usable alternative to the coax (no cannon, no shells, or the
			// cannon cannot engage this target).
			Print("[FFRX][AI] Obus infanterie: aucune arme alternative a la mitrailleuse sur cette tourelle -> on garde la MG.");
			m_fFFRX_NextHE_ms = now + HE_REROLL_MS;
			return false;
		}

		m_WeaponTargetSelector.GetSelectedWeaponProperties(m_fSelectedWeaponMinDist, m_fSelectedWeaponMaxDist, m_bSelectedWeaponDirectDamage);

		array<BaseMuzzleComponent> muzzles = {};
		weaponComp.GetMuzzlesList(muzzles);
		if (muzzleId < 0 || muzzleId >= muzzles.Count())
			m_SelectedWeaponResource = m_ConfigComponent.GetTreeNameForWeaponType(weaponComp.GetWeaponType(), 0);
		else
			m_SelectedWeaponResource = m_ConfigComponent.GetTreeNameForWeaponType(weaponComp.GetWeaponType(), muzzles[muzzleId].GetMuzzleType());

		m_SelectedWeaponComp   = weaponComp;
		m_iSelectedMuzzle      = muzzleId;
		m_SelectedMagazineComp = magazineComp;
		m_SelectedTarget       = m_WeaponTargetSelector.GetSelectedTarget();
		m_eUnitTypesCanAttack  = m_WeaponTargetSelector.GetUnitTypesCanAttack();

		m_fFFRX_NextHE_ms = now + cache.m_fFFRX_HeCooldown * 1000.0;

		string reason = "des reussi";
		if (grouped)
			reason = "cibles groupees (" + assignedTargets.Count().ToString() + ")";

		Print("[FFRX][AI] Obus sur infanterie TIRE (" + reason + ") - arme type "
			+ weaponComp.GetWeaponType().ToString() + ", prochain obus dans "
			+ cache.m_fFFRX_HeCooldown.ToString() + "s.");

		return true;
	}

	// ================================================================================================
	// Enemy vehicle crews: hold the turret.
	//
	// Vanilla defect: a turret gunner with no driver aboard, whose target drifts outside the
	// turret's traverse limits (TURRET_TARGET_EXCESS_ANGLE_THRESHOLD_DEG = 3 deg), dismounts
	// after 1.2 s to go investigate ON FOOT -- he abandons a heavy MG for his rifle and stops
	// being a threat. Vanilla only forbids this for APCs (s_aForbidDismountTurretsOfVehicleTypes).
	//
	// That is the single biggest reason enemy vehicles feel harmless: park next to a technical
	// and its gunner climbs out. We keep enemy gunners at their post; the vehicle stays a threat
	// and the player has to actually deal with it.
	//
	// Friendly/civilian AI is untouched, and the crew still dismounts for the reasons handled
	// elsewhere by the engine (vehicle on fire, damage evac -- see Event_OnDamage).
	// ================================================================================================

	//------------------------------------------------------------------------------------------------
	override bool DismountTurretCondition(inout vector targetPos, bool targetPosProvided, out float threatPriority)
	{
		if (FFRX_ShouldHoldTurret())
			return false;

		return super.DismountTurretCondition(targetPos, targetPosProvided, threatPriority);
	}

	//------------------------------------------------------------------------------------------------
	protected bool m_bFFRX_HoldTurretLogged;

	protected bool FFRX_ShouldHoldTurret()
	{
		if (!m_CurrentTurretController)
			return false;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache || cache.m_fFFRX_VehCrewHold < 0.5)
			return false;

		if (!FFRX_IsEnemyAI())
			return false;

		// Once per gunner: DismountTurretCondition is polled continuously.
		if (!m_bFFRX_HoldTurretLogged)
		{
			m_bFFRX_HoldTurretLogged = true;
			Print("[FFRX][AI] Servant de tourelle ennemi maintenu a son poste (debarquement vanilla annule).");
		}

		return true;
	}
}

// ====================================================================================================
// Enemy reaction time.
//
// Vanilla pre-fire delay (SCR_AIAttackBehavior.InitWaitTime) = threatDelay + distanceDelay:
//   threatDelay   0.25 s when surprised, 0 s when already alert, 0.8 s when overwhelmed
//   distanceDelay 1.4*d / (300+d)  ->  ~0.34 s at 100 m, ~0.70 s at 300 m, ~1.0 s at 800 m
// So an unaware enemy at 300 m stands there for nearly a full second before firing back.
//
// Note this only applies when the AI had NO previous target: vanilla already fires instantly
// when merely switching between known targets (m_fWaitTime = 0 in the constructor). So this
// scales exactly the "caught by surprise" reaction, which is the one that feels too soft.
//
// Idea credit: "CRX Enfusion A.I." by ATiM- (see header). Implementation is our own.
// ====================================================================================================

modded class SCR_AIAttackBehavior
{
	override protected void InitWaitTime(SCR_AIUtilityComponent utility)
	{
		super.InitWaitTime(utility);

		if (!m_CombatComponent || !m_CombatComponent.FFRX_IsEnemyAI())
			return;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return;

		m_fWaitTime.m_Value = m_fWaitTime.m_Value * cache.m_fFFRX_ReactionDelay;
	}

	//------------------------------------------------------------------------------------------------
	//! Contact-range reflex: attacking must outrank any move order. Paired with the move
	//! behaviours standing down in FFRX_AIReflex.c (credit: AIReflexFire by cjcn).
	override float CustomEvaluate()
	{
		float score = super.CustomEvaluate();

		// 3e garde-fou d'AIReflexFire 0.1.5 : un score nul ou negatif signifie que le vanilla
		// a DEJA juge l'attaque impossible (cible non retenue par le selecteur d'arme, occupant
		// d'un vehicule hors tourelle...). Y ajouter le bonus forcerait un etat invalide.
		if (score <= 0)
			return score;

		if (FFRX_Reflex.HasCloseContact(m_Utility))
			score += FFRX_Reflex.ATTACK_SCORE_BONUS;

		return score;
	}
}
