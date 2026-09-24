// FF - REMIXED - PVE
// Touche J = ouvre DIRECTEMENT le menu de construction, sans passer par la roue radiale,
// et seulement pour l'escouade du genie.
//
// ======================================================================================
//  POURQUOI ON COURT-CIRCUITE LA ROUE
// ======================================================================================
//
// La roue de FF ne porte plus qu'une seule entree utile chez nous (Construction) : les
// cinq autres sont neutralisees dans FFRX_TrimMainMenu.c. Elle coutait donc un clic pour
// rien -- et quand l'entree Construction ne se genere pas, elle s'ouvre VIDE, ce que le
// joueur lit comme "la touche est cassee" (constate sur le dedie).
//
// On garde la touche J (deja liee par FF, et le bandeau du HUD annonce "J Construction
// menu") mais on ouvre le menu nous-memes, exactement comme le fait
// JWK_AssetSelectionMainMenuController.OnPerformed :
//
//     JWK_UIContextTU<JWK_AssetSelectionMenuContext>.Get().SetHandler(handler);
//     JWK.GetUI().OpenContext(JWK_AssetSelectionMenuContext);
//
// La roue s'ouvre quand meme derriere (son controleur ecoute la meme action, et on ne
// peut pas l'en empecher sans modder JWK_MainMenuContext -- interdit, cf. l'avertissement
// en tete de FFRX_TrimMainMenu.c). Mais le contexte du menu a `m_bHideHUDOnShow = 1` par
// defaut : ouvrir le menu masque tout le HUD, roue comprise. Elle est donc invisible.
//
// ======================================================================================
//  LES TROIS VERROUS, ET POURQUOI CHACUN PARLE
// ======================================================================================
//
// 1. une pelle en main   2. reconnue comme outil de construction   3. escouade du genie
//
// Chaque refus DIT lequel a bloque. C'est le coeur du probleme qu'on traine : jusqu'ici
// l'echec etait muet et indiscernable d'un bug de touche, ce qui a coute plusieurs
// sessions de recherche. Un refus explicite se diagnostique en une seconde.
//
// Le refus n2 affiche le nom du prefab tenu -- utile car toutes les pelles ne sont PAS
// des outils de construction : la branche `ETool_*_carrier*.et` derive de
// Equip_Accessory_base.et et n'herite donc jamais du JWK_ConstructionToolItemComponent
// qu'on ajoute sur ETool_ALICE.et / ETool_MPL50.et.
//
// NOTE : ASCII uniquement dans les chaines (le build du dedie desynchronise sur l'UTF-8).

class FFRX_BuildDirect
{
	// Fragment cherche dans le nom de l'escouade, en MAJUSCULES.
	//
	// "NIE" et non "GENIE" volontairement : le nom reel porte un accent ("ECHO - Genie"
	// s'ecrit avec un E accentue cote site), et on ne peut pas mettre d'accent dans une
	// chaine ici (regle ASCII du build dedie). "GENIE" comme "GENIE-accentue" contiennent
	// tous deux "NIE", donc ce fragment couvre les deux orthographes sans dependre de
	// l'encodage. Risque de faux positif negligeable sur des indicatifs d'escouade.
	static const string SQUAD_TOKEN = "NIE";

	//! Le COMMANDEMENT ouvre aussi le menu (modele arrete par Benji le 2026-09-24 : le
	//! commandement et le genie choisissent QUOI construire, les autres peuvent seulement
	//! aider a monter un batiment deja pose). L'escouade s'appelle "KILO - Commandement" ;
	//! "KILO" est un indicatif OTAN, donc sans accent et sans ambiguite.
	static const string SQUAD_TOKEN_HQ = "KILO";

	protected static bool s_bArmed;

