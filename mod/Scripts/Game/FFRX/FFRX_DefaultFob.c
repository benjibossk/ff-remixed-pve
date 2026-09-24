// FF - REMIXED - PVE
// Default starting FOB.
//
// Idea (Benji): instead of seeding one friendly starting town, start with a single
// functional player FOB placed "a bit far from everything", and 0 supporters on
// every town (see Brique A / FFRX_SingleStartingTown). The whole map is then
// contested/enemy at launch and the FOB is the sole spawn (MOB > FOB > Town, towns
// at 0 supporters aren't spawnable anyway).
//
// HOW: JWK_PlayerFOBController.et is a fully functional player FOB out of the box --
// it carries JWK_ResistanceResourceRespawnLocationComponent (respawn, charged in
// SUPPLIES), a logistics storage, PLAYER faction control, a build area, map icon and
// EPF persistence. So we just spawn that prefab at a good spot and seed it with
// supplies. Placement anchors reuse the map's pre-placed SCR_SiteSlotEntity slots
// (already scattered across Everon for base building) -- we pick the free slot
// FARTHEST from every controlled POI.
//
// Server only. Runs ONCE per fresh campaign: the FOB has EPF persistence, so on a
// saved campaign it reloads itself -> we skip when HasSaveGame() is true.
// ASCII only in strings (Enforce desyncs on UTF-8 accents).

class FFRX_DefaultFob
{
	static const ResourceName FOB_PREFAB =
		"{5EB765AB8C227E5A}Prefabs/Controllers/Runtime/JWK_PlayerFOBController.et";

	// Supplies seeded into the FOB so players can respawn from it at launch.
	static const int STARTING_SUPPLIES = 5000;

	// A candidate slot must be at least this far from every POI to qualify as
	// "far from everything" (matches the FOB placement-deny range).
	static const float MIN_DISTANCE_FROM_POI = 800;

	protected static bool s_bDone;

	//! La FOB de depart, une fois posee. Pas de `ref` : c'est une entite du monde.
	protected static IEntity s_FobEntity;

	//------------------------------------------------------------------------------------------------
	//! Position de la base de depart. \return false tant qu'elle n'est pas posee.
	static bool FFRX_GetFobPos(out vector outPos)
	{
		if (!s_FobEntity || s_FobEntity.IsDeleted())
			return false;

		outPos = s_FobEntity.GetOrigin();
		return true;
	}
	protected static int s_iAttempts;
	protected static int s_iSeedAttempts;
	protected static ref array<SCR_SiteSlotEntity> s_aSlots;

	// Called from SCR_BaseGameMode.OnGameModeStart (server). Defers the real work so
	// the FF systems / territory index are up, then retries a few times if not.
	static void Boot()
	{
		if (!Replication.IsServer()) return;

		// IMPORTANT: statics survive a mission RESTART (the script VM is not reset),
		// but the placed FOB is destroyed by the mission reload. So reset our run
		// state every game-mode start; the real anti-duplicate guard is the in-world
		// "does a FOB already exist?" check inside TryPlace().
		s_bDone = false;
		s_iAttempts = 0;

		// Place early so the resistance has a foothold before FF evaluates defeat
		// conditions (0 supporters everywhere means the FOB is the only PLAYER node).
		// TryPlace() retries itself until the world/territory index is ready.
		GetGame().GetCallqueue().CallLater(TryPlace, 3000, false);
	}

	// Territory/POI index not ready yet -> try again shortly (bounded).
	protected static void RetryLater()
	{
		s_iAttempts = s_iAttempts + 1;
		if (s_iAttempts > 20)
		{
			Print("[FFRX][FOB] World/POI index never became ready -> aborting default FOB.", LogLevel.WARNING);
			return;
		}
		GetGame().GetCallqueue().CallLater(TryPlace, 3000, false);
	}

