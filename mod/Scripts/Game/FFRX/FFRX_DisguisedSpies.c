// FF - REMIXED - PVE
// Asymmetric warfare -- BRICK A: disguised enemy spies (discreet).
//
// No spawning/despawning: we use the REAL ambient civilian population. When players move among
// civilians in ENEMY / contested territory, occasionally one of those civilians turns out to be
// an infiltrator -> it FLIPS to the enemy faction and draws a weapon (same mechanism MCD already
// uses when a civilian turns hostile after being extorted: MCD_CivilianInteractionHelper
// .TurnMilitarilyHostile + GiveRandomHostileWeapon). The civilian stays the exact same character
// -- it just becomes hostile. Players learn to be wary of / frisk civilians.
//
// Rate-limited (a cooldown between reveals) + chance-gated so it stays a rare, tense surprise.
// Server-only. Booted from FFRX_Groups.c. Test command: #revealspy (unmask the nearest civilian).
//
// FRISK counter-play (search a civilian to unmask it early / safely) = later step: needs a
// user-action-on-characters mechanism.

class FFRX_DisguisedSpies
{
	protected static ref FFRX_DisguisedSpies s_Instance;

	protected static const int   FFRX_TICK_MS             = 4000;
	protected static const float FFRX_TRIGGER_DIST        = 14.0;    // a civ this close to a player may reveal
	protected static const float FFRX_SPY_CHANCE          = 0.20;    // chance a candidate civ is actually a spy
	protected static const float FFRX_REVEAL_COOLDOWN_MS  = 90000.0; // min time (ms) between spy reveals
	protected static const float FFRX_SPY_MAX_TRUST       = 55.0;    // civs you've earned trust from (> this) never betray you
	protected static const float FFRX_FRISK_SPY_CHANCE    = 0.30;    // chance a frisked (low-trust) civ turns out to be armed
	protected static const float FFRX_FRISK_INNOCENT_TRUST = -8.0;   // frisking an innocent civilian offends them

	protected float m_fNextRevealTime;          // world-time (ms) gate
	protected ref array<IEntity> m_aScan = {};  // query accumulator

