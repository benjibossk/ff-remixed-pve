// FF - REMIXED - PVE
// Le HUD de la LOI MARTIALE (Reoccupation) ne s'affiche plus.
//
// ======================================================================================
//  CE QUI RESTAIT A L'ECRAN, ET POURQUOI
// ======================================================================================
//
// FFRX_SilenceNotifications.c avait deja fait taire les canaux de TEXTE : popups DARC,
// file de notifications de Reoccupation, bandeau de bataille, panneau des escadrons de la
// mort. Mais le module Loi martiale (actif chez nous -- couvre-feu 21h-05h, limites de
// vitesse, checkpoints) accroche trois elements de HUD a un endroit different : il modde
// JWK_StatusIconsHandlerUIComponent.Refresh() et y appelle
//
//     FFML_FrontierProgressHUD.Update()
//     FFML_SpeedLimitHUD.Update(visibility)
//     FFML_InspectionRiskHUD.Update(visibility)
//
// Ces trois-la echappaient donc completement au silence precedent.
//
// ======================================================================================
//  POURQUOI ON MODDE CES TROIS CLASSES, ET PAS Refresh()
// ======================================================================================
//
// Surcharger JWK_StatusIconsHandlerUIComponent.Refresh() ne marcherait PAS. Reoccupation
// est une dependance de REMIXED, donc sa classe moddee est compilee AVANT la notre : la
// notre en HERITE. Appeler super.Refresh() executerait sa version, appels de HUD compris.
// On ne peut pas "sauter" le code d'un parent.
//
// On agit donc a la source : chaque HUD a un unique point d'entree statique `Update()`,
// qu'on rend inoperant. Verifie avant d'ecrire : aucune des trois classes n'est citee dans
// un .conf, elles sont donc moddables sans risque (a la difference des contextes UI --
// cf. l'avertissement en tete de FFRX_TrimMainMenu.c, ou modder une classe citee en .conf
// avait tue la touche J en silence pendant trois sessions).
//
// ======================================================================================
//  CE QU'ON NE TOUCHE PAS, ET C'EST DELIBERE
// ======================================================================================
//
// 1. LE MODULE LOI MARTIALE RESTE ACTIF. Couvre-feu, limites de vitesse et checkpoints
//    continuent de s'appliquer : on retire l'AFFICHAGE, pas la regle. Le joueur apprend
//    la limite en se faisant arreter, pas en lisant un compteur -- c'est le meme parti
//    pris que pour le reste du brouillard de guerre.
//
// 2. LES CINQ REGLAGES "INTELLIGENCE & NOTIFICATIONS" de Reoccupation restent a OUI.
//    Il serait tentant de les passer a NON puisqu'ils existent, mais leurs descriptions
//    amont sont formelles : les couper ne masque pas un texte, ca SUPPRIME DU CONTENU.
//      - "Show Patrol destruction message" -> coupe aussi le DOCUMENT d'intel recuperable
//        sur le cadavre d'un patrouilleur.
//      - "Enable counterattack after-action reports" -> empeche la collecte et la
//        LIVRAISON du rapport.
//      - "Show intelligence reports" -> plus aucun rapport n'est genere, donc
//        FFRX_RadioIntel.CaptureFromQueue n'aurait plus rien a intercepter, et le
//        renseignement gagne en capturant les tours radio disparaitrait.
//    Nos notifications etant deja interceptees a la file, le texte ne s'affiche pas et le
//    CONTENU, lui, est conserve et reroute. Couper ces reglages detruirait ce travail.
//
// Le suivi cote admin reste possible : le serveur continue de journaliser
// ([FF_Reoccupation][...]), et c'est cette source que lit le site.
//
// NOTE : ASCII uniquement dans les chaines (le dedie compile en strict).

//! Barre de progression de la frontiere (avancement des travaux de bornage).
modded class FFML_FrontierProgressHUD
{
	override static void Update()
	{
	}
}

//! Panneau de limitation de vitesse en vehicule.
modded class FFML_SpeedLimitHUD
{
	override static void Update(JWK_PlayerVisibilityComponent visibility)
	{
	}
}

//! Jauge de "risque de controle" a l'approche d'un checkpoint.
modded class FFML_InspectionRiskHUD
{
	override static void Update(JWK_PlayerVisibilityComponent visibility)
	{
	}
}
