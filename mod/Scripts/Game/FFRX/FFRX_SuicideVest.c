// FF - REMIXED - PVE
// GILETS SUICIDE : quelques combattants ennemis chargent pour exploser.
//
// ------------------------------------------------------------------------------------
// CE QUE CA AJOUTE, ET EN QUOI C'EST DIFFERENT DE L'EXISTANT
//
// FFRX_SuicideBomber (brick B) fait apparaitre un CIVIL kamikaze isole, expres, pres d'un
// joueur : c'est une menace d'ambiance qui oblige a se mefier de la population.
//
// Ici c'est autre chose : un COMBATTANT d'un groupe ennemi normal, deja sur la carte,
// deja en train de se battre, qui a un gilet sous sa veste. Quand le joueur s'approche
// trop, il lache le combat et vient chercher le contact. Le joueur ne peut pas le
// distinguer des autres avant qu'il ne se mette a courir droit sur lui.
//
// Consequence de jeu voulue : on ne peut plus nettoyer une position au corps a corps
// sans reflechir, ni laisser un blesse ennemi derriere soi sans le surveiller.
//
// ------------------------------------------------------------------------------------
// QUI PORTE LE GILET
//
// Un pourcentage des combattants ennemis, tire une fois a leur apparition (meme point
// que les optiques de vision nocturne, cf. FFRX_AIDifficulty.FFRX_ApplyDifficulty).
//
// PAS de multiplicateur pour l'elite, contrairement aux JVN : le gilet suicide est
// l'arme du faible, pas celle d'une unite bien equipee. Un Spetsnaz qui se fait sauter
// n'aurait aucun sens. C'est donc un pourcentage unique, volontairement bas.
//
// NOTE : le porteur n'a pas de gilet VISIBLE. Lui en mettre un demanderait un prefab de
// vetement dedie par faction, et surtout ca le trahirait -- alors que tout l'interet est
// justement qu'il soit indiscernable jusqu'a sa course.
//
// ------------------------------------------------------------------------------------
// LE DECLENCHEMENT
//
// Il faut que ce soit lisible pour le joueur, sinon c'est juste une mort gratuite :
//   - le porteur ne charge que si un joueur est DEJA proche (TRIGGER_DIST) -- pas de
//     course de 300 m, on doit pouvoir le voir venir et le descendre ;
//   - il court en zigzag, donc il est touchable mais pas trivial a arreter ;
//   - il explose au contact, ou s'il meurt EN COURSE (l'abattre a trois metres ne sauve
//     pas -- c'est ce qui rend la menace reelle et pousse a l'engager tot).
//
// Le comportement de course reprend celui, deja eprouve, de FFRX_SuicideBomber et
// FFRX_AIAssault : un SCR_AIMoveAndInvestigateBehavior pousse directement sur l'agent
// avec une priorite choisie -- au-dessus de l'attaque (70/90) pour qu'il avance vraiment,
// en dessous des reflexes de survie (110+).
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict, cf. memoire).

class FFRX_SuicideVest
{
	protected static ref FFRX_SuicideVest s_Instance;

	// Distance a laquelle un porteur se declenche. Volontairement courte : il doit etre
	// visible avant de partir, pas surgir du decor.
	protected static const float TRIGGER_DIST = 70.0;
	// Contact = explosion.
	protected static const float DETONATE_DIST = 3.5;
	// Au-dela, il abandonne (le joueur a decroche).
	protected static const float GIVEUP_DIST = 140.0;

	protected static const int   TICK_MS = 1000;
	protected static const float CHARGE_PRIORITY = 100;  // > attaque (70/90), < survie (110+)
	protected static const float CHARGE_ZIGZAG   = 5.0;

	// Meme charge que le kamikaze civil : eprouvee, et l'echelle est la bonne.
	protected static const ResourceName EXPLOSION = "{564D57EA34A75775}Prefabs/Weapons/Warheads/Explosions/Explosion_Tnt_Medium.et";

	// Porteurs vivants, en course ou non.
	protected ref array<IEntity> m_aCarriers = {};
	protected int m_iChargeTick;
	protected static bool s_bStarted;

