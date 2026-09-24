// FF - REMIXED - PVE
// Ecrire une ligne dans le CHAT du joueur local.
//
// ------------------------------------------------------------------------------------
// POURQUOI LE CHAT PLUTOT QUE LE HINT
//
// Choix Benji (2026-09-10) : nos messages passent par le chat, pas par le bandeau de
// retour de FF. Le hint a deux defauts pour ce qu'on en fait :
//   - il est FUGACE : il s'efface au bout de quelques secondes, et un joueur occupe le
//     rate completement. Le chat, lui, garde l'historique -- on peut relire.
//   - il est UNIQUE : `ShowFeedback` masque le hint courant pour afficher le nouveau, et
//     refuse meme un hint de priorite inferieure. Deux informations coup sur coup et la
//     premiere disparait sans avoir ete lue.
//
// Le chat n'a aucun de ces deux problemes, et c'est deja la ou les joueurs regardent
// quand ils cherchent une reponse.
//
// ------------------------------------------------------------------------------------
// PORTEE : LOCAL, PAS RESEAU
//
// `SCR_ChatPanelManager.OnNewMessage(string)` insere une ligne dans le chat de CE client
// seulement -- rien n'est envoye au serveur ni aux autres joueurs. C'est exactement ce
// qu'on veut pour un retour personnel ("pourquoi je ne peux pas construire ici").
//
// Pour ecrire a un joueur DEPUIS le serveur, ce n'est pas ce chemin : il faut passer par
// un RPC vers le client concerne (cf. FFRX_ProcurementShop.FFRX_SendIntelHint).
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

class FFRX_Chat
{
	//! Prefixe commun : dans un chat partage avec les messages des joueurs, une ligne
	//! systeme doit se distinguer au premier coup d'oeil.
	protected static const string PREFIX = "[REMIXED] ";

	//------------------------------------------------------------------------------------------------
	//! Ecrit une ligne dans le chat du joueur local. Sans effet cote serveur dedie
	//! (pas de panneau de chat), ce qui est le comportement voulu.
	static void Local(string text)
	{
		if (text == "")
			return;

		SCR_ChatPanelManager mgr = SCR_ChatPanelManager.GetInstance();
		if (!mgr)
			return;

		mgr.OnNewMessage(PREFIX + FFRX_Strip(FFRX_Translate(text)));
	}

	//------------------------------------------------------------------------------------------------
	//! Retire le balisage de mise en forme prevu pour le BANDEAU, que le chat n'interprete pas.
	//!
	//! Constate en jeu : « Equipement rendu (6 objets) : +1 ravito.<br/><br/> » -- les
	//! sauts de ligne s'affichaient en clair. Les textes de FF (et les notres, ecrits a
	//! l'epoque du bandeau) contiennent ce balisage ; le chat, lui, est une ligne simple.
	//!
	//! On nettoie ICI plutot que dans chaque message : le helper est le point de passage
	//! oblige, donc la correction couvre aussi les libelles de FF qu'on ne controle pas.
	//! Un saut de ligne devient une ESPACE -- le supprimer collerait deux phrases
	//! ("...ravito.Prochain envoi"), ce qui est pire que le balisage visible.
	static string FFRX_Strip(string s)
	{
		if (s == "")
			return s;

		// Les trois graphies rencontrees. `Replace` modifie la chaine en place.
		s.Replace("<br/>", " ");
		s.Replace("<br />", " ");
		s.Replace("<br>", " ");

		// Espaces doubles laisses par les balises consecutives.
		s.Replace("  ", " ");
		s.Replace("  ", " ");

		s.TrimInPlace();
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Resout une cle de localisation ("#JWK-...") en texte lisible.
	//!
	//! Necessaire parce qu'on reutilise les libelles de FF, qui sont des CLES : l'UI les
	//! traduit toute seule, le chat non -- sans ca le joueur lirait "#JWK-Feedback-..."
	//! au lieu d'une phrase. Un texte deja en clair traverse sans etre touche.
	static string FFRX_Translate(string s)
	{
		// ⚠️ Pas d'indexation `s[0]` en Enforce, et surtout : `out` est un MOT RESERVE,
		// l'utiliser comme nom de variable donne un "Broken expression" au numero de
		// ligne trompeur. D'ou `s.Get(0)` et `translated`.
		if (s == "" || s.Get(0) != "#")
			return s;

		string translated;
		if (WidgetManager.Translate(translated, s))
			return translated;

		return s;   // cle inconnue : mieux vaut afficher la cle que rien du tout
	}
}

// ------------------------------------------------------------------------------------
//  Acces au libelle d'un JWK_EFeedback
// ------------------------------------------------------------------------------------
//! FF sait afficher un `JWK_EFeedback` dans son bandeau, mais le TEXTE correspondant vit
//! dans `m_Config`, qui est protege et sans accesseur. On ouvre juste une fenetre en
//! lecture : indispensable des lors qu'on veut ecrire la meme information ailleurs (ici
//! le chat) plutot que de reecrire a la main des dizaines de libelles deja traduits.
modded class JWK_HintManagerComponent
{
	// ------------------------------------------------------------------------------------
	//  TOUT LE BANDEAU DE RETOUR PART DANS LE CHAT (decision Benji, 2026-09-10)
	// ------------------------------------------------------------------------------------
	//
	// Le bandeau de FF prend trop de place a l'ecran. On le supprime, et on redirige son
	// contenu vers le chat.
	//
	// POURQUOI ICI ET NULLE PART AILLEURS : cette methode est l'ENTONNOIR. La variante
	// `ShowFeedback(JWK_EFeedback)` ne fait que resoudre son libelle puis appeler
	// celle-ci, et `JWK_PlayerControllerComponent.ShowFeedback` (le chemin reseau utilise
	// par les dialogues civils) y aboutit aussi. Un seul point d'interception attrape
	// donc TOUT -- nos messages comme ceux de FF -- sans toucher a un seul appelant.
	//
	// CE QU'ON GARDE : le son d'erreur. Il porte une information (« ton action a
	// echoue ») qui arrive avant meme la lecture, et un joueur qui a le chat replie n'a
	// plus que lui. On perd le bandeau, pas le signal.
	//
	// CE QU'ON PERD, ASSUME : la barre de progression et le minuteur de certains hints
	// (`IsTimerVisible`), qui n'ont pas d'equivalent en texte. Si un compte a rebours
	// devient necessaire un jour, il faudra le reintroduire autrement -- ne pas rebrancher
	// le bandeau juste pour ca.
	//
	// On NE delegue PAS a super : c'est lui qui affiche le bandeau.
	override bool ShowFeedback(JWK_FeedbackUIInfo info, bool isSilent = false, bool ignoreShown = false)
	{
		if (!info)
			return false;

		string txt = info.GetDescription();
		if (txt == "")
			txt = info.GetName();

		FFRX_Chat.Local(txt);

		if (!isSilent && info.ErrorSound())
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);

		return true;
	}

	//! Libelle d'un feedback, "" si introuvable.
	string FFRX_FeedbackText(JWK_EFeedback feedback)
	{
		if (!m_Config)
			return "";

		JWK_FeedbackUIInfo info = m_Config.GetFeedbackHintByEnum(feedback);
		if (!info)
			return "";

		string desc = info.GetDescription();
		if (desc != "")
			return desc;

		return info.GetName();
	}
}
