// FF - REMIXED - PVE
// Allege la roue "J" : on retire les entrees solo (Journal, Profil, Progression, Banque,
// Radio) et on garde Construction + Call-to-Action.
//
// ======================================================================================
//  ⚠️ NE JAMAIS FAIRE `modded class JWK_MainMenuContext` -- BUG COUTEUX, RELIRE CECI
// ======================================================================================
//
// La premiere version de ce fichier moddait JWK_MainMenuContext pour surcharger
// InitRadialMenuEntries. Resultat, au chargement du monde :
//
//     DEFAULT (E): Unknown class 'JWK_MainMenuContext' at offset 2880(0xb40)
//
// Le moteur n'arrivait plus a resoudre la classe en lisant Configs/UI/UIManager.conf, et
// **jetait l'entree silencieusement**. Le contexte du menu radial n'etait donc JAMAIS
// construit : 21 contextes charges au lieu de 22. Mesure faite des deux cotes :
//
//     avec le modded class : 21 contextes, JWK_MainMenuContext ABSENT  -> J ne fait rien
//     sans le modded class : 22 contextes, JWK_MainMenuContext present -> J fonctionne
//
// Symptome pour le joueur : la touche J morte, le journal inaccessible, et AUCUN log --
// y compris aucun log de nos propres sondes, puisque le code qui les portait n'etait
// jamais instancie. On a cherche pendant trois sessions du cote de la pelle, de
// l'escouade, des collisions de touches et du gate de construction, alors que le menu
// n'existait tout simplement pas.
//
// A retenir : modder un CONTROLEUR du menu est sans danger (FFRX_BuildRefusalHint.c
// modde JWK_AssetSelectionMainMenuController et n'a jamais pose probleme). C'est modder
// le CONTEXTE -- l'objet cite nommement dans le .conf -- qui casse sa resolution.
//
// ======================================================================================
//  LA BONNE METHODE : le point d'accroche que FF fournit deja
// ======================================================================================
//
// JWK_MainMenuContext.InitRadialMenuEntries() genere les entrees puis les passe a un
// ScriptInvoker public, prevu exactement pour qu'un mod les ajuste :
//
//     GetOnRadialMenuEntrySetup().Invoke(entries);
//
// On s'y abonne. Aucune classe de config n'est moddee, donc rien ne peut casser son
// instanciation. Et on travaille sur le tableau final, ce qui est meme plus simple.
//
// ======================================================================================
//  ON NEUTRALISE LES CONTROLEURS, PAS LES ENTREES
// ======================================================================================
//
// Filtrer le tableau d'entrees par libelle aurait ete l'ideal, mais `SCR_SelectionMenuEntry.Name`
// est protege : impossible d'identifier une entree apres coup. On agit donc a la source, en
// surchargeant `GenerateEntries()` des controleurs solo pour qu'ils ne produisent rien.
//
// Les 5 entrees visees se repartissent sur exactement 3 classes (cf. UIManager.conf) :
//
//     JWK_OpenContextMainMenuController  -> Journal, Profil, Banque
//     JWK_GameProgressMainMenuController -> Progression
//     JWK_RadioMainMenuController        -> Radio
//
// Restent intacts JWK_AssetSelectionMainMenuController (Construction) et
// JWK_CtaMainMenuController (Call-to-Action) -- on ne les touche pas du tout.
//
// Le ScriptInvoker de FF reste utilise, mais uniquement comme GARDE-FOU : il ne filtre
// plus rien, il signale seulement le cas ou la roue s'ouvrirait vide. Une roue vide est
// indiscernable d'une touche cassee, et c'est precisement la confusion qui nous a coute
// trois sessions -- on veut qu'elle soit dite, pas devinee.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

class FFRX_MainMenuTrim
{
	protected static bool s_bHooked;

	//------------------------------------------------------------------------------------------------
	//! Branche le filtre sur le menu radial de FF. Idempotent : sans effet si deja fait.
	//! Rend false tant que le contexte n'existe pas encore (l'appelant reessaiera).
	static bool TryHook()
	{
		if (s_bHooked)
			return true;

		if (!JWK.GetUI())
			return false;

		JWK_MainMenuContext ctx = JWK_UIContextTU<JWK_MainMenuContext>.Get();
		if (!ctx)
			return false;

		ctx.GetOnRadialMenuEntrySetup().Insert(OnEntriesReady);

		s_bHooked = true;
		Print("[FFRX][Menu] garde-fou de la roue J branche (sans modder le contexte).", LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Appele par FF juste apres la generation des entrees, a chaque ouverture de la roue.
	//! Ne modifie RIEN : on observe seulement le cas degrade.
	static void OnEntriesReady(array<ref SCR_SelectionMenuEntry> entries)
	{
		if (entries && !entries.IsEmpty())
			return;

		Print("[FFRX][Menu] roue J ouverte mais VIDE -- le joueur va croire que la touche est cassee.", LogLevel.WARNING);
		FFRX_Chat.Local("Rien a afficher ici : sors la pelle (touche du slot correspondant) pour acceder a la construction.");
	}
}

// ------------------------------------------------------------------------------------
//  Les controleurs solo : ils ne generent plus rien.
//
//  Modder un CONTROLEUR est sans danger -- contrairement au contexte, il n'est pas cite
//  comme classe racine dans UIManager.conf, donc sa resolution ne peut pas echouer.
//  (Relire l'avertissement en tete de fichier.)
// ------------------------------------------------------------------------------------

//! Journal, Profil et Banque passent tous les trois par ce controleur.
modded class JWK_OpenContextMainMenuController
{
	override void GenerateEntries(array<ref SCR_SelectionMenuEntry> entries)
	{
	}
}

//! Progression / Game Progress.
modded class JWK_GameProgressMainMenuController
{
	override void GenerateEntries(array<ref SCR_SelectionMenuEntry> entries)
	{
	}
}

//! Radio (l'usage de la radio passe par ailleurs en jeu).
modded class JWK_RadioMainMenuController
{
	override void GenerateEntries(array<ref SCR_SelectionMenuEntry> entries)
	{
	}
}

// ------------------------------------------------------------------------------------
//  AMORCAGE : depuis FFRX_BuildDirect.Arm(), lui-meme appele par le
//  modded SCR_PlayerController.OnControlledEntityChanged (cf. FFRX_IntroCinematic.c).
//
//  Il y avait ici un `GameSystem` en WorldSystemLocation.Client. Il ne s'amorce JAMAIS
//  chez un joueur connecte a un serveur dedie : mesure du 2026-09-17, la ligne
//  "[FFRX][Menu] garde-fou ... branche" n'est jamais apparue dans le log client alors que
//  le fichier etait bien present dans le pak publie. Meme constat pour
//  FFRX_BuildDirectSystem, construit a l'identique.
//
//  A retenir pour tout code CLIENT de cet addon : ne pas compter sur un GameSystem client,
//  passer par un hook du PlayerController local.
// ------------------------------------------------------------------------------------
