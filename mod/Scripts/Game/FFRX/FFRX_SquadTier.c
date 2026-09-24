// FF - REMIXED - PVE
// NIVEAU D'ESCOUADE : toutes les patrouilles ne se valent pas.
//
// ------------------------------------------------------------------------------------
// LE PROBLEME
//
// Aujourd'hui, tout l'ennemi d'un theatre a exactement le meme niveau. Sur Anizay c'est
// flagrant : FFRX_ClassifyPrefab range TOUS les MEI dans "Reguliers" (le mod MEI n'a pas
// de sous-forces), donc chaque patrouille tire aussi bien que la precedente, et le
// multiplicateur "elite" des optiques de vision nocturne ne s'applique jamais -- constate
// le 2026-09-17 : "porteurs 7/123, dont 0 par dotation", tous au taux de base.
//
// Consequence de jeu : aucune montee en tension. On ne craint pas de tomber sur "la
// mauvaise patrouille", puisqu'elles sont toutes identiques.
//
// ------------------------------------------------------------------------------------
// CE QU'ON FAIT
//
// Chaque ESCOUADE recoit un niveau a sa creation, tire au sort. Le niveau s'applique a
// tous ses membres : il decale leur competence de tir et leur chance d'avoir une optique
// de vision nocturne.
//
// Le niveau est une propriete du GROUPE, pas du soldat -- c'est ce qui rend la chose
// lisible en jeu : on affronte une escouade de bleus OU une escouade aguerrie, pas un
// melange indistinct. Un joueur peut sentir la difference et adapter son approche.
//
// Repartition par defaut (reglable) : la plupart des patrouilles sont ordinaires, les
// extremes sont rares. C'est ce qui rend la rencontre d'une escouade d'elite marquante.
//
// ------------------------------------------------------------------------------------
// OU ON S'ACCROCHE
//
// Le tirage se fait dans le `modded class SCR_AIGroup.EOnInit` de FFRX_AIAssault.c (le
// seul autorise pour cet addon), et la lecture se fait a l'init de chaque soldat, dans
// FFRX_ApplyDifficulty.
//
// ORDRE D'INITIALISATION -- le point delicat. Un soldat peut s'initialiser AVANT d'etre
// rattache a son groupe : GetAIGroup() renvoie alors null et on retombe sur le niveau
// ordinaire. Ce n'est pas grave, et c'est meme rattrape tout seul : FFRX_ReapplyAll
// repasse sur tous les ennemis vivants a chaque bascule jour/nuit et a chaque changement
// de reglage, et le niveau est alors correctement lu.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict, cf. memoire).

enum FFRX_ESquadTier
{
	ORDINARY,   // le gros de la troupe
	GREEN,      // bleus : tirent mal, voient mal
	VETERAN,    // aguerris
	ELITE       // rares, dangereux, bien equipes
}

class FFRX_SquadTier
{
	// Repartition, en pourcents cumules. Le reste est ORDINARY.
	// 15 bleus / 20 veterans / 5 elite / 60 ordinaires.
	protected static const float PCT_GREEN   = 15;
	protected static const float PCT_VETERAN = 20;
	protected static const float PCT_ELITE   = 5;

	// Decalage de competence applique au skill de la force, en points (echelle 0-100).
	protected static const float SKILL_GREEN   = -25;
	protected static const float SKILL_VETERAN = 12;   // pas de '+' unaire en Enforce
	protected static const float SKILL_ELITE   = 25;

	// Cibles de repartition aux deux extremes de l'anciennete, quand l'adaptation est a
	// fond. Entre les deux on interpole ; le curseur dose ensuite l'ecart a la base.
	//
	// Le reste est ORDINARY : face a des bleus il y a donc 61 % de patrouilles ordinaires,
	// face a des anciens 53 %. Le gros de la troupe ne bouge pas -- ce sont les EXTREMES
	// qui se deplacent. Une escouade d'elite reste rare meme face a des anciens (13 %),
	// sinon elle cesse d'etre un evenement.
	protected static const float ADAPT_GREEN_ROOKIE    = 30;   // beaucoup de bleus en face des nouveaux
	protected static const float ADAPT_VETERAN_ROOKIE  = 8;
	protected static const float ADAPT_ELITE_ROOKIE    = 1;

	protected static const float ADAPT_GREEN_VETERAN   = 4;    // presque plus de bleus en face des anciens
	protected static const float ADAPT_VETERAN_VETERAN = 34;
	protected static const float ADAPT_ELITE_VETERAN   = 13;

