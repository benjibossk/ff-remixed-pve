// FF - REMIXED - PVE
// Discipline de soin : l'IA ne panse plus son voisin en plein champ de tir.
//
// LE PROBLEME. Un medecin se leve au milieu de l'echange pour soigner un blesse a
// decouvert. Deux cibles immobiles, et une scene qui casse la credibilite du combat.
//
// LA CAUSE, ET ELLE EST INSTRUCTIVE. Ce n'est pas une betise de l'IA, c'est une
// PRIORITE. Dans SCR_AIAction.c :
//     PRIORITY_BEHAVIOR_MEDIC_HEAL = 111   (soigner un camarade)
//     PRIORITY_BEHAVIOR_HEAL       = 65    (se soigner soi-meme)
// A 111, soigner un camarade passe AU-DESSUS des reflexes de survie du jeu de base
// (mise a couvert, repli sous le feu, ~110-125). Le medecin choisit donc litteralement
// de soigner plutot que de se proteger.
//
// ET LE JEU DE BASE AVAIT PREVU LA GARDE -- SANS JAMAIS LA BRANCHER.
// SCR_AIMedicHealBehavior.c:12 declare :
//     protected const float MAX_THREAT_THRESHOLD = 0.002;  // "Max threat value under
//                                                          //  which we will consider healing"
// Cette constante n'est utilisee NULLE PART dans tout le code du jeu (verifie). C'est
// exactement le meme motif que la fumee de combat : la fonctionnalite est ecrite, le
// cablage manque. On ne contourne donc pas le moteur, on finit son travail.
//
// COMMENT. `CustomEvaluate()` est le point d'extension prevu par le moteur
// (AIActionBase.c : `event float CustomEvaluate() { return GetPriority(); }`) : il est
// reevalue en continu. On y rend une priorite ABAISSEE quand l'unite est sous menace,
// de sorte que les reflexes de survie repassent devant. Des que la menace retombe, la
// priorite normale revient d'elle-meme et le soin reprend -- rien a reinitialiser.
//
// On ne met PAS la priorite a zero : le soin doit rester possible, juste plus tard et
// a couvert. Un medecin qui n'agit jamais serait pire que le probleme d'origine.
//
// Serveur : l'IA n'existe que sur l'autorite. Chaines ASCII.

class FFRX_HealDiscipline
{
	//! Priorite rendue quand l'unite est sous menace. 95 est choisi pour passer SOUS la
	//! plage 110-125 des reflexes de survie (couvert, repli, soin critique de soi-meme),
	//! tout en restant au-dessus du comportement d'attente : le medecin se met a l'abri,
	//! puis soigne des que ca se calme.
	static const float UNDER_THREAT_PRIORITY = 95;

	//------------------------------------------------------------------------------------------------
	//! true si la mecanique est active (reglage FF ; 0 = comportement vanilla).
	static bool Enabled()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return true;   // pas encore de reglages : on applique la discipline par defaut
  		return c.m_fFFRX_HealDiscipline > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! true si l'unite est trop exposee pour soigner quelqu'un.
	//!
	//! On bloque des ALERTED, pas seulement THREATENED : ALERTED = l'ennemi est connu et
	//! proche. C'est precisement la situation "au milieu de la fusillade" qu'on veut
	//! interdire. THREATENED seul laisserait passer trop de cas.
	static bool TooExposed(SCR_AIUtilityComponent utility)
	{
		if (!utility || !utility.m_AIInfo)
			return false;

		EAIThreatState threat = utility.m_AIInfo.GetThreatState();
		return threat == EAIThreatState.ALERTED || threat == EAIThreatState.THREATENED;
	}
}

// ---------------------------------------------------------------------------
modded class SCR_AIMedicHealBehavior
{
	//! Reevalue en continu par le composant d'utilite : la valeur rendue ici decide si
	//! le soin gagne ou perd face aux autres comportements.
	override float CustomEvaluate()
	{
		float base = super.CustomEvaluate();

		if (!FFRX_HealDiscipline.Enabled())
			return base;

		if (!FFRX_HealDiscipline.TooExposed(m_Utility))
			return base;

		// Sous menace : on rend la main aux reflexes de survie. On ne renvoie la valeur
		// abaissee que si elle est REELLEMENT plus basse -- si un jour le jeu de base
		// descend cette priorite, on ne veut pas la remonter par accident.
		if (FFRX_HealDiscipline.UNDER_THREAT_PRIORITY < base)
			return FFRX_HealDiscipline.UNDER_THREAT_PRIORITY;

		return base;
	}
}
