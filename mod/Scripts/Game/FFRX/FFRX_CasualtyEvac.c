// FF - REMIXED - PVE
// L'IA tire ses blesses a couvert AVANT de les soigner.
//
// COMPLEMENT DE FFRX_HealDiscipline. Celui-la EMPECHE de soigner a decouvert (en
// abaissant la priorite du soin sous menace). Mais empecher ne suffit pas : sans ce
// fichier, le blesse reste ou il est tombe et personne ne s'en occupe. Ici on donne
// la suite logique -- on le SORT de la zone, puis le soin reprend tout seul parce que
// la menace est retombee.
//
// POURQUOI UN DIRECTEUR PERIODIQUE ET PAS UN COMPORTEMENT. On aurait pu greffer ca sur
// SCR_AIMedicHealBehavior, mais son point d'entree utilisable (CustomEvaluate) est
// reevalue en continu : y declencher un effet de bord ferait partir des dizaines
// d'evacuations par seconde. Et comme FFRX_HealDiscipline abaisse justement la priorite
// du soin sous menace, ce comportement n'est meme plus selectionne au moment ou on en
// aurait besoin. Un directeur qui balaie periodiquement est plus simple a raisonner et
// suit l'architecture deja en place (cf. FFRX_AssaultDirector).
//
// CE QU'ON UTILISE, ET C'EST DU JEU DE BASE / ACE, PAS DU BRICOLAGE :
//   - ACE : SCR_CharacterControllerComponent.ACE_Carrying_DragCasualty(casualty) pour
//     saisir, et ACE_CarriableEntityComponent.Release() pour reposer. Ce sont de simples
//     methodes publiques, donc appelables sur une IA -- c'est ce qui rend l'idee jouable.
//   - Moteur : SCR_AICombatMoveRequest_Move avec m_bTryFindCover = true. Le jeu SAIT
//     chercher un couvert (c'est ce que fait SCR_AIAvoidCharacterBehavior) ; on ne
//     recalcule donc pas de "spot safe" a la main, on demande au moteur.
//
// Serveur uniquement (l'IA n'existe que sur l'autorite). Chaines ASCII.

class FFRX_EvacTuning
{
	static const int   TICK_MS        = 4000;   // cadence de balayage
	static const float SCAN_RADIUS    = 150.0;  // autour de chaque joueur
	static const float HELPER_RADIUS  = 30.0;   // distance max entre le blesse et son porteur
	static const float COVER_MIN      = 12.0;   // distance de recherche de couvert
	static const float COVER_MAX      = 45.0;
	static const int   RELEASE_MS     = 12000;  // duree max d'une evacuation avant de reposer
	static const int   MAX_ACTIVE     = 4;      // evacuations simultanees
}

// ---------------------------------------------------------------------------
class FFRX_CasualtyEvac
{
	protected static ref FFRX_CasualtyEvac s_Instance;

	//! Blesses deja pris en charge : evite qu'un meme homme soit saisi par deux porteurs
	//! au tick suivant.
	protected ref map<EntityID, bool> m_mInProgress = new map<EntityID, bool>();
	protected ref array<IEntity> m_aScan = {};

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;