	// Multiplicateur applique a la chance d'avoir une optique NV.
	// Un groupe de bleus n'a quasiment rien ; une escouade d'elite est bien dotee.
	protected static const float NVG_GREEN   = 0.25;
	protected static const float NVG_VETERAN = 2.0;
	protected static const float NVG_ELITE   = 4.0;

	// Niveau par groupe. Cle = EntityID du groupe en texte (stable pour sa duree de vie).
	protected static ref map<string, int> s_mTiers;

	// Statistiques, pour #nvg.
	protected static int s_iGreen, s_iOrdinary, s_iVeteran, s_iElite;

	// Derniere adaptation reellement appliquee, pour le diagnostic : sans ca le systeme
	// est invisible en jeu (on ne voit que le resultat du tirage, pas ce qui l'a biaise).
	protected static int s_iAdapted;        // groupes nes avec une repartition adaptee
	protected static float s_fLastRatio = -1;
	protected static float s_fLastGreen, s_fLastVeteran, s_fLastElite;

	//------------------------------------------------------------------------------------------------
	//! Tire le niveau d'une escouade a sa creation.
	static void AssignTier(SCR_AIGroup group)
	{
		if (!group)
			return;
		if (!Replication.IsServer())
			return;

		if (!s_mTiers)
			s_mTiers = new map<string, int>();

		// SCR_AIGroup est une ENTITE (cf. son EOnInit(IEntity owner) dans
		// FFRX_AIAssault) : pas de GetOwner() dessus, l'identifiant se lit
		// directement sur le groupe.
		string key = group.GetID().ToString();
		if (s_mTiers.Contains(key))
			return;

		// Repartition de base, puis biaisee par l'anciennete des joueurs a proximite.
		float pctGreen = PCT_GREEN;
		float pctVeteran = PCT_VETERAN;
		float pctElite = PCT_ELITE;
		AdaptToNearbyPlayers(group.GetOrigin(), pctGreen, pctVeteran, pctElite);

		float roll = JWK.Random.RandFloat01() * 100;

		int tier = FFRX_ESquadTier.ORDINARY;
		if (roll < pctGreen)
		{
			tier = FFRX_ESquadTier.GREEN;
			s_iGreen++;
		}
		else if (roll < pctGreen + pctVeteran)
		{
			tier = FFRX_ESquadTier.VETERAN;
			s_iVeteran++;
		}
		else if (roll < pctGreen + pctVeteran + pctElite)
		{
			tier = FFRX_ESquadTier.ELITE;
			s_iElite++;
		}
		else
		{
			s_iOrdinary++;
		}

		s_mTiers.Set(key, tier);
	}

	//------------------------------------------------------------------------------------------------
	//! ADAPTATION AU NIVEAU DES JOUEURS EN FACE.
	//!
	//! Une escouade qui nait pres de joueurs aguerris a plus de chances d'etre aguerrie
	//! elle-meme ; pres de nouveaux venus, plus de chances d'etre composee de bleus.
	//!
	//! POURQUOI L'XP ET PAS LE NOMBRE DE JOUEURS : on veut repondre a la COMPETENCE, pas a
	//! la masse. Trois anciens doivent trouver a qui parler ; huit nouveaux ne doivent pas
	//! se faire accueillir par de l'elite. L'XP de Fleet monte au temps de jeu (10/min), ce
	//! qui en fait une mesure d'anciennete honnete, difficile a farmer.
	//!
	//! CE QUI N'EST PAS ADAPTE : la difficulte reste une propriete du GROUPE tiree a sa
	//! naissance, jamais recalculee. Un joueur ancien qui arrive sur une zone deja peuplee
	//! n'en durcit pas les patrouilles retroactivement -- sinon le monde changerait sous
	//! les pieds des joueurs, ce qui se remarque et casse la lisibilite.
	//!
	//! HORS DE PORTEE DES JOUEURS : aucun joueur dans le rayon -> on ne touche a rien. Il
	//! n'y a personne pour qui adapter, et ca evite de biaiser tout le spawn de la carte
	//! sur la seule base de qui est connecte.
	protected static void AdaptToNearbyPlayers(vector groupPos, out float pctGreen, out float pctVeteran, out float pctElite)
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return;

		float strength = cache.m_fFFRX_TierAdapt;
		if (strength <= 0)
			return;                 // 0 = tirage d'origine, systeme desactive

		float ratio;
		if (!NearbyVeterancy(groupPos, cache.m_fFFRX_TierRadius, cache.m_fFFRX_TierXpRef, ratio))
			return;                 // personne a proximite

		// Cibles aux deux extremes, interpolees par l'anciennete mesuree...
		float tGreen   = Math.Lerp(ADAPT_GREEN_ROOKIE,   ADAPT_GREEN_VETERAN,   ratio);
		float tVeteran = Math.Lerp(ADAPT_VETERAN_ROOKIE, ADAPT_VETERAN_VETERAN, ratio);
		float tElite   = Math.Lerp(ADAPT_ELITE_ROOKIE,   ADAPT_ELITE_VETERAN,   ratio);

