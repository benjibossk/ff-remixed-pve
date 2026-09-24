// FF - REMIXED - PVE
// Correctif du mod AIUsingStingers : ne pas tirer le missile dans le sol.
//
// ======================================================================================
//  LE DEFAUT, TOUJOURS PRESENT DANS LA VERSION DU 2026-09-18
// ======================================================================================
//
// Deko_AIAntiAirFireController.ForceAimAtTarget() calcule la direction de visee... et ne
// s'en sert jamais :
//
//     vector aimDir = m_vTargetPos - owner.GetOrigin();
//     aimDir.Normalize();
//     if (aimDir == vector.Zero) return;
//     m_CharController.SetWeaponRaised(true);          // <- c'est tout
//     m_CharController.SetStanceChange(ECharacterStance.STAND);
//
// `aimDir` est une variable morte. Le servant leve son lance-missiles et se met debout,
// mais ne pointe jamais vers la cible en hauteur. Et TriggerFire() n'autorise le tir que
// sur un temps de visee ecoule, sans le moindre controle d'elevation.
//
// Resultat en jeu : sur un helicoptere bas ou proche de l'horizontale, le missile rase le
// terrain et l'equipe AA se vide pour rien.
//
// CE QU'ON FAIT : on retient le tir tant que la cible n'est pas nettement au-dessus du
// tireur. Le servant continue de viser en attendant. Voler bas reste donc une esquive
// valable pour le joueur -- c'est le comportement souhaitable, pas un contournement.
//
// ======================================================================================
//  HISTORIQUE -- POURQUOI CE FICHIER A ETE REECRIT LE 2026-09-18
// ======================================================================================
//
// Le mod a ete mis a jour ce jour-la (pak passe de 95 761 a 83 501 octets). L'auteur a
// refondu la temporisation de visee et SUPPRIME le champ `m_fAimTimer`, que la version
// precedente de ce fichier lisait. Consequence : `Can't find variable 'm_fAimTimer'`, puis
// `Can't compile "Game" script module!`, et le DEDIE NE DEMARRAIT PLUS.
//
// Deux pieges de diagnostic, notes pour la prochaine fois :
//
//  1. L'echec du module produit des erreurs en cascade sur des fichiers du JEU DE BASE
//     (`Can't find class SCR_ScenarioFrameworkParam`, `SCR_SpinningWidgetAnimation`...).
//     Elles ressemblent a s'y meprendre au plafond de compilation. Ce sont des degats
//     collateraux : la seule erreur reelle est la premiere.
//  2. Le Workbench compilait TRES BIEN. Le PC de dev et le dedie telechargent chacun leur
//     copie du mod, et celle du PC etait restee en version du 08/09. Comparer les deux
//     paks est le premier reflexe a avoir quand "ca marche chez moi".
//
// CE QU'ON A RETIRE A CETTE OCCASION, ET POURQUOI :
//
//  - Les trois surcharges qui faisaient taire le debug du mod (DebugLog,
//    DebugBlockReason, DebugCurrentWeapon). L'auteur les gate desormais toutes sur
//    `DEKO_FIRE_DEBUG = false` : le probleme est corrige en amont, nos surcharges etaient
//    devenues du poids mort.
//  - La surcharge de ForceAimAtTarget et la mesure de distance de detection, qui
//    n'existaient que pour les diagnostics et reposaient sur `m_fAimTimer`.
//
// Il ne reste donc qu'UNE surcharge, qui ne lit qu'un seul membre du mod (`m_vTargetPos`,
// present dans les deux versions). Moins on s'accroche a l'interieur d'un mod tiers, moins
// sa prochaine mise a jour nous casse -- cette regle vient de couter une soiree.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict).

modded class Deko_AIAntiAirFireController
{
	//! La cible doit etre au moins a tant de degres au-dessus de l'horizon du tireur.
	//! 6 deg : assez pour degager le relief proche, assez bas pour ne pas rendre l'AA
	//! inoffensive contre un helicoptere en approche.
	protected static const float FFRX_MIN_ELEV_DEG = 6.0;

	//! Anti-spam du journal : TriggerFire est rappele tant que le tir est retenu.
	protected float m_fFFRX_NextHoldLog_ms;

	//------------------------------------------------------------------------------------------------
	override protected void TriggerFire(IEntity owner)
	{
		if (!owner)
		{
			super.TriggerFire(owner);
			return;
		}

		vector to = m_vTargetPos - owner.GetOrigin();

		// Distance horizontale : c'est elle qui sert de reference a l'angle. Sous 1 m on
		// ne peut plus parler d'elevation, on laisse passer plutot que de diviser par
		// presque zero.
		float horiz = Math.Sqrt(to[0] * to[0] + to[2] * to[2]);
		if (horiz <= 1.0)
		{
			super.TriggerFire(owner);
			return;
		}

		float elevDeg = Math.Atan2(to[1], horiz) * Math.RAD2DEG;

		if (elevDeg < FFRX_MIN_ELEV_DEG)
		{
			float now = GetGame().GetWorld().GetWorldTime();
			if (now >= m_fFFRX_NextHoldLog_ms)
			{
				m_fFFRX_NextHoldLog_ms = now + 5000;
				Print(string.Format("[FFRX][AA] tir RETENU : cible trop basse (elev %1 deg < %2), dist %3 m -- continue de viser.",
					(int)elevDeg, (int)FFRX_MIN_ELEV_DEG, (int)to.Length()), LogLevel.NORMAL);
			}

			// On ne tire pas, mais on ne casse rien : le mod rappellera TriggerFire au
			// prochain cycle, et le servant garde sa cible.
			return;
		}

		Print(string.Format("[FFRX][AA] TIR autorise : elev %1 deg, dist %2 m.",
			(int)elevDeg, (int)to.Length()), LogLevel.NORMAL);

		super.TriggerFire(owner);
	}
}