	protected static void TryPlace()
	{
		if (s_bDone) return;

		World world = GetGame().GetWorld();
		if (!world) { RetryLater(); return; }

		// Robust, self-healing gate: place a default FOB only when NONE exists yet.
		// - fresh campaign            -> no FOB -> place one.
		// - reload with persisted FOB -> FOB present -> skip (it reloaded itself).
		// - reload where the FOB was lost/wiped -> none -> place again.
		// This replaces HasSaveGame() gating, which was unreliable (the FF persistence
		// DB can report a save while the FOB is actually absent -> we skipped forever).
		array<EntityID> fobs = JWK_IndexSystem.Get(world).GetAll(JWK_PlayerFobEntity);
		if (fobs && !fobs.IsEmpty())
		{
			s_bDone = true;
			Print(string.Format("[FFRX][FOB] A FOB already exists (%1) -> not placing a default one.", fobs.Count()), LogLevel.NORMAL);
			return;
		}

		// Need the territory/faction index up to measure "far from POIs".
		array<GenericComponent> pois;
		if (JWK.GetFactions())
			pois = JWK_IndexSystem.Get(world).GetAllGC(JWK_FactionControlComponent);

		bool poisReady = pois && !pois.IsEmpty();
		if (!poisReady) { RetryLater(); return; }

		vector mat[4];
		Math3D.MatrixIdentity4(mat);

		SCR_SiteSlotEntity slot = PickFarthestSlot(world, pois);
		if (slot)
		{
			slot.GetWorldTransform(mat);
		}
		else
		{
			// Fallback: no usable map slot (e.g. a stripped/modified test world) -> place at
			// a computed safe spot so there is ALWAYS a starting FOB to spawn from.
			vector fb = FallbackPos(world);
			if (fb == vector.Zero)
			{
				Print("[FFRX][FOB] No slot and no fallback position -> aborting default FOB.", LogLevel.WARNING);
				return;
			}
			mat[3] = fb;
			Print(string.Format("[FFRX][FOB] No usable slot -> fallback placement at (%1, %2).",
				(int)fb[0], (int)fb[2]), LogLevel.WARNING);
		}

		IEntity fob = JWK_SpawnUtils.SpawnEntityPrefabMatrix(FOB_PREFAB, mat);
		if (!fob)
		{
			Print("[FFRX][FOB] Spawn of FOB prefab FAILED.", LogLevel.ERROR);
			return;
		}

		s_iSeedAttempts = 0;
		SeedSuppliesDeferred(fob);
		s_bDone = true;

		// Memorise la FOB : d'autres systemes ont besoin de savoir OU est la base de depart
		// plutot que de recalculer un emplacement (aujourd'hui le PNJ guide, qui se place
		// pres du drapeau -- cf. FFRX_GuideNPC.c).
		s_FobEntity = fob;

		vector p = fob.GetOrigin();
		string slotName = "fallback";
		if (slot) slotName = slot.GetName();
		Print(string.Format("[FFRX][FOB] Default FOB placed at slot '%1' (%2, %3).",
			slotName, (int)p[0], (int)p[2]), LogLevel.NORMAL);
	}

	// The free slot maximizing its distance to the NEAREST POI (i.e. deepest in the
	// wild). Falls back to the overall farthest if none clears MIN_DISTANCE_FROM_POI.
	protected static SCR_SiteSlotEntity PickFarthestSlot(World world, array<GenericComponent> pois)
	{
		// Map-agnostic: derive the query sphere from FF's per-map world bounds
		// (JWK_WorldEntity map offset + size), so this works on Arland, Everon or any
		// future map with no hardcoded coordinates. Fallback covers any map from the
		// terrain origin corner if the world entity isn't available.
		vector center;
		float radius;
		JWK_WorldEntity we = JWK_WorldEntity.Get();
		if (we)
		{
			vector size = we.GetMapSize();
			center = we.GetMapOffset() + size * 0.5;
			radius = size.Length() * 0.5 + 100;
		}
		else
		{
			center = "0 0 0";
			radius = 60000;
		}

		s_aSlots = new array<SCR_SiteSlotEntity>();
		world.QueryEntitiesBySphere(center, radius, CollectSlot, null, EQueryEntitiesFlags.STATIC);
		if (s_aSlots.IsEmpty()) return null;

		// Only consult occupancy when the composition slot manager exists; calling
		// IsOccupied() without it logs an error PER slot (hundreds of them).
		bool canCheckOccupied = (SCR_CompositionSlotManagerComponent.GetInstance() != null);

		SCR_SiteSlotEntity best;
		float bestScore = -1;
		foreach (SCR_SiteSlotEntity slot : s_aSlots)
		{
			if (canCheckOccupied && slot.IsOccupied()) continue;

			vector sp = slot.GetOrigin();
			float nearest = float.MAX;
			foreach (GenericComponent gc : pois)
			{
				JWK_FactionControlComponent fc = JWK_FactionControlComponent.Cast(gc);
				if (!fc || !fc.GetOwner()) continue;
				float d = vector.Distance(sp, fc.GetOwner().GetOrigin());
				if (d < nearest) nearest = d;
			}

			// "distance to nearest POI" is the score; bigger = more remote.
			if (nearest > bestScore)
			{
				bestScore = nearest;
				best = slot;
			}
		}

		if (best && bestScore < MIN_DISTANCE_FROM_POI)
			Print(string.Format("[FFRX][FOB] Farthest slot is only %1 m from a POI (< %2).",
				(int)bestScore, (int)MIN_DISTANCE_FROM_POI), LogLevel.WARNING);

		return best;
	}

	protected static bool CollectSlot(IEntity e)
	{
		SCR_SiteSlotEntity slot = SCR_SiteSlotEntity.Cast(e);
		if (slot) s_aSlots.Insert(slot);
		return true; // keep querying
	}