	// Concealable weapons a spy pulls out (hidden under civilian clothes): compact AKS-74U + pistols.
	protected ref array<ResourceName> m_aSpyWeapons = {
		"{BFEA719491610A45}Prefabs/Weapons/Rifles/AKS74U/Rifle_AKS74U.et",
		"{C0F7DD85A86B2900}Prefabs/Weapons/Handguns/PM/Handgun_PM.et",
		"{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et"
	};

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;
		s_Instance = new FFRX_DisguisedSpies();
		GetGame().GetCallqueue().CallLater(s_Instance.FFRX_Tick, FFRX_TICK_MS, true);
		Print("[FFRX][Spy] Systeme espions deguises demarre.", LogLevel.NORMAL);
	}

	// #revealspy: reveal (turn hostile) the nearest civilian to a position.
	static bool RevealNearest(vector fromPos, float range)
	{
		if (!s_Instance)
			s_Instance = new FFRX_DisguisedSpies();
		return s_Instance.FFRX_RevealNearestImpl(fromPos, range);
	}

	// Frisk the nearest civilian. Returns 1 = armed spy caught early, 0 = innocent (offended), -1 = none.
	static int FriskNearest(vector fromPos, float range)
	{
		if (!s_Instance)
			s_Instance = new FFRX_DisguisedSpies();
		return s_Instance.FFRX_FriskNearestImpl(fromPos, range);
	}

	// Frisk a SPECIFIC civilian (the one you're talking to, from the dialogue "Fouiller" option).
	// 1 = armed spy caught early, 0 = innocent (offended), -1 = invalid.
	static int FriskCiv(IEntity civEntity)
	{
		if (!s_Instance)
			s_Instance = new FFRX_DisguisedSpies();
		return s_Instance.FFRX_FriskCivImpl(civEntity);
	}

	protected int FFRX_FriskCivImpl(IEntity civEntity)
	{
		if (!civEntity)
			return -1;
		FFRX_CivIdentity id = FFRX_CivIdentityRegistry.Get().IdentityOfOrCreate(civEntity);
		bool couldBeSpy = (!id || id.m_fTrust <= FFRX_SPY_MAX_TRUST);
		if (couldBeSpy && Math.RandomFloat01() < FFRX_FRISK_SPY_CHANCE && FFRX_RevealSpy(civEntity))
		{
			// Le controle a paye : on le trace AVANT de sortir. C'est ce compteur que la page
			// Civils du site affiche -- sans lui, un espion demasque ne laisserait aucune trace
			// dans le fichier de renseignement.
			FFRX_CivIdentityRegistry.Get().NoteFrisk(id, true);
			return 1;
		}
		FFRX_CivIdentityRegistry.Get().AddTrust(id, FFRX_FRISK_INNOCENT_TRUST);
		FFRX_CivIdentityRegistry.Get().NoteFrisk(id, false);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_Tick()
	{
		float now = GetGame().GetWorld().GetWorldTime();
		if (now < m_fNextRevealTime)
			return;

		IEntity civ = FFRX_PickCandidate();
		if (!civ)
			return;
		if (Math.RandomFloat01() >= FFRX_SPY_CHANCE)
			return;

		if (FFRX_RevealSpy(civ))
			m_fNextRevealTime = now + FFRX_REVEAL_COOLDOWN_MS;
	}

	// A civilian within TRIGGER_DIST of a random player who is in enemy/contested territory.
	protected IEntity FFRX_PickCandidate()
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return null;
		array<int> ids = {};
		pm.GetPlayers(ids);
		if (ids.IsEmpty())
			return null;

		IEntity pe = pm.GetPlayerControlledEntity(ids.GetRandomElement());
		if (!pe)
			return null;
		vector ppos = pe.GetOrigin();
		if (!FFRX_InEnemyTerritory(ppos))
			return null;   // spies only surface when players venture into contested/enemy areas

		m_aScan.Clear();
		BaseWorld world = GetGame().GetWorld();
		vector mins = ppos - Vector(FFRX_TRIGGER_DIST, 20, FFRX_TRIGGER_DIST);
		vector maxs = ppos + Vector(FFRX_TRIGGER_DIST, 20, FFRX_TRIGGER_DIST);
		world.QueryEntitiesByAABB(mins, maxs, FFRX_CollectCiv, FFRX_FilterChar, EQueryEntitiesFlags.DYNAMIC);

		if (m_aScan.IsEmpty())
			return null;
		return m_aScan.GetRandomElement();
	}

	protected bool FFRX_FilterChar(IEntity ent)
	{
		return ChimeraCharacter.Cast(ent) != null;
	}

	protected bool FFRX_CollectCiv(IEntity ent)
	{
		if (JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(ent))
			m_aScan.Insert(ent);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Turn an existing civilian into an enemy infiltrator (reuse MCD's proven mechanism).
	protected bool FFRX_RevealSpy(IEntity civEntity)
	{
		JWK_CivilianCharacterComponent civ = JWK_CompTU<JWK_CivilianCharacterComponent>.FindIn(civEntity);
		if (!civ)
			return false;

		// RPG trust tie-in: a civilian whose trust you've earned won't betray you (never a spy).
		FFRX_CivIdentity id = FFRX_CivIdentityRegistry.Get().IdentityOfOrCreate(civEntity);
		if (id && id.m_fTrust > FFRX_SPY_MAX_TRUST)
			return false;

		if (!MCD_CivilianInteractionHelper.TurnMilitarilyHostile(civ))
			return false;

		FFRX_GiveSpyWeapon(civEntity);
		Print("[FFRX][Spy] Un civil se revele espion (bascule hostile + arme cachee).", LogLevel.NORMAL);
		return true;
	}

	// Give the spy a random concealable weapon from our pool (AKS-74U / pistols).
	protected void FFRX_GiveSpyWeapon(IEntity civEntity)
	{
		SCR_InventoryStorageManagerComponent inv = JWK_CompTU<SCR_InventoryStorageManagerComponent>.FindIn(civEntity);
		if (!inv || m_aSpyWeapons.IsEmpty())
			return;
		inv.TrySpawnPrefabToStorage(m_aSpyWeapons.GetRandomElement());
	}

	//------------------------------------------------------------------------------------------------
	protected bool FFRX_RevealNearestImpl(vector fromPos, float range)
	{
		m_aScan.Clear();
		BaseWorld world = GetGame().GetWorld();
		vector mins = fromPos - Vector(range, 20, range);
		vector maxs = fromPos + Vector(range, 20, range);
		world.QueryEntitiesByAABB(mins, maxs, FFRX_CollectCiv, FFRX_FilterChar, EQueryEntitiesFlags.DYNAMIC);

		IEntity best = null;
		float bestSq = range * range;
		foreach (IEntity civ : m_aScan)
		{
			float dSq = vector.DistanceSqXZ(civ.GetOrigin(), fromPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = civ;
			}
		}
		if (!best)
			return false;
		return FFRX_RevealSpy(best);
	}

	// Frisk: search the nearest civ. Armed (low-trust + chance) -> caught early (revealed, but you
	// initiated so you have the drop). Innocent -> offended (-trust). 1 = spy, 0 = innocent, -1 = none.
	protected int FFRX_FriskNearestImpl(vector fromPos, float range)
	{
		m_aScan.Clear();
		BaseWorld world = GetGame().GetWorld();
		vector mins = fromPos - Vector(range, 20, range);
		vector maxs = fromPos + Vector(range, 20, range);
		world.QueryEntitiesByAABB(mins, maxs, FFRX_CollectCiv, FFRX_FilterChar, EQueryEntitiesFlags.DYNAMIC);

		IEntity best = null;
		float bestSq = range * range;
		foreach (IEntity civ : m_aScan)
		{
			float dSq = vector.DistanceSqXZ(civ.GetOrigin(), fromPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = civ;
			}
		}
		if (!best)
			return -1;

		FFRX_CivIdentity id = FFRX_CivIdentityRegistry.Get().IdentityOfOrCreate(best);
		bool couldBeSpy = (!id || id.m_fTrust <= FFRX_SPY_MAX_TRUST);
		if (couldBeSpy && Math.RandomFloat01() < FFRX_FRISK_SPY_CHANCE && FFRX_RevealSpy(best))
			return 1;

		// Innocent -> frisking them for nothing damages your relationship.
		FFRX_CivIdentityRegistry.Get().AddTrust(id, FFRX_FRISK_INNOCENT_TRUST);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected bool FFRX_InEnemyTerritory(vector pos)
	{
		JWK_TerritoryControlSystem tc = JWK.GetTerritoryControl();
		if (!tc)
			return false;
		JWK_TerritoryControlNodeComponent node = tc.GetNodeAt(pos);
		if (!node)
			return false;
		return node.GetFactionRole() == JWK_EFactionRole.ENEMY || node.IsBorder();
	}
}

//----------------------------------------------------------------------------------------------------
// Dev command "#frisk": frisk the nearest civilian (armed spy caught / innocent offended).
[BaseContainerProps()]
class FFRX_FriskCommand : ScrServerCommand
{
	override string GetKeyword() { return "frisk"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		int r = FFRX_DisguisedSpies.FriskNearest(ent.GetOrigin(), 6.0);
		if (r == 1)
			return ScrServerCmdResult("Espion demasque a la fouille ! (il degaine, mais tu es pret)", EServerCmdResultType.OK);
		if (r == 0)
			return ScrServerCmdResult("Civil innocent fouille -> il est offense (-confiance).", EServerCmdResultType.OK);
		return ScrServerCmdResult("Aucun civil a portee de fouille.", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}

// Dev command "#revealspy": turn the nearest civilian into an enemy infiltrator (test the reveal).
// Needs a civilian nearby (go to a town).
[BaseContainerProps()]
class FFRX_RevealSpyCommand : ScrServerCommand
{
	override string GetKeyword() { return "revealspy"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		IEntity ent = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!ent)
			return ScrServerCmdResult("Incarne un personnage.", EServerCmdResultType.ERR);

		if (FFRX_DisguisedSpies.RevealNearest(ent.GetOrigin(), 30.0))
			return ScrServerCmdResult("Civil le plus proche demasque (bascule hostile + arme).", EServerCmdResultType.OK);
		return ScrServerCmdResult("Aucun civil a proximite (va pres d'une ville).", EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv) { return ScrServerCmdResult("En jeu uniquement.", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
