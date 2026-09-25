// FF - REMIXED - PVE
// Pillar 6/7 (air, Brique 1): ENEMY air RESUPPLY. Periodically an enemy helicopter flies
// to an enemy-held FF point, LANDS to drop a reinforcement squad (cargo troops disembark)
// and deliver SUPPLIES to that point's logistics storage, then flies away. Because it is a
// slow, visible target, players can SHOOT IT DOWN to cut the enemy's air logistics
// (D5 "vulnerable logistics", from the enemy side).
//
// We drive DARC's SDRC_ChopperComp directly -- the component is explicitly built to be
// mod-driven ("call Ready() yourself after Setup()"). We mirror DARC's own chopper-mission
// spawn sequence (autostart OFF -> Setup -> crew -> Ready -> InitFlight + AddDestination).
//
// Server only. ASCII in strings.
class FFRX_EnemyAirResupply
{
	// DARC Mi8 transport (unarmed, cargo -> ideal resupply chopper). Change to another DARC
	// chopper prefab if you want. Empty = the run is skipped (logged).
	static const ResourceName CHOPPER_PREFAB = "{5BBDA2DACF9CDCA4}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_transport_Patrol.et";

	static const int   SUPPLIES_DELIVERED = 2000;    // supplies dropped at the point (if it has storage)
	static const int   FIRST_DELAY_MS     = 420000;  // first run 7 min after start
	static const int   INTERVAL_MS        = 1500000; // a resupply run every 25 min
	static const float SPEED_MIN          = 25;      // m/s
	static const float SPEED_MAX          = 55;      // m/s
	static const float FLY_LOW            = 60;      // approach altitude AGL low
	static const float FLY_HIGH           = 120;     // approach altitude AGL high
	static const float SPAWN_DIST         = 1600;    // spawn this far from the target (map edge-ish approach)
	static const float DELIVER_RADIUS     = 90;      // deliver once the chopper is this close + landed
	static const int   MAX_TICKS          = 150;     // safety despawn after ~150 * 4s = 10 min

	protected static bool s_bStarted;
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<ref FFRX_AirResupplyOp> s_aOps;

	protected static array<ref FFRX_AirResupplyOp> Ops()
	{
		if (!s_aOps)
			s_aOps = new array<ref FFRX_AirResupplyOp>();

		return s_aOps;
	}

	// Called from the game-mode start (server), next to FFRX_MarineResupply.Boot().
	static void Boot()
	{
		if (!Replication.IsServer()) return;
		if (s_bStarted) return;
		s_bStarted = true;
		GetGame().GetCallqueue().CallLater(TryResupply, FIRST_DELAY_MS, true);
	}