	// ---- Observabilite ----
	// Meme raison que pour les optiques NV : a 3 %, l'absence de kamikaze pendant une
	// session ne prouve rien. On compte.
	protected static int s_iRolled;     // ennemis passes par le tirage
	protected static int s_iGranted;    // dont porteurs d'un gilet
	protected static int s_iDetonated;  // gilets ayant reellement explose

	//! Appele par FFRX_AIDifficulty a chaque tirage.
	static void NoteRoll(bool granted)
	{
		s_iRolled++;
		if (granted)
			s_iGranted++;
	}

	//------------------------------------------------------------------------------------------------
	static string Render()
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		float wantPct = 0;
		if (cache)
			wantPct = cache.m_fFFRX_VestPct;

		int gotPct = 0;
		if (s_iRolled > 0)
			gotPct = (s_iGranted * 100) / s_iRolled;

		// Decoupe volontaire, meme raison que dans FFRX_NightVision.Render : une longue
		// chaine de '+' dans une seule expression fait echouer Enforce sur
		// "Formula too complex", avec une cascade d'erreurs trompeuses ailleurs.
		string txt = "[FFRX][Vest] porteurs " + s_iGranted.ToString() + "/" + s_iRolled.ToString();
		txt = txt + " (" + gotPct.ToString() + "%, reglage " + Math.Round(wantPct).ToString() + "%)";
		txt = txt + " | en vie " + Get().m_aCarriers.Count().ToString();
		txt = txt + " | explosions " + s_iDetonated.ToString();
		return txt;
	}

	//------------------------------------------------------------------------------------------------
	static FFRX_SuicideVest Get()
	{
		if (!s_Instance)
			s_Instance = new FFRX_SuicideVest();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_bStarted)
			return;

		s_bStarted = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		Print("[FFRX][Vest] Gilets suicide actifs.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Tirage a l'apparition du soldat. Appele depuis FFRX_ApplyDifficulty, au meme
	//! endroit que celui des optiques NV.
	static bool Roll()
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return false;

		float pct = cache.m_fFFRX_VestPct;
		if (pct <= 0)
			return false;

		return (JWK.Random.RandFloat01() * 100) < pct;
	}

	//------------------------------------------------------------------------------------------------
	//! Un porteur vient d'apparaitre : on le suit.
	static void Register(IEntity carrier)
	{
		if (!carrier)
			return;
		if (!Replication.IsServer())
			return;

		Get().m_aCarriers.Insert(carrier);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		Get().DoTick();
	}

	//------------------------------------------------------------------------------------------------
	protected void DoTick()
	{
		// Parcours a l'envers : on retire des elements en cours de route.
		for (int i = m_aCarriers.Count() - 1; i >= 0; i--)
		{
			IEntity carrier = m_aCarriers[i];
			if (!carrier)
			{
				m_aCarriers.Remove(i);
				continue;
			}

			// Mort : s'il etait en course, il emporte ce qu'il y a autour. Sinon la charge
			// n'a jamais ete amorcee et il meurt normalement.
			if (IsDead(carrier))
			{
				if (WasCharging(carrier))
					Detonate(carrier);

				m_aCarriers.Remove(i);
				continue;
			}

			IEntity target = NearestPlayer(carrier.GetOrigin());
			if (!target)
				continue;

			float dist = vector.Distance(carrier.GetOrigin(), target.GetOrigin());

			if (dist <= DETONATE_DIST)
			{
				Detonate(carrier);
				m_aCarriers.Remove(i);
				continue;
			}

			if (dist <= TRIGGER_DIST)
			{
				MarkCharging(carrier);
				Charge(carrier, target);
			}
			else if (dist > GIVEUP_DIST)
			{
				// Le joueur a decroche : on cesse de le poursuivre, mais le soldat reste
				// porteur -- il se redeclenchera a la prochaine occasion.
				ClearCharging(carrier);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Course vers la cible, en zigzag. Meme forme que FFRX_SuicideBomber.FFRX_Charge :
	//! on annule l'ordre precedent avant d'en poser un neuf, sinon les comportements
	//! s'empilent et le porteur poursuit une position perimee.
	protected void Charge(IEntity carrier, IEntity target)
	{
		AIControlComponent ctrl = AIControlComponent.Cast(carrier.FindComponent(AIControlComponent));
		if (!ctrl)
			return;

		if (!ctrl.GetControlAIAgent())
			return;

		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(carrier.FindComponent(SCR_AIUtilityComponent));
		if (!utility)
			return;

		vector goal = target.GetOrigin();

		vector dir = goal - carrier.GetOrigin();
		dir[1] = 0;
		if (dir.LengthSq() > 1)
		{
			dir.Normalize();
			vector side = Vector(-dir[2], 0, dir[0]);
			// Pas d'operateur '%' en Enforce -> parite a la main.
			int half = m_iChargeTick / 2;
			int parity = m_iChargeTick - (half * 2);
			goal = goal + side * ((parity * 2 - 1) * CHARGE_ZIGZAG);
		}

		utility.SetStateAllActionsOfType(SCR_AIMoveAndInvestigateBehavior, EAIActionState.FAILED);

		SCR_AIMoveAndInvestigateBehavior move = new SCR_AIMoveAndInvestigateBehavior(
			utility,
			null,
			goal,
			CHARGE_PRIORITY,
			SCR_AIActionBase.PRIORITY_LEVEL_NORMAL,
			DETONATE_DIST,
			true,
			EAIUnitType.UnitType_Infantry,
			TICK_MS / 1000.0);

		utility.AddAction(move);
		m_iChargeTick++;
	}

	//------------------------------------------------------------------------------------------------
	protected void Detonate(IEntity carrier)
	{
		if (!carrier)
			return;

		vector pos = carrier.GetOrigin();
		s_iDetonated++;

		Resource res = Resource.Load(EXPLOSION);
		if (res && res.IsValid())
		{
			EntitySpawnParams p = new EntitySpawnParams();
			p.TransformMode = ETransformMode.WORLD;
			p.Transform[3] = pos;
			GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), p);
		}

		// Le corps disparait avec la charge : il ne reste pas un cadavre intact au centre
		// du cratere.
		SCR_EntityHelper.DeleteEntityAndChildren(carrier);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsDead(IEntity e)
	{
		SCR_ChimeraCharacter ch = SCR_ChimeraCharacter.Cast(e);
		if (!ch)
			return true; // plus un personnage : on cesse de le suivre

		CharacterControllerComponent cc = ch.GetCharacterController();
		if (!cc)
			return true;

		return cc.IsDead();
	}

	//------------------------------------------------------------------------------------------------
	// Etat "en course" : garde en memoire ceux qui ont deja ete declenches, pour savoir si
	// une mort doit faire sauter la charge. Une simple liste suffit, ils sont peu nombreux.
	protected ref array<IEntity> m_aCharging = {};

	protected void MarkCharging(IEntity e)
	{
		if (!m_aCharging.Contains(e))
			m_aCharging.Insert(e);
	}

	protected void ClearCharging(IEntity e)
	{
		m_aCharging.RemoveItem(e);
	}

	protected bool WasCharging(IEntity e)
	{
		return m_aCharging.Contains(e);
	}

	//------------------------------------------------------------------------------------------------
	//! Joueur vivant le plus proche. On ne vise que des joueurs : un kamikaze qui court
	//! sur une IA alliee ne raconterait rien.
	protected IEntity NearestPlayer(vector from)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return null;

		array<int> ids = {};
		pm.GetPlayers(ids);

		IEntity best = null;
		float bestSq = -1;

		foreach (int pid : ids)
		{
			IEntity ent = pm.GetPlayerControlledEntity(pid);
			if (!ent)
				continue;
			if (IsDead(ent))
				continue;

			float d = vector.DistanceSq(from, ent.GetOrigin());
			if (bestSq < 0 || d < bestSq)
			{
				bestSq = d;
				best = ent;
			}
		}

		return best;
	}
}
