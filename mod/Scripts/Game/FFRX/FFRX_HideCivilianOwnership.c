// FF - REMIXED - PVE
// MASQUE L'INTERFACE DE "FF - CIVILIAN OWNERSHIP" -- on subit le mod, on ne le joue pas.
//
// ======================================================================================
//  POURQUOI CE FICHIER EXISTE
// ======================================================================================
// Reoccupation 6.4.4 declare `FF - Civilian Ownership` (58CD60EC5CFA455A) en dependance
// DURE. On ne peut donc ni le retirer ni l'eviter :
//   - il n'est PAS dans nos mods de config.json, il arrive par resolution de dependances ;
//   - epingler une ancienne version de Reoccupation ne sert a rien, le Workshop resout
//     toujours les dependances de la DERNIERE version (confirme par JohnnyKerner) ;
//   - et Reoccupation l'APPELLE vraiment (inspection aux postes de controle, repos en
//     interieur possede, prix des localites) -- l'auteur a confirme qu'il ne peut pas
//     l'extraire.
//
// Mais on ne joue pas a ca : REMIXED est une campagne de guerilla, pas un jeu de vie
// civile. Les joueurs n'ont donc aucune raison de voir de l'immobilier et de l'ameublement
// dans leur menu d'interaction -- en anglais, qui plus est.
//
// ======================================================================================
//  CE QU'ON MASQUE, ET OU CA APPARAISSAIT
// ======================================================================================
//   - "Look up property / manage home" et "Furnish home and garden" : sur CHAQUE PORTE de
//     la carte. Le mod shadow-override `Prefabs/Structures/BuildingParts/Doors/Door_Base.et`
//     (le prefab de porte du JEU DE BASE) pour y greffer ses deux actions.
//   - "Sell ..." : sur chaque meuble pose.
//   - Le HUD de RISQUE DE VOL : un pourcentage greffe sur les icones d'etat de FF.
//
// L'action vehicule ("Vehicle sales and supplies") n'est PAS masquee ici : elle ne
// s'affiche que sur une station-service non liee (`FFCO_GasShopComponent`), donc seulement
// si le mod en a instancie une. Si elle apparait en jeu, ajouter un `modded JWK_ShopAction`
// -- mais inutile de payer pour un cas qu'on n'a pas observe.
//
// ======================================================================================
//  POURQUOI ON MOD LES CLASSES ET PAS LE PREFAB
// ======================================================================================
// La voie evidente etait de re-overrider `Door_Base.et` a notre tour. Ecartee :
//   1. ca devient une course a l'ordre de chargement entre deux addons, et cet ordre n'est
//      PAS garanti -- si Civilian Ownership charge apres nous, il gagne et le travail ne
//      sert a rien ;
//   2. il faudrait recopier les 76 lignes du prefab de porte du jeu de base, et le
//      resynchroniser a chaque mise a jour d'Arma.
//
// Modder les CLASSES d'action est insensible a l'ordre de chargement et tient en quelques
// lignes. C'est le precedent de FFRX_SilenceStingerDebug.c, qui neutralise de la meme
// facon les logs de debug d'AIUsingStingers.
//
// ⚠️ ON NE CASSE RIEN COTE LOGIQUE. On masque seulement l'AFFICHAGE : le mod continue de
// tourner, sa persistance et ses donnees restent intactes, et les integrations dont
// Reoccupation depend continuent de fonctionner. Si on empechait son demarrage, ces
// integrations interrogeraient un manager jamais pret.
//
// ⚠️ A SURVEILLER a chaque mise a jour du mod : si les noms de classe changent, le
// masquage cesse SANS erreur -- les actions reapparaitraient simplement en jeu. C'est le
// genre de regression qu'on ne voit qu'en jouant.
//
// Client (affichage). Chaines ASCII (le dedie compile en strict).

//------------------------------------------------------------------------------------------------
//! "Look up property / manage home" -- sur toutes les portes.
modded class FFCO_PropertyAction
{
	override bool CanBeShownScript(IEntity user)
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! "Furnish home and garden" -- sur toutes les portes.
modded class FFCO_FurnishAction
{
	override bool CanBeShownScript(IEntity user)
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! "Sell ..." -- sur le mobilier pose.
modded class FFCO_RemoveFurnitureAction
{
	override bool CanBeShownScript(IEntity user)
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Le HUD de risque de vol.
//!
//! On neutralise le rafraichissement plutot que de toucher au widget : `Update()` est le
//! seul point d'entree (appele depuis `modded JWK_StatusIconsHandlerUIComponent.Refresh`),
//! et `Destroy()` retire proprement ce qui aurait pu etre cree avant notre passage --
//! par exemple si le HUD s'est affiche une fois avant qu'on prenne la main.
modded class FFCO_TheftHUD
{
	override static void Update()
	{
		Destroy();
	}
}