	// Launch one enemy air-resupply run. Also callable from #airresupply.
	static bool TryResupply()
	{
		if (!Replication.IsServer()) return false;

		if (CHOPPER_PREFAB == string.Empty)
		{
			Print("[FFRX][AirSup] Pas de CHOPPER_PREFAB defini -> run ignore. (colle le GUID d'un helico transport DARC.)", LogLevel.WARNING);
			return false;
		}

		IEntity target = PickEnemyPoint();
		if (!target)
		{
			Print("[FFRX][AirSup] Aucun point ennemi trouve -> pas de ravitaillement aerien.", LogLevel.NORMAL);
			return false;
		}
		vector targetPos = target.GetOrigin();

		// Spawn origin: offset from the target by SPAWN_DIST in a pseudo-random direction,
		// elevated to approach altitude. (index-free variation, no Math.Random dependency.)
		float ang = JWK.Random.RandFloat01() * Math.PI2;
		vector origin = targetPos;
		origin[0] = targetPos[0] + Math.Cos(ang) * SPAWN_DIST;
		origin[2] = targetPos[2] + Math.Sin(ang) * SPAWN_DIST;

		// On se DECLARE au recensement avant de spawner.
		//
		// Sans ca, nos helicos de ravitaillement atterrissent dans "non attribue" et on les
		// prend pour des appareils de DARC. C'est arrive le 2026-09-19 : un recensement
		// montrait 50 % de Mi8 DESARMES, on a modifie la liste d'helicos de DARC pour les
		// en retirer... alors qu'ils venaient d'ICI. Un transport desarme est d'ailleurs
		// le bon appareil pour du ravitaillement -- il n'y avait rien a corriger.
		FFRX_SpawnCensus.SetSourceHint("FFRX ravitaillement aerien");

		IEntity veh = SDRC_SpawnHelper.SpawnItem(origin, CHOPPER_PREFAB, 0, -1, false);
		if (!veh)
		{
			Print("[FFRX][AirSup] APPARITION ECHOUEE : l'helico n'a pas pu spawn (" + CHOPPER_PREFAB + ").", LogLevel.ERROR);
			return false;
		}
		SDRC_ChopperComp comp = SDRC_ChopperComp.Cast(veh.FindComponent(SDRC_ChopperComp));
		if (!comp)
		{
			Print("[FFRX][AirSup] Le prefab n'a pas de SDRC_ChopperComp -> ce n'est pas un helico DARC. Suppression.", LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(veh);
			return false;
		}

		int gx, gz;
		SCR_MapEntity.GetGridPos(targetPos, gx, gz);
		Print(string.Format("[FFRX][AirSup] +++ APPARITION helico ennemi -> ravitaille point ennemi grille %1-%2 (spawn a %3 m).",
			gx, gz, (int)SPAWN_DIST), LogLevel.NORMAL);

		FFRX_AirResupplyOp op = new FFRX_AirResupplyOp(veh, comp, target);
		Ops().Insert(op);
		op.Begin();
		return true;
	}

	// An enemy-controlled FF point (base/checkpoint/garrison/factory), preferring the one
	// nearest to a player (so the resupply is relevant/contestable). Returns its entity.
	protected static IEntity PickEnemyPoint()
	{
		World world = GetGame().GetWorld();
		if (!world) return null;

		array<GenericComponent> fcs = JWK_IndexSystem.Get(world).GetAllGC(JWK_FactionControlComponent);
		if (!fcs || fcs.IsEmpty()) return null;

		// Reference point = first player's position (fallback: world center-ish first enemy).
		vector refPos = FFRX_AnyPlayerPos(world);

		IEntity best;
		float bestD = float.MAX;
		bool haveRef = refPos != vector.Zero;
		foreach (GenericComponent gc : fcs)
		{
			JWK_FactionControlComponent fc = JWK_FactionControlComponent.Cast(gc);
			if (!fc || !fc.GetOwner() || !fc.Affiliation()) continue;
			if (fc.GetFactionRole() != JWK_EFactionRole.ENEMY) continue;

			IEntity owner = fc.GetOwner();
			if (!haveRef) return owner; // no players -> just take the first enemy point
			float d = vector.Distance(refPos, owner.GetOrigin());
			if (d < bestD) { bestD = d; best = owner; }
		}
		return best;
	}

	protected static vector FFRX_AnyPlayerPos(World world)
	{
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int pid : ids)
		{
			IEntity pe = GetGame().GetPlayerManager().GetPlayerControlledEntity(pid);
			if (pe) return pe.GetOrigin();
		}
		return vector.Zero;
	}

	// Deliver supplies to the target point's logistics storage (if it has one). The
	// reinforcement squad is handled automatically by WP_M_LAND_TROOPS (cargo disembark).
	static void DeliverSupplies(IEntity target)
	{
		if (!target) return;
		JWK_LogisticsStorageControllerComponent st =
			JWK_CompTU<JWK_LogisticsStorageControllerComponent>.FindIn(target);
		if (st)
		{
			int added = st.AddResources(JWK_ELogisticsResourceType.SUPPLIES, SUPPLIES_DELIVERED);
			Print(string.Format("[FFRX][AirSup] Livraison : +%1 supplies au point ennemi + renfort debarque.", added), LogLevel.NORMAL);
		}
		else
		{
			Print("[FFRX][AirSup] Point sans stockage logistique -> renfort debarque seul (pas de supplies a livrer).", LogLevel.NORMAL);
		}
	}

	// Called by an op when it finishes, to free its slot.
	static void RemoveOp(FFRX_AirResupplyOp op)
	{
		int idx = Ops().Find(op);
		if (idx != -1) Ops().Remove(idx);
	}
}

// One in-flight resupply operation. Holds its own state so the CallLater chain needs no
// entity parameters (EntityID is not a valid CallLater arg). Mirrors DARC's chopper-mission
// spawn sequence with autostart OFF.
class FFRX_AirResupplyOp
{
	protected IEntity m_Veh;
	protected EntityID m_VehID;
	protected SDRC_ChopperComp m_Comp;
	protected IEntity m_Target;
	protected vector m_TargetPos;
	protected bool m_bDelivered;
	protected int m_iTicks;

	void FFRX_AirResupplyOp(IEntity veh, SDRC_ChopperComp comp, IEntity target)
	{
		m_Veh = veh;
		m_VehID = veh.GetID();
		m_Comp = comp;
		m_Target = target;
		m_TargetPos = target.GetOrigin();
	}

	// Step 1: configure the helicopter (mirror SDRC_Mission_Chopper.MissionSpawn).
	void Begin()
	{
		m_Comp.SetAutostart(false);
		m_Comp.SetHeli(FFRX_EnemyAirResupply.SPEED_MIN, FFRX_EnemyAirResupply.SPEED_MAX,
			FFRX_EnemyAirResupply.FLY_LOW, FFRX_EnemyAirResupply.FLY_HIGH, 0.1, 0.3);
		m_Comp.SetEnemySearchType(SDRC_EHeliEnemySearchType.NONE); // transport: don't hunt, just deliver
		m_Comp.Setup(m_Veh);
		SDRC_Math.TurnEntityTowardsXZ(m_Veh, m_TargetPos);
		GetGame().GetCallqueue().CallLater(SpawnCrewReady, 1200, false);
	}