	// Seed supplies AFTER the FOB's storage capacity is computed. Right after spawn the
	// build-area/logistics storage often reports max 0 (not initialized yet) -> AddResources
	// is a no-op and the FOB stays empty/unspawnable. So we retry once a second until the
	// storage has a real capacity (or give up, leaving free-respawn to cover it).
	protected static void SeedSuppliesDeferred(IEntity fob)
	{
		if (!fob) return;

		JWK_LogisticsAreaStorageControllerComponent store =
			JWK_CompTU<JWK_LogisticsAreaStorageControllerComponent>.FindIn(fob);
		if (!store)
		{
			Print("[FFRX][FOB] FOB has no logistics storage -> cannot seed supplies.", LogLevel.WARNING);
			return;
		}

		int max = store.GetMaxResources(JWK_ELogisticsResourceType.SUPPLIES);
		if (max <= 0)
		{
			s_iSeedAttempts = s_iSeedAttempts + 1;
			if (s_iSeedAttempts <= 20)
			{
				GetGame().GetCallqueue().CallLater(SeedSuppliesDeferred, 1000, false, fob);
				return;
			}
			Print("[FFRX][FOB] FOB storage max still 0 after retries -> respawn relies on free-respawn.", LogLevel.WARNING);
			return;
		}

		int before = store.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
		store.AddResources(JWK_ELogisticsResourceType.SUPPLIES, STARTING_SUPPLIES);
		int after = store.GetResources(JWK_ELogisticsResourceType.SUPPLIES);
		Print(string.Format("[FFRX][FOB] Supplies seeded (attempt %1): %2 -> %3 (max %4).",
			s_iSeedAttempts, before, after, max), LogLevel.NORMAL);
	}

	// A safe FOB position when the world has no usable SCR_SiteSlotEntity: the map centre
	// (via JWK_WorldEntity, else terrain origin), snapped to the ground surface.
	protected static vector FallbackPos(World world)
	{
		vector center = "0 0 0";
		JWK_WorldEntity we = JWK_WorldEntity.Get();
		if (we)
			center = we.GetMapOffset() + we.GetMapSize() * 0.5;

		center[1] = world.GetSurfaceY(center[0], center[2]);
		return center;
	}

	// -------------------------------------------------------------------------------------------------
	// Admin relocate: move the starting FOB to a chosen ground position (the #placefob command below).
	// Deletes EVERY existing player FOB first (the random default one), then spawns a fresh FOB there
	// and re-seeds supplies. Meant for the very start of a new campaign, before players build up.
	// Returns true on success. Server only.
	static bool RelocateTo(vector pos)
	{
		if (!Replication.IsServer()) return false;

		World world = GetGame().GetWorld();
		if (!world) return false;

		// 1) Remove the existing FOB(s) (the auto-placed random one).
		int removed = 0;
		array<EntityID> fobs = JWK_IndexSystem.Get(world).GetAll(JWK_PlayerFobEntity);
		if (fobs)
		{
			foreach (EntityID id : fobs)
			{
				IEntity e = world.FindEntityByID(id);
				if (e)
				{
					SCR_EntityHelper.DeleteEntityAndChildren(e);
					removed = removed + 1;
				}
			}
		}

		// 2) Snap to the ground surface at the requested X/Z (the caller stands on the ground).
		pos[1] = world.GetSurfaceY(pos[0], pos[2]);

		// 3) Spawn a fresh FOB there (identity rotation).
		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = pos;
		IEntity fob = JWK_SpawnUtils.SpawnEntityPrefabMatrix(FOB_PREFAB, mat);
		if (!fob)
		{
			Print("[FFRX][FOB] Relocate: spawn of FOB prefab FAILED.", LogLevel.ERROR);
			return false;
		}

		// 4) Seed supplies + stop the auto-placer from adding another one.
		s_bDone = true;
		s_iSeedAttempts = 0;
		SeedSuppliesDeferred(fob);

		Print(string.Format("[FFRX][FOB] FOB relocated to (%1, %2); removed %3 old FOB(s).",
			(int)pos[0], (int)pos[2], removed), LogLevel.NORMAL);
		return true;
	}
}

//----------------------------------------------------------------------------------------------------
// Admin command "#placefob": delete the auto-placed random starting FOB and spawn a new one AT THE
// CALLER'S FEET (ground level). For repositioning the first-arrival FOB at the start of a campaign.
[BaseContainerProps()]
class FFRX_PlaceFobCommand : ScrServerCommand
{
	override string GetKeyword() { return "placefob"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Place-toi a l'endroit voulu (incarne un perso).", EServerCmdResultType.ERR);

		if (!FFRX_DefaultFob.RelocateTo(ent.GetOrigin()))
			return ScrServerCmdResult("Echec du placement de la FOB (voir logs).", EServerCmdResultType.ERR);

		return ScrServerCmdResult("FOB de depart replacee ici ; ancienne FOB supprimee.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement (place-toi ou tu veux la FOB).", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