	//------------------------------------------------------------------------------------------------
	static void Arm()
	{
		if (s_bArmed)
			return;

		InputManager im = GetGame().GetInputManager();
		if (!im)
			return;

		im.AddActionListener("JWK_MainMenu", EActionTrigger.DOWN, FFRX_OnBuildKey);
		s_bArmed = true;

		// Le garde-fou de la roue vide souffrait du meme mal (GameSystem client jamais
		// amorce sur le dedie) : on le branche ici, sur le chemin qui marche.
		FFRX_MainMenuTrim.TryHook();

		SuppressWheel();
		Print("[FFRX][Build] touche J branchee sur l'ouverture directe du menu de construction.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static void FFRX_OnBuildKey(float value, EActionTrigger reason)
	{
		TryOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! Empeche DEFINITIVEMENT la roue radiale de s'ouvrir.
	//!
	//! FF expose exactement ce qu'il faut : `JWK_GetOnAboutToOpen()` est invoque AVANT
	//! l'ouverture, et `JWK_SetPreventOpening(true)` l'annule "sans effet de bord" (ce sont
	//! les mots du commentaire amont). C'est le mecanisme que FF utilise lui-meme pour
	//! bloquer la roue quand le joueur est mort.
	//!
	//! On atteint l'instance par `SCR_RadialMenu.GlobalRadialMenu()`, statique et publique
	//! -- ce qui evite d'avoir a toucher a JWK_MainMenuContext, dont le `m_RadialMenuController`
	//! est protege et qu'il est de toute facon INTERDIT de modder (cf. FFRX_TrimMainMenu.c).
	//!
	//! ATTENTION, CONSEQUENCE ASSUMEE : la roue ne s'ouvre plus DU TOUT, donc l'entree Call-to-Action
	//! (la seule qu'on gardait avec Construction) n'est plus atteignable par J. C'est le
	//! choix demande : J sert uniquement a construire. Pour revenir en arriere, il suffit de
	//! ne plus appeler SuppressWheel().
	protected static void SuppressWheel()
	{
		SCR_RadialMenu rm = SCR_RadialMenu.GlobalRadialMenu();
		if (!rm)
		{
			Print("[FFRX][Build] roue radiale introuvable -- elle s'ouvrira encore (vide).", LogLevel.WARNING);
			return;
		}

		rm.JWK_GetOnAboutToOpen().Insert(FFRX_OnWheelAboutToOpen);
		Print("[FFRX][Build] roue radiale supprimee (J ouvre directement la construction).", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static void FFRX_OnWheelAboutToOpen(SCR_RadialMenu menu)
	{
		if (menu)
			menu.JWK_SetPreventOpening(true);
	}

	//------------------------------------------------------------------------------------------------
	//! true si le joueur local appartient a une escouade du genie.
	static bool InEngineerSquad(out string groupName)
	{
		groupName = "";

		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm)
			return false;

		SCR_AIGroup g = gm.GetPlayerGroup(SCR_PlayerController.GetLocalPlayerId());
		if (!g)
			return false;

		groupName = g.GetCustomName();
		if (groupName == "")
			return false;

		string up = groupName;
		up.ToUpper();
		return up.Contains(SQUAD_TOKEN) || up.Contains(SQUAD_TOKEN_HQ);
	}

	//------------------------------------------------------------------------------------------------
	static void TryOpen()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return;

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(player.FindComponent(SCR_CharacterControllerComponent));
		if (!cc)
			return;

		// --- Verrou 1 : une pelle EN MAIN (et pas seulement dans le sac).
		IEntity gadget = cc.GetAttachedGadgetAtLeftHandSlot();
		if (!gadget)
		{
			FFRX_Chat.Local("Sors ta pelle avant d'appuyer sur J (touche du slot ou elle est rangee).");
			return;
		}

		string item = "objet inconnu";
		EntityPrefabData pd = gadget.GetPrefabData();
		if (pd)
			item = FFRX_BuildDiagUtil.ShortName(pd.GetPrefabName());

		// --- Verrou 2 : FF reconnait-il cet objet comme un outil de construction ?
		bool isTool = false;
		if (JWK.GetItemManager())
		{
			JWK_ItemAttributes at = JWK.GetItemManager().GetEntityAttributes(gadget);
			if (at)
				isTool = at.m_bConstructionTool;
		}

		if (!isTool)
		{
			FFRX_Chat.Local("'" + item + "' n'est pas un outil de construction. Prends une pelle de terrassement (ETool) a l'arsenal.");
			Print("[FFRX][Build] refus : outil non reconnu -- prefab=" + item, LogLevel.NORMAL);
			return;
		}

		// --- Verrou 3 : reserve au genie ET au commandement.
		string groupName;
		if (!InEngineerSquad(groupName))
		{
			string where = groupName;
			if (where == "")
				where = "aucune escouade";

			FFRX_Chat.Local("Menu de construction reserve au Genie et au Commandement. Tu es dans : " + where + ". Tu peux quand meme aider a monter un batiment deja pose.");
			return;
		}

		Open();
	}

	//------------------------------------------------------------------------------------------------
	//! Meme sequence que JWK_AssetSelectionMainMenuController.OnPerformed, sans la roue.
	protected static void Open()
	{
		JWK_AssetSelectionMenuHandler handler = new JWK_ConstructionSelectionMenuHandler();

		// Liste vide = rien de constructible ici. On ne laisse pas un menu vide s'ouvrir :
		// FFRX_BuildRefusalHint sait deja expliquer POURQUOI (hors zone, ravitaillement
		// insuffisant, limite atteinte...), on lui delegue plutot que d'afficher un ecran
		// vide de plus.
		if (handler.GetItemsCount() == 0)
		{
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.ERROR);
			FFRX_BuildRefusalUtil.Explain();
			return;
		}

		JWK_UIContextTU<JWK_AssetSelectionMenuContext>.Get().SetHandler(handler);
		JWK.GetUI().OpenContext(JWK_AssetSelectionMenuContext);
	}
}

// ------------------------------------------------------------------------------------
//  L'INDICATION A L'ECRAN ("J Menu de construction")
// ------------------------------------------------------------------------------------
// Les trois verrous ci-dessus refusent l'ouverture, mais FF continuait a AFFICHER
// l'invitation a un joueur qui n'y a pas droit -- on lui montrait une touche pour se faire
// refouler ensuite. On aligne donc l'affichage sur le droit reel.
//
// D'OU VIENT CE TEXTE : JWK_CallToActionInfo.UpdateTrigger(), qui tourne une fois par
// seconde et fait, sans autre condition que l'outil en main :
//
//     if (m_bHasConstructionToolInHand) {
//         ShowCTA("#JWK-CTA-ConstructionMenu");
//         return;
//     }
//
// CE QU'ON GARDE : la pelle sortie reste la condition d'affichage (demande de Benji). On
// n'ajoute que le genie par-dessus. Range ta pelle, l'indication disparait, comme avant.
//
// POURQUOI ShowCTA ET PAS OnGadgetStateChanged : ce dernier ne se declenche qu'au
// changement d'objet en main. Un joueur qui change d'escouade SANS ranger sa pelle
// garderait alors une indication perimee. ShowCTA est rappele a chaque passe (1 s), donc
// l'affichage suit les changements d'escouade en direct.
//
// ⚠️ EFFET DE BORD ASSUME : quand on masque l'indication, UpdateTrigger a deja fait son
// `return` -- il ne retombera donc pas sur les indications suivantes (radio, "ouvre le
// menu"). Un non-genie pelle en main ne verra donc aucune indication, au lieu de celle de
// la radio. C'est le prix a payer pour ne pas reecrire UpdateTrigger en entier, et ca reste
// l'affichage le plus honnete : rien, plutot qu'une touche qui ne marche pas.
//
// Et c'est nous qui masquons le widget : comme super n'est pas appele, personne d'autre ne
// le fera, et l'indication de la passe precedente resterait collee a l'ecran.
//
// Modder un InfoDisplay est sans danger ici (precedent verifie : FFRX_SilenceReoccupHud.c
// fait de meme sur trois HUD de Reoccupation). La classe est citee dans un .et, pas dans un
// .conf -- ce n'est donc PAS le piege de JWK_MainMenuContext, cf. FFRX_TrimMainMenu.c.
//
// Client uniquement (c'est un HUD). Chaines ASCII.
// ------------------------------------------------------------------------------------

modded class JWK_CallToActionInfo
{
	//! Cle de localisation de l'invitation a construire, telle qu'appelee par UpdateTrigger.
	protected static const string FFRX_CTA_CONSTRUCTION = "#JWK-CTA-ConstructionMenu";

	override protected void ShowCTA(string text)
	{
		if (text == FFRX_CTA_CONSTRUCTION)
		{
			string squad;
			if (!FFRX_BuildDirect.InEngineerSquad(squad))
			{
				if (m_wCTA)
					m_wCTA.SetVisible(false);

				return;
			}
		}

		super.ShowCTA(text);
	}
}

// ------------------------------------------------------------------------------------
//  AMORCAGE : voir FFRX_IntroCinematic.c, modded SCR_PlayerController.OnControlledEntityChanged
//
//  Il n'y a PAS de GameSystem ici, et c'est volontaire. La premiere version en utilisait un
//  (WorldSystemLocation.Client) : il ne s'est jamais amorce chez un joueur connecte au
//  serveur dedie. Verifie le 2026-09-17 -- le fichier etait bien dans le pak 1.0.66, mais
//  la ligne "[FFRX][Build] touche J branchee" n'est jamais apparue dans le log client,
//  pas plus que celle de FFRX_MainMenuTrimSystem (meme construction).
//
//  L'amorcage se fait donc depuis OnControlledEntityChanged du PlayerController local, un
//  chemin qui imprime de facon fiable dans ce meme log. Arm() est idempotent, donc etre
//  rappele a chaque respawn ne pose aucun probleme.
// ------------------------------------------------------------------------------------