	// Step 2: spawn crew (enemy faction) + activate the component.
	void SpawnCrewReady()
	{
		if (!ResolveVeh()) { Finish(); return; }

		string factionKey = "";
		JWK_Faction enemy = JWK_Faction.GetByRole(JWK_EFactionRole.ENEMY);
		if (enemy) factionKey = enemy.GetKey();

		array<ref SCR_DefaultOccupantData> crew = {};
		SDRC_ChopperCrewHelper.SpawnCrew(m_Veh, SDRC_EHeliCargoSeatFill.HALF, crew, factionKey, EAISkill.REGULAR, 1.0);
		m_Comp.Ready(m_Veh);
		GetGame().GetCallqueue().CallLater(StartFlight, 1200, false);
	}

	// Step 3: fly to the point, land + drop troops, then leave.
	void StartFlight()
	{
		if (!ResolveVeh()) { Finish(); return; }

		// ORDRE IMPORTANT : les destinations d'ABORD, InitFlight ENSUITE.
		// DARC a retire le 2e parametre de InitFlight (avant :
		// InitFlight(owner, destination) -- maintenant : InitFlight(owner)). La
		// destination initiale n'est donc plus passee en argument : InitFlight lit
		// m_vFlyDestinations, et **si la liste est vide il invente une destination
		// aleatoire** (GetDestinationForward) puis oriente l'helico dessus. En
		// remplissant la liste avant, on retrouve exactement l'ancien comportement :
		// l'helico part et se tourne vers m_TargetPos. Les waypoints de decollage
		// qu'InitFlight ajoute (RAISE / HOVER_UP / HOVER) sont inseres en index 0,
		// donc ils passent bien AVANT les notres dans la sequence.
		m_Comp.AddDestination(SDRC_EFlyWayPointType.WP_M_LAND_TROOPS, m_TargetPos);
		m_Comp.AddDestination(SDRC_EFlyWayPointType.WP_FLY_AWAY_IMMEDIATELY);
		m_Comp.InitFlight(m_Veh);
		GetGame().GetCallqueue().CallLater(Tick, 4000, true);
	}

	// Repeating: deliver supplies when landed near the point; despawn after the safety window
	// or when the chopper is gone (shot down = enemy logistics cut, no delivery).
	void Tick()
	{
		m_iTicks++;

		if (!ResolveVeh())
		{
			if (!m_bDelivered)
				Print("[FFRX][AirSup] --- Helico ravitailleur perdu avant livraison (abattu ?) -> logistique ennemie coupee.", LogLevel.NORMAL);
			Finish();
			return;
		}

		if (!m_bDelivered)
		{
			float d = vector.Distance(m_Veh.GetOrigin(), m_TargetPos);
			SDRC_EHeliState state = m_Comp.GetState();
			// DarcChopper 1.0.28 a renomme LAND en LAND_VERTICAL. Le mod compile en meme
			// temps que nous : un membre d'enum disparu fait echouer TOUT le module Game,
			// pas seulement ce fichier -- et la premiere erreur masque toutes les autres.
			bool onGround = (state == SDRC_EHeliState.LAND_VERTICAL || state == SDRC_EHeliState.WAIT || state == SDRC_EHeliState.ON_GROUND);
			if (d < FFRX_EnemyAirResupply.DELIVER_RADIUS && onGround)
			{
				FFRX_EnemyAirResupply.DeliverSupplies(m_Target);
				m_bDelivered = true;
			}
		}

		if (m_iTicks >= FFRX_EnemyAirResupply.MAX_TICKS)
		{
			Print("[FFRX][AirSup] --- DISPARITION : fenetre ecoulee, despawn de l'helico ravitailleur.", LogLevel.NORMAL);
			if (m_Comp && m_Veh) m_Comp.DeSpawn(m_Veh);
			Finish();
		}
	}

	// Re-resolve the vehicle from its ID (safe against deletion). Returns false if gone.
	protected bool ResolveVeh()
	{
		World world = GetGame().GetWorld();
		if (!world) return false;
		m_Veh = world.FindEntityByID(m_VehID);
		if (!m_Veh) return false;
		m_Comp = SDRC_ChopperComp.Cast(m_Veh.FindComponent(SDRC_ChopperComp));
		return m_Comp != null;
	}

	protected void Finish()
	{
		GetGame().GetCallqueue().Remove(Tick);
		FFRX_EnemyAirResupply.RemoveOp(this);
	}
}
