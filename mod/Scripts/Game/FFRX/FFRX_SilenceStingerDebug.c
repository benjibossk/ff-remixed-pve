// FF - REMIXED - PVE
// Coupe le spam console de AIUsingStingers (WCS).
//
// ======================================================================================
//  LE SYMPTOME
// ======================================================================================
// 1620 lignes en 8 minutes de jeu (mesure Workbench du 2026-09-19) :
//   [WCS_AA_Engagement] Ignoring non-enemy aircraft: {...}UH1H_civ_livery_v1_Patrol.et
//   [WCS_AA_Engagement] Ignoring non-enemy aircraft: {...}Mi8MT_unarmed_transport_Patrol.et
//   [WCS_AA_Engagement] No AA weapon equipped according to gate
// Repartition : 1456 "Ignoring non-enemy", 145 "No AA weapon", 19 "Ignoring empty aircraft".
//
// ======================================================================================
//  LA CAUSE -- C'EST AMONT, PAS CHEZ NOUS
// ======================================================================================
// Ces Print sont pourtant CONDITIONNES par `m_bDebug` dans le mod
// (WCS_AI_AntiHeliEngagementComponent.c, DebugState / DebugRejectedTarget).
// Et notre Character_MEI_Air.et met bien `m_bDebug 0`.
//
// Le coupable est le prefab RACINE du mod : AIUsingStingers surcharge
// `Prefabs/Characters/Core/Character_Base.et` et y pose le composant avec **m_bDebug 1** :
//
//     SCR_ChimeraCharacter {
//      components {
//       Deko_AIAntiAirFireController { }
//       WCS_AI_AntiHeliEngagementComponent { m_bFreezeMovementOnLock 1  m_bDebug 1 }
//       WCS_AI_AntiVehicleEngagementComponent { m_fMinEngagementRange 300 ... }
//      }
//     }
//
// Donc TOUS les personnages du jeu heritent du composant avec le debug ACTIF -- pas
// seulement les servants sol-air. Ses propres prefabs de specialistes (Character_US_Air,
// Character_FIA_Air, Character_USSR_Air) sont, eux, bien a 0 : c'est un oubli amont,
// present dans la version installee le 18/09.
//
// ======================================================================================
//  POURQUOI CE CORRECTIF ET PAS UN OVERRIDE DE PREFAB
// ======================================================================================
// Remettre `m_bDebug 0` demanderait de shadow-override `Character_Base.et` au GUID du jeu
// de base -- exactement le piege documente dans la memoire
// `enfusion-shadow-override-full-content` : un .et au meme GUID REMPLACE le fichier, et
// tout composant non re-declare est perdu EN SILENCE. Sur le prefab racine de tous les
// personnages, c'est hors de question.
//
// Modder le COMPOSANT est sans danger (cf. la regle inverse sur les contextes de .conf :
// composants et controleurs se moddent, pas les classes citees dans un .conf). On neutralise
// donc les deux methodes de log, ce qui coupe le spam quelle que soit la valeur du flag --
// y compris si une mise a jour du mod le remet a 1.
//
// ⚠️ EFFET DE BORD ASSUME : ca rend `m_bDebug` inoperant pour ces composants. Si un jour on
// veut vraiment deboguer le ciblage sol-air / antichar de WCS, il faut commenter ce fichier.
//
// A SIGNALER A L'AUTEUR du mod : `m_bDebug 1` laisse sur Character_Base.et.
//
// Serveur ET client (le Print vient du composant, qui tourne des deux cotes).
// Chaines ASCII (le dedie compile en strict, cf. memoire).

modded class WCS_AI_AntiHeliEngagementComponent
{
	override protected void DebugState(string message) {}
	override protected void DebugRejectedTarget(string message) {}
}

modded class WCS_AI_AntiVehicleEngagementComponent
{
	// Meme oubli potentiel, meme traitement. Character_Base.et ne met pas `m_bDebug` sur ce
	// composant-la (donc il vaut false et il est muet aujourd'hui), mais il a les memes
	// methodes plus une troisieme -- autant les couper toutes les trois maintenant plutot
	// que de revenir le jour ou une mise a jour du mod l'active.
	override protected void DebugState(string message) {}
	override protected void DebugRejectedTarget(string message) {}
	override protected void DebugCandidateReason(string message) {}
}
