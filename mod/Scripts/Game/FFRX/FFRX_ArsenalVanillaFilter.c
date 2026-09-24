// FF - REMIXED - PVE
// Recherche + tri par categorie sur la caisse ARSENAL VANILLA (celle qu'on pose depuis
// le Game Master), et non sur la boutique FF.
//
// ------------------------------------------------------------------------------------
// POURQUOI CE FICHIER EXISTE
//
// On avait deja un tri par categorie (FFRX_ShopCategories), mais il est branche sur
// JWK_ShopInterfaceUIComponent -- l'interface de boutique de Freedom Fighters. La caisse
// arsenal vanilla utilise une interface TOTALEMENT differente : l'inventaire du jeu de
// base (SCR_InventoryOpenedStorageArsenalUI), qui n'a AUCUN filtrage. Rien n'etait donc
// reutilisable cote UI ; seule la classification des objets l'est, et on la reprend
// telle quelle (FFRX_ArsenalCategory).
//
// Choix de la caisse vanilla (decision Benji, 2026-09-10) : on peut y REMETTRE un objet,
// ce que la boutique FF ne permet pas, et les batiments arsenal constructibles ont ete
// desactives.
//
// ------------------------------------------------------------------------------------
// CREDITS -- WCS_LoadoutEditor (Workshop 61D57616CAFBB23D), par l'equipe WCS.
//
// Le mod n'est PAS une dependance : rien de son code n'est repris, et il ne nous apporte
// aucun asset. Mais c'est en lisant sa source qu'on a compris comment on injecte
// proprement de l'UI dans le menu d'inventaire du jeu de base, apres plusieurs tentatives
// ratees de notre cote. Deux enseignements qui viennent de lui :
//   1. NE PAS empiler des widgets bruts sous un parent quelconque -- il faut soit un slot
//      qui les dimensionne, soit un `.layout` charge dans un conteneur NOMME du layout
//      vanilla (`CreateWidgets(layout, parent)`), ce qu'ils font systematiquement ;
//   2. le filtrage par categorie se fait cote DONNEES, en surchargeant la recuperation
//      des items -- ce qu'on faisait deja, et que leur code confirme comme la bonne voie.
// Le merite de ces deux constats leur revient ; le code ci-dessous est le notre.
//
// ------------------------------------------------------------------------------------
// OU ON SE BRANCHE
//
// `GetAllItems` construit la liste des prefabs affiches dans la grille -- c'est LA
// source de verite de l'affichage. Filtrer la, plutot que de masquer des cases apres
// coup, garde la pagination coherente : sans ca on obtiendrait des pages a moitie vides
// et un compteur "1/15" qui mentirait.
//
// ------------------------------------------------------------------------------------
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

//! Etat du filtre, partage entre l'UI et le filtrage. Statique parce que l'UI d'inventaire
//! est recreee a chaque ouverture : sans ca le filtre serait perdu a chaque page tournee.
class FFRX_ArsenalFilterState
{
	static int    s_iCategoryMask;   // 0 = tout
	static string s_sSearch;         // minuscules, "" = pas de recherche

	static void Reset()
	{
		s_iCategoryMask = 0;
		s_sSearch = "";
	}

	//! Vrai si le prefab passe le filtre courant (categorie ET recherche).
	static bool Passes(ResourceName prefab)
	{
		if (s_iCategoryMask != 0 && !FFRX_ArsenalCategory.Matches(prefab, s_iCategoryMask))
			return false;

		if (s_sSearch != "")
		{
			string name = FFRX_ShortName(prefab);
			name.ToLower();
			if (!name.Contains(s_sSearch))
				return false;
		}
		return true;
	}