		s_Instance = new FFRX_CasualtyEvac();
		GetGame().GetCallqueue().CallLater(s_Instance.Tick, FFRX_EvacTuning.TICK_MS, true);
		Print("[FFRX][Evac] Evacuation des blesses a couvert : active.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Reutilise le reglage de la discipline de soin : les deux vont ensemble, ca n'aurait
	//! pas de sens d'evacuer sans interdire le soin a decouvert, ni l'inverse.
	protected static bool Enabled()
	{
		return FFRX_HealDiscipline.Enabled();
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (!Enabled())
			return;
		if (m_mInProgress.Count() >= FFRX_EvacTuning.MAX_ACTIVE)
			return;

		// On balaie autour des JOUEURS : c'est la seule zone ou la scene est vue, donc la
		// seule ou le comportement a un interet. Balayer toute la carte couterait cher pour
		// des combats que personne ne regarde.
		PlayerManager pm = GetGame().GetPlayerManager();
		BaseWorld world = GetGame().GetWorld();
		if (!pm || !world)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);

		foreach (int pid : ids)
		{
			IEntity p = pm.GetPlayerControlledEntity(pid);
			if (!p)
				continue;

			m_aScan.Clear();
			world.QueryEntitiesBySphere(p.GetOrigin(), FFRX_EvacTuning.SCAN_RADIUS, CollectCharacter, null, EQueryEntitiesFlags.DYNAMIC);
			TryEvacuateFrom(m_aScan, p.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool CollectCharacter(IEntity ent)
	{
		if (SCR_ChimeraCharacter.Cast(ent))
			m_aScan.Insert(ent);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Cherche un couple (blesse, porteur) dans le lot balaye et lance l'evacuation.
	protected void TryEvacuateFrom(array<IEntity> chars, vector threatPos)
	{
		foreach (IEntity c : chars)
		{
			SCR_ChimeraCharacter casualty = SCR_ChimeraCharacter.Cast(c);
			if (!casualty || !IsEvacuable(casualty))
				continue;

			SCR_ChimeraCharacter carrier = FindCarrier(chars, casualty);
			if (!carrier)
				continue;

			if (Start(carrier, casualty, threatPos))
				return;   // une evacuation par tick : on ne veut pas vider un groupe d'un coup
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Blesse eligible : ENNEMI, inconscient, pas deja porte, pas deja pris en charge.
	protected bool IsEvacuable(SCR_ChimeraCharacter ch)
	{
		if (m_mInProgress.Contains(ch.GetID()))
			return false;

		if (!JWK.GetFactions() || JWK.GetFactions().GetEntityRole(ch) != JWK_EFactionRole.ENEMY)
			return false;

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ch.GetCharacterController());
		if (!cc || !cc.IsUnconscious())
			return false;

		ACE_CarriableEntityComponent carriable = ACE_CarriableEntityComponent.GetCarriableEntity(ch);
		if (!carriable || carriable.IsCarried())
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Porteur : un camarade ENNEMI valide, debout, le plus proche du blesse.
	//! Debout est une contrainte d'ACE : CanCarryCasualty refuse a genoux ou couche.
	protected SCR_ChimeraCharacter FindCarrier(array<IEntity> chars, SCR_ChimeraCharacter casualty)
	{
		SCR_ChimeraCharacter best = null;
		float bestSq = FFRX_EvacTuning.HELPER_RADIUS * FFRX_EvacTuning.HELPER_RADIUS;

		foreach (IEntity c : chars)
		{
			SCR_ChimeraCharacter cand = SCR_ChimeraCharacter.Cast(c);
			if (!cand || cand == casualty)
				continue;

			if (!JWK.GetFactions() || JWK.GetFactions().GetEntityRole(cand) != JWK_EFactionRole.ENEMY)
				continue;

			// Pas de joueur : on ne pilote jamais le perso de quelqu'un.
			if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(cand) > 0)
				continue;

			SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(cand.GetCharacterController());
			if (!cc || cc.IsUnconscious())
				continue;

			if (cc.GetStance() != ECharacterStance.STAND)
				continue;

			float dSq = vector.DistanceSq(cand.GetOrigin(), casualty.GetOrigin());
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = cand;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Start(SCR_ChimeraCharacter carrier, SCR_ChimeraCharacter casualty, vector threatPos)
	{
		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(carrier.GetCharacterController());
		if (!cc)
			return false;

		// TRAINER, pas porter : le traine reste bas et le porteur garde son arme. Porter
		// expose les deux silhouettes debout -- l'inverse de ce qu'on cherche.
		cc.ACE_Carrying_DragCasualty(casualty);

		ACE_CarriableEntityComponent carriable = ACE_CarriableEntityComponent.GetCarriableEntity(casualty);
		if (!carriable || !carriable.IsCarried())
			return false;   // ACE a refuse (posture, deja occupe...) : on n'insiste pas

		m_mInProgress.Set(casualty.GetID(), true);
		MoveToCover(carrier, threatPos);

		// Filet de securite : on repose au bout d'un delai, meme si le deplacement se
		// bloque. Un blesse traine indefiniment serait pire que pas d'evacuation du tout.
		GetGame().GetCallqueue().CallLater(Release, FFRX_EvacTuning.RELEASE_MS, false, casualty);

		Print(string.Format("[FFRX][Evac] Blesse traine a couvert (menace a %1 m).",
			(int)vector.Distance(carrier.GetOrigin(), threatPos)), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Deplacement AVEC recherche de couvert, dos a la menace. On ne calcule pas le point
	//! nous-memes : m_bTryFindCover laisse le moteur choisir un vrai couvert, et
	//! m_vTargetPos (la menace) sert a le noter -- un couvert qui protege DE l'ennemi.
	protected void MoveToCover(SCR_ChimeraCharacter carrier, vector threatPos)
	{
		SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(carrier.FindComponent(SCR_AIUtilityComponent));
		if (!utility || !utility.m_CombatMoveState)
			return;

		SCR_AICombatMoveRequest_Move rq = new SCR_AICombatMoveRequest_Move();
		rq.m_eReason        = SCR_EAICombatMoveReason.CHARACTER_AVOIDANCE;
		rq.m_vTargetPos     = threatPos;         // sert au SCORING du couvert
		rq.m_vMovePos       = threatPos;         // reference de direction
		rq.m_eDirection     = SCR_EAICombatMoveDirection.BACKWARD;   // s'eloigner de la menace
		rq.m_bTryFindCover  = true;
		rq.m_bFailIfNoCover = false;             // pas de couvert -> on s'eloigne quand meme
		rq.m_fCoverSearchDistMin = FFRX_EvacTuning.COVER_MIN;
		rq.m_fCoverSearchDistMax = FFRX_EvacTuning.COVER_MAX;
		rq.m_eMovementType  = EMovementType.RUN;
		rq.m_eStanceMoving  = ECharacterStance.STAND;
		rq.m_eStanceEnd     = ECharacterStance.STAND;
		rq.m_fMoveDuration_s = FFRX_EvacTuning.RELEASE_MS / 1000.0;

		utility.m_CombatMoveState.ApplyNewRequest(rq);
	}

	//------------------------------------------------------------------------------------------------
	//! Repose le blesse. Le soin reprend ensuite tout seul : FFRX_HealDiscipline rend la
	//! priorite normale des que la menace retombe, donc rien a relancer ici.
	protected void Release(SCR_ChimeraCharacter casualty)
	{
		if (!casualty)
			return;

		ACE_CarriableEntityComponent carriable = ACE_CarriableEntityComponent.GetCarriableEntity(casualty);
		if (carriable && carriable.IsCarried())
			carriable.Release();

		m_mInProgress.Remove(casualty.GetID());
		Print("[FFRX][Evac] Blesse repose, soin possible.", LogLevel.NORMAL);
	}
}
