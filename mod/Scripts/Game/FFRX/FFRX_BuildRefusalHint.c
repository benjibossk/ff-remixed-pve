// FF - REMIXED - PVE
// Dire POURQUOI le menu de construction ne s'ouvre pas.
//
// ------------------------------------------------------------------------------------
// LE PROBLEME : UN REFUS MUET
//
// Un joueur qui declenche l'action de construction sans remplir les conditions n'obtient
// RIEN : pas de menu, pas de refus, pas de raison. Il recommence, il insiste, il conclut
// a un bug du serveur -- et il vient le signaler. C'est une des sources de confusion les
// plus regulieres, alors que l'information existe deja dans le moteur.
//
// ------------------------------------------------------------------------------------
// CE QUE FAIT FF, ET CE QUI MANQUE
//
// `JWK_AssetSelectionMainMenuController.OnPerformed` gere pourtant le cas : si la liste
// d'objets est vide, il joue un son d'erreur et affiche `m_iEmptySelectionFeedback`...
// **si ce champ est renseigne**. Pour la construction il ne l'est pas -> son d'erreur
// seul, aucun texte. Le canal existe, personne ne l'a branche.
//
// On ne "repare" donc pas FF : on remplit le trou qu'il a laisse, avec SA mecanique.
//
// ------------------------------------------------------------------------------------
// D'OU VIENT LA RAISON AFFICHEE
//
// La liste est vide quand AUCUN objet ne passe `CheckCanBuild`. Or cette methode a une
// surcharge qui rend la raison du refus sous forme de `JWK_EFeedback` -- le meme type que
// `JWK.GetHint().ShowFeedback()` sait afficher, deja traduit et deja au style du jeu.
//
// On interroge donc tous les objets et on affiche la raison **majoritaire** : s'il y a 20
// batiments et que 18 sont refuses pour "pas assez de ravitaillement", c'est ca qu'il faut
// dire. Prendre la premiere raison venue donnerait un message exact mais anecdotique
// (le refus d'un item marginal), et donc trompeur.
//
// Cas particulier traite a part : AUCUNE zone de construction. Ce n'est pas un refus par
// objet -- il n'y a rien contre quoi tester -- donc aucune `JWK_EFeedback` ne sort. On
// ecrit alors notre propre texte, dans le meme bandeau de retour.
//
// ------------------------------------------------------------------------------------
// PORTEE
//
// UI CLIENT pure : on n'ouvre ni n'autorise rien de plus, on explique seulement un refus
// qui a deja eu lieu. La decision reste entierement a FF.
//
// La commande `#build` (FFRX_BuildCommand) a deja son propre message et n'est pas
// concernee : le cas muet est l'ouverture par l'outil / le menu radial.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

modded class JWK_AssetSelectionMainMenuController
{
	override protected void OnPerformed()
	{
		FFRX_ExplainIfEmpty();

		// On delegue TOUJOURS : FF garde son son d'erreur, son eventuel feedback
		// configure, et surtout l'ouverture normale du menu quand tout va bien.
		super.OnPerformed();
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_ExplainIfEmpty()
	{
		typename type = m_sHandlerClass.ToType();
		if (type == typename.Empty)
			return;

		// On ne parle QUE de la construction : ce controleur sert aussi a d'autres
		// selections d'assets (vehicules...), qui ont leurs propres retours.
		if (type != JWK_ConstructionSelectionMenuHandler)
			return;

		JWK_AssetSelectionMenuHandler handler = JWK_AssetSelectionMenuHandler.Cast(type.Spawn());
		if (!handler || handler.GetItemsCount() > 0)
			return;   // le menu va s'ouvrir normalement, rien a expliquer

		FFRX_ShowBuildRefusal();
	}

	//------------------------------------------------------------------------------------------------
	protected void FFRX_ShowBuildRefusal()
	{
		FFRX_BuildRefusalUtil.Explain();
	}
}

// ----------------------------------------------------------------------------
//  L'explication, sortie de la classe moddee pour etre reutilisable.
//
//  Elle sert maintenant a DEUX appelants : l'ouverture par la roue de FF (ci-dessus) et
//  l'ouverture directe par la touche J (FFRX_BuildDirect). Une seule redaction du refus,
//  donc un message identique quel que soit le chemin emprunte.
// ----------------------------------------------------------------------------
class FFRX_BuildRefusalUtil
{
	static void Explain()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return;

		JWK_ConstructionManagerComponent cm = JWK.GetConstruction();
		if (!cm)
			return;

		vector pos = player.GetOrigin();
		JWK_BuildAreaControllerComponent area = cm.GetEffectiveAreaController(pos);

		// --- Cas 1 : hors de toute zone. Aucun objet n'a meme ete teste.
		if (!area)
		{
			FFRX_Chat.Local("Construction impossible : tu es hors zone. Rapproche-toi d'une FOB, d'un camp ou d'une base tenue.");
			return;
		}

		// --- Cas 2 : dans une zone, mais tout est refuse. On cherche la raison MAJORITAIRE.
		array<JWK_BaseConstructionItemConfig> all = cm.GetAllConstructionItemConfigs();
		if (!all || all.IsEmpty())
		{
			FFRX_Chat.Local("Construction impossible : aucun batiment n'est disponible sur ce serveur.");
			return;
		}

		JWK_EFeedback best = JWK_EFeedback.UNDEFINED;
		int bestCount = 0;
		int tested = 0;

		// Comptage a la main plutot qu'une map : une poignee de raisons distinctes, et
		// ca evite d'allouer une table a chaque pression de touche.
		array<int> seenReason = {};
		array<int> seenCount  = {};

		foreach (JWK_BaseConstructionItemConfig cfg : all)
		{
			JWK_BuildItemConfig buildItem = JWK_BuildItemConfig.Cast(cfg);
			if (!buildItem)
				continue;   // les placeables passent par S_CanPlace, qui ne rend pas de raison

			tested++;

			JWK_EFeedback reason;
			if (cm.CheckCanBuild(buildItem, area, pos, true, reason))
				continue;   // celui-la passe : la liste ne devrait pas etre vide
			if (reason == JWK_EFeedback.UNDEFINED)
				continue;

			int idx = seenReason.Find(reason);
			if (idx < 0)
			{
				seenReason.Insert(reason);
				seenCount.Insert(1);
				idx = seenReason.Count() - 1;
			}
			else
			{
				seenCount[idx] = seenCount[idx] + 1;
			}

			if (seenCount[idx] > bestCount)
			{
				bestCount = seenCount[idx];
				best = reason;
			}
		}

		if (best != JWK_EFeedback.UNDEFINED)
		{
			// On reprend le LIBELLE de FF (deja redige, deja traduit) mais on l'affiche
			// dans le chat : on ne reecrit pas ce qu'il sait deja dire, et on ne subit
			// pas pour autant le caractere fugace du bandeau.
			JWK_HintManagerComponent hint = JWK.GetHint();
			string txt;
			if (hint)
				txt = hint.FFRX_FeedbackText(best);

			if (txt != "")
			{
				FFRX_Chat.Local("Construction impossible : " + FFRX_Chat.FFRX_Translate(txt));
				return;
			}
		}

		if (tested == 0)
			FFRX_Chat.Local("Construction impossible : aucun batiment n'est disponible sur ce serveur.");
		else
			FFRX_Chat.Local("Construction impossible ici pour le moment.");
	}

}