		// ...puis dosees par le curseur : a 100 % on atteint la cible, a 50 % on fait
		// la moitie du chemin depuis la repartition de base.
		float k = strength / 100;
		pctGreen   = PCT_GREEN   + (tGreen   - PCT_GREEN)   * k;
		pctVeteran = PCT_VETERAN + (tVeteran - PCT_VETERAN) * k;
		pctElite   = PCT_ELITE   + (tElite   - PCT_ELITE)   * k;

		s_iAdapted++;
		s_fLastRatio = ratio;
		s_fLastGreen = pctGreen;
		s_fLastVeteran = pctVeteran;
		s_fLastElite = pctElite;
	}

	//------------------------------------------------------------------------------------------------
	//! Anciennete moyenne des joueurs dans le rayon, ramenee entre 0 et 1.
	//! Renvoie false si personne n'est a portee (l'appelant ne touche alors a rien).
	protected static bool NearbyVeterancy(vector groupPos, float radius, float xpRef, out float ratio)
	{
		ratio = 0;
		if (xpRef <= 0)
			return false;

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;

		array<int> ids = {};
		pm.GetPlayers(ids);

		float radiusSq = radius * radius;
		int counted = 0;
		float total = 0;

		foreach (int pid : ids)
		{
			IEntity player = pm.GetPlayerControlledEntity(pid);
			if (!player)
				continue;

			if (vector.DistanceSq(player.GetOrigin(), groupPos) > radiusSq)
				continue;

			string uid = FFRX_LoadoutSystem.UidOfPlayer(pid);
			if (uid == "")
				continue;

			total = total + Flt_XpProgression.GetXp(uid);
			counted++;
		}

		if (counted == 0)
			return false;

		ratio = Math.Clamp((total / counted) / xpRef, 0, 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Niveau de l'escouade a laquelle appartient ce soldat. ORDINARY s'il n'est pas
	//! (encore) rattache a un groupe -- cf. la note sur l'ordre d'initialisation en tete.
	static int TierOf(IEntity character)
	{
		if (!character || !s_mTiers)
			return FFRX_ESquadTier.ORDINARY;

		AIControlComponent ctrl = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (!ctrl)
			return FFRX_ESquadTier.ORDINARY;

		AIAgent agent = ctrl.GetControlAIAgent();
		if (!agent)
			return FFRX_ESquadTier.ORDINARY;

		SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!group)
			return FFRX_ESquadTier.ORDINARY;

		int tier;
		if (s_mTiers.Find(group.GetID().ToString(), tier))
			return tier;

		return FFRX_ESquadTier.ORDINARY;
	}

	//------------------------------------------------------------------------------------------------
	//! Decalage de competence du niveau, en points de skill.
	static float SkillOffset(int tier)
	{
		if (tier == FFRX_ESquadTier.GREEN)   return SKILL_GREEN;
		if (tier == FFRX_ESquadTier.VETERAN) return SKILL_VETERAN;
		if (tier == FFRX_ESquadTier.ELITE)   return SKILL_ELITE;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Multiplicateur du niveau sur la chance d'optique NV.
	static float NvgMultiplier(int tier)
	{
		if (tier == FFRX_ESquadTier.GREEN)   return NVG_GREEN;
		if (tier == FFRX_ESquadTier.VETERAN) return NVG_VETERAN;
		if (tier == FFRX_ESquadTier.ELITE)   return NVG_ELITE;
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	static string Render()
	{
		string s = string.Format("[FFRX][Tier] escouades : %1 bleues / %2 ordinaires / %3 aguerries / %4 elite",
			s_iGreen, s_iOrdinary, s_iVeteran, s_iElite);

		// Deux Print separes seraient plus surs, mais on reste sous 6 arguments ici.
		if (s_fLastRatio < 0)
		{
			s = s + "\n[FFRX][Tier] adaptation : jamais declenchee (aucun groupe n'est ne pres d'un joueur, ou curseur a 0)";
		}
		else
		{
			s = s + string.Format("\n[FFRX][Tier] adaptation : %1 groupe(s) adapte(s), derniere anciennete mesuree %2 (0=bleu, 1=ancien)",
				s_iAdapted, s_fLastRatio);
			s = s + string.Format("\n[FFRX][Tier] derniere repartition : %1%% bleus / %2%% aguerris / %3%% elite",
				s_fLastGreen, s_fLastVeteran, s_fLastElite);
		}

		return s;
	}
}