	//! "{GUID}Prefabs/.../HK416F-S_AimM5.et" -> "HK416F-S_AimM5"
	//! On cherche sur le nom de fichier : c'est ce que le joueur reconnait, et c'est
	//! disponible sans resoudre le prefab (donc sans cout).
	static string FFRX_ShortName(string rn)
	{
		string s = rn;
		int b = s.IndexOf("}");
		if (b >= 0 && b + 1 < s.Length()) s = s.Substring(b + 1, s.Length() - b - 1);
		int slash = s.LastIndexOf("/");
		if (slash >= 0 && slash + 1 < s.Length()) s = s.Substring(slash + 1, s.Length() - slash - 1);
		int dot = s.LastIndexOf(".");
		if (dot > 0) s = s.Substring(0, dot);
		return s;
	}
}

// ------------------------------------------------------------------------------------
//  Le filtrage lui-meme
// ------------------------------------------------------------------------------------
modded class SCR_InventoryOpenedStorageArsenalUI
{
	override protected void GetAllItems(out notnull array<IEntity> pItemsInStorage, BaseInventoryStorageComponent pStorage = null)
	{
		// pStorage renseigne = ce n'est PAS la grille de l'arsenal (c'est un autre
		// stockage, l'inventaire du joueur par exemple). On ne filtre que l'arsenal.
		if (pStorage)
		{
			super.GetAllItems(pItemsInStorage, pStorage);
			return;
		}

		// Aucun filtre actif -> comportement d'origine, cout nul.
		if (FFRX_ArsenalFilterState.s_iCategoryMask == 0 && FFRX_ArsenalFilterState.s_sSearch == "")
		{
			super.GetAllItems(pItemsInStorage, pStorage);
			return;
		}

		// On refait le travail de la methode d'origine en ecartant ce qui ne passe pas.
		// (On ne peut pas filtrer APRES super() : elle rend des entites de previsualisation
		// deja resolues, sans moyen fiable de remonter au prefab d'origine.)
		if (!m_Storage || !m_Storage.GetOwner())
			return;

		SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(m_Storage.GetOwner().FindComponent(SCR_ArsenalComponent));
		if (!arsenal)
			return;

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return;
		ItemPreviewManagerEntity previews = world.GetItemPreviewManager();
		if (!previews)
			return;

		array<ResourceName> prefabs = {};
		arsenal.GetAvailablePrefabs(prefabs);

		foreach (ResourceName rn : prefabs)
		{
			if (!FFRX_ArsenalFilterState.Passes(rn))
				continue;
			pItemsInStorage.Insert(previews.ResolvePreviewEntityForPrefab(rn));
		}
	}

	//! A l'ouverture de la caisse, on repart d'une grille complete : garder le filtre
	//! d'une session precedente ferait croire a un arsenal vide ou incomplet.
	override void Init()
	{
		FFRX_ArsenalFilterState.Reset();
		super.Init();
		FFRX_BuildFilterBar();
	}

	// --------------------------------------------------------------------------------
	//  Barre de categories + champ de recherche, construits a la main
	// --------------------------------------------------------------------------------
	//! Position de la barre dans le frame d'ancrage. A ajuster si elle recouvre un
	//! element du menu : c'est le seul reglage a toucher pour la deplacer.
	protected static const float FFRX_BAR_X = 40;
	protected static const float FFRX_BAR_Y = 120;

	protected ref array<ref FFRX_ArsenalCatBtnHandler> m_aFFRXHandlers;
	protected ref FFRX_ArsenalSearchHandler m_FFRXSearchHandler;   // sinon GC immediat
	protected EditBoxWidget m_wFFRXSearch;

	//! Traduit une taille ecran en verdict lisible : c'est la reponse a "les widgets
	//! sont-ils crees mais invisibles ?" -- une barre a 0 de haut existe et ne se voit pas.
	protected string FFRX_SizeVerdict(float w, float h)
	{
		if (w <= 1 || h <= 1)
			return "-> EFFONDREE (invisible malgre SetVisible)";
		return "-> OK";
	}

	protected void FFRX_BuildFilterBar()
	{
		// Traces de diagnostic : la barre ne s'affichait pas et il fallait savoir OU ca
		// echoue -- methode jamais appelee, widget racine absent, ou widgets crees mais
		// invisibles (taille nulle). A retirer une fois la barre en place.
		Print("[FFRX][ArsenalUI] BuildFilterBar appelee.", LogLevel.NORMAL);

		if (!m_widget)
		{
			Print("[FFRX][ArsenalUI] ECHEC : m_widget est nul.", LogLevel.WARNING);
			return;
		}
		// Le TYPE du parent decide de la facon de dimensionner l'enfant : sous un
		// FrameWidget il faut passer par FrameSlot.SetSize, sous un layout vertical /
		// horizontal c'est le parent qui repartit, sous autre chose il faut une taille
		// explicite. Tant qu'on ne le connait pas, tout dimensionnement est une devinette
		// -- et c'est exactement pour ca que la barre est restee invisible.
		Print(string.Format("[FFRX][ArsenalUI] widget racine = '%1' | type = %2",
			m_widget.GetName(), m_widget.Type().ToString()), LogLevel.NORMAL);

		WorkspaceWidget ws = GetGame().GetWorkspace();
		if (!ws)
		{
			Print("[FFRX][ArsenalUI] ECHEC : pas de workspace.", LogLevel.WARNING);
			return;
		}

		// ------------------------------------------------------------------------------
		// OU ACCROCHER LA BARRE -- c'est TOUT le sujet
		//
		// Version precedente : widgets bruts crees sous `m_widget`, sans slot ni taille.
		// Resultat : effondrement a 0x0. Le widget existe, il se dit visible, et on ne voit
		// rien -- on a cherche un bug de logique alors qu'il ne s'agissait que de geometrie.
		// (Le commentaire d'alors annoncait meme une "taille explicite" jamais posee.)
		//
		// On reprend la recette qui rend deja correctement chez nous
		// (FFRX_ShopCategories.FFRX_BuildCategoryBar) : ancrer sur un FrameWidget, puis
		// positionner via les helpers FrameSlot.
		//
		// UNE DIFFERENCE VOULUE avec la version du shop : celle-ci cherche son frame par NOM
		// ("rootFrame"), et cette approche s'est deja retournee contre nous -- un
		// FindAnyWidget("Frame0") avait matche une sous-carte de la grille et la barre avait
		// disparu. Ici on remonte jusqu'au premier ancetre qui EST un FrameWidget : aucun nom
		// a deviner, et rien ne casse quand le layout du jeu de base evolue.
		Widget anchor = m_widget;
		while (anchor && !FrameWidget.Cast(anchor))
			anchor = anchor.GetParent();

		if (!anchor)
		{
			Print("[FFRX][ArsenalUI] ECHEC : aucun FrameWidget ancetre -- rien ou ancrer.", LogLevel.WARNING);
			return;
		}
		Print(string.Format("[FFRX][ArsenalUI] ancrage sur le frame '%1'", anchor.GetName()), LogLevel.NORMAL);

		VerticalLayoutWidget root = VerticalLayoutWidget.Cast(ws.CreateWidget(
			WidgetType.VerticalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.FromInt(0x00000000), 0, anchor));
		if (!root)
			return;
		root.SetName("FFRX_FilterBar");
		root.SetVisible(true);

		// Les helpers de placement sont STATIQUES sur la classe de slot : `Widget` n'expose
		// ni SetSize ni SetAnchorMin/Max ("Undefined function", verifie au compilateur).
		// SetSizeToContent : la barre se dimensionne sur ses boutons, plutot qu'une largeur
		// devinee qui deviendrait fausse des qu'on change un libelle.
		FrameSlot.SetAnchorMin(root, 0, 0);
		FrameSlot.SetAnchorMax(root, 0, 0);
		FrameSlot.SetSizeToContent(root, true);
		FrameSlot.SetPos(root, FFRX_BAR_X, FFRX_BAR_Y);

		float rw, rh;
		root.GetScreenSize(rw, rh);
		Print(string.Format("[FFRX][ArsenalUI] barre creee | taille ecran = %1 x %2 %3",
			rw, rh, FFRX_SizeVerdict(rw, rh)), LogLevel.NORMAL);

		// --- champ de recherche ---
		EditBoxWidget search = EditBoxWidget.Cast(ws.CreateWidget(WidgetType.EditBoxWidgetTypeID, WidgetFlags.VISIBLE, Color.White, 0, root));
		if (search)
		{
			search.SetName("FFRX_Search");
			// PAS de SetPlaceholderText : cette methode appartient a SCR_EditBoxComponent,
			// pas au widget brut EditBoxWidget qu'on cree ici.
			m_wFFRXSearch = search;
			FFRX_ArsenalSearchHandler sh = new FFRX_ArsenalSearchHandler();
			sh.m_UI = this;
			m_FFRXSearchHandler = sh;
			search.AddHandler(sh);
		}

		// --- boutons de categorie ---
		HorizontalLayoutWidget row = HorizontalLayoutWidget.Cast(ws.CreateWidget(WidgetType.HorizontalLayoutWidgetTypeID, WidgetFlags.VISIBLE, Color.White, 0, root));
		if (!row)
			return;

		m_aFFRXHandlers = {};
		array<ref FFRX_CategoryDef> cats = FFRX_ArsenalCategory.GetCategories();
		foreach (FFRX_CategoryDef cat : cats)
		{
			if (!cat) continue;
			ButtonWidget b = ButtonWidget.Cast(ws.CreateWidget(WidgetType.ButtonWidgetTypeID, WidgetFlags.VISIBLE, Color.White, 0, row));
			if (!b) continue;

			TextWidget t = TextWidget.Cast(ws.CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE, Color.White, 0, b));
			if (t) t.SetText(cat.m_sLabel);

			FFRX_ArsenalCatBtnHandler h = new FFRX_ArsenalCatBtnHandler();
			h.m_iMask = cat.m_iMask;
			h.m_UI = this;
			b.AddHandler(h);
			m_aFFRXHandlers.Insert(h);   // garde les handlers en vie (sinon GC immediat)
		}
		Print(string.Format("[FFRX][ArsenalUI] %1 boutons de categorie crees.", m_aFFRXHandlers.Count()), LogLevel.NORMAL);
	}

	//! Reconstruit la grille avec le filtre courant.
	void FFRX_ApplyFilter()
	{
		// On relit le texte a la source plutot que de se fier a un evenement : la
		// signature exacte de OnChange n'est pas garantie d'une version a l'autre,
		// alors que GetText() l'est.
		if (m_wFFRXSearch)
		{
			string s = m_wFFRXSearch.GetText();
			s.ToLower();
			FFRX_ArsenalFilterState.s_sSearch = s;
		}
		Refresh();
	}
}

// ------------------------------------------------------------------------------------
//  Handlers
// ------------------------------------------------------------------------------------
class FFRX_ArsenalCatBtnHandler : ScriptedWidgetEventHandler
{
	int m_iMask;
	SCR_InventoryOpenedStorageArsenalUI m_UI;

	override bool OnClick(Widget w, int x, int y, int button)
	{
		FFRX_ArsenalFilterState.s_iCategoryMask = m_iMask;
		if (m_UI) m_UI.FFRX_ApplyFilter();
		return true;
	}
}

class FFRX_ArsenalSearchHandler : ScriptedWidgetEventHandler
{
	SCR_InventoryOpenedStorageArsenalUI m_UI;

	// Signature a DEUX arguments : c'est celle utilisee par le jeu de base
	// (cf. SCR_BaseEditorAttributeUIComponent). La variante a 4 arguments ne compile pas
	// et produit une erreur illisible ("error: <mojibake>") sur la ligne de declaration.
	override bool OnChange(Widget w, bool finished)
	{
		if (m_UI) m_UI.FFRX_ApplyFilter();
		return true;
	}
}
