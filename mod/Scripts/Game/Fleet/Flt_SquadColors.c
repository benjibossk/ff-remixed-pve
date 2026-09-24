// ============================================================================
//  Flt_SquadColors — couleur d'escouade (pour colorer les marqueurs joueurs
//  côté web par escouade).
//
//  Priorité de résolution (Flt_SquadColors.Resolve) :
//    1. Couleur FORCÉE posée sur le groupe (via un group preset -> voir modded
//       SCR_GroupPreset ci-dessous). Sert pour tes groupes prédéfinis.
//    2. Table nom→couleur $profile:Fleet/SquadColors.json. Sert pour les
//       escouades AUTO-créées (Alpha, Bravo…) qui n'ont pas de preset.
//    3. Palette AUTO (par GroupID) — chaque escouade a une couleur distincte.
//
//  Couleur = int ARGB (même convention que la couleur des marqueurs).
//  Lu côté serveur uniquement (dans Flt_GTGPositions.SendSquads).
// ============================================================================

// --- Champ couleur forcée porté par le groupe runtime -----------------------
modded class SCR_AIGroup
{
	protected int m_iFltForcedColor = 0;	// 0 = aucune (auto)

	void Flt_SetForcedColor(int argb) { m_iFltForcedColor = argb; }
	int  Flt_GetForcedColor()          { return m_iFltForcedColor; }
}

// --- Ajout d'une couleur au group preset ------------------------------------
// Coche "Forcer la couleur" et choisis la couleur : elle sera posée sur le
// groupe créé depuis ce preset (SetupGroup) et remontée au web.
modded class SCR_GroupPreset
{
	[Attribute("0", UIWidgets.CheckBox, "Fleet : forcer une couleur d'escouade (sinon auto).")]
	protected bool m_bFltUseForcedColor;

	[Attribute("1 0 0 1", UIWidgets.ColorPicker, "Fleet : couleur forcée de l'escouade.")]
	protected ref Color m_FltForcedColor;

	override void SetupGroup(SCR_AIGroup group)
	{
		super.SetupGroup(group);
		if (m_bFltUseForcedColor && m_FltForcedColor)
			group.Flt_SetForcedColor(m_FltForcedColor.PackToInt());
	}
}

// --- Indicateur couleur dans le "Menu du groupe" (UI, client) ---------------
// Colore le nom de chaque escouade avec sa couleur (même résolution qu'au web).
// NB: la palette auto (par GroupID répliqué) matche le web partout ; les couleurs
// forcées (preset/table serveur) ne matchent qu'en host/solo.
modded class SCR_GroupButton
{
	override void SetGroup(notnull SCR_AIGroup group)
	{
		super.SetGroup(group);
		Flt_ApplySquadColor();
	}

	override void UpdateGroup(bool canJoin = true)
	{
		super.UpdateGroup(canJoin);
		Flt_ApplySquadColor();
	}

	// Point le + fiable : le jeu pose le texte du nom ici -> on teinte juste APRÈS
	// (sinon le menu P écrase notre couleur en re-settant le texte).
	override void UpdateGroupName()
	{
		super.UpdateGroupName();
		Flt_ApplySquadColor();
	}


	protected void Flt_ApplySquadColor()
	{
		if (!m_Group)
			return;

		// même nom complet que Fleet côté serveur (cohérence table nom->couleur)
		string name = SCR_GroupHelperUI.GetTranslatedGroupName(m_Group);

		int argb = Flt_SquadColors.Resolve(m_Group, name);
		if (argb == 0)
			return;

		Color col = Color.FromInt(argb);
		if (m_wGroupName)
			m_wGroupName.SetColor(col);

		// Teinte l'IMAGE du drapeau (via SCR_GroupFlagImageComponent.GetImageWidget) : ça survit
		// à l'état du bouton dans le menu P, contrairement à la couleur du texte.
		if (m_wGroupFlag)
		{
			SCR_GroupFlagImageComponent fc = SCR_GroupFlagImageComponent.Cast(m_wGroupFlag.FindHandler(SCR_GroupFlagImageComponent));
			if (fc)
			{
				ImageWidget img = fc.GetImageWidget();
				if (img)
					img.SetColor(col);
			}
		}
	}
}

// --- Libellé de groupe dans le PANNEAU DE RESPAWN / sélecteur (m_wExpandButtonName) ----------
// Ce libellé est posé par SGetGroupName (statique, non-moddable) mais appelé depuis
// SetPlayerGroup / UpdateGroupNames -> on teinte APRÈS via ces méthodes.
modded class SCR_GroupRequestUIComponent
{
	override void SetPlayerGroup(SCR_AIGroup group)
	{
		super.SetPlayerGroup(group);
		Flt_ColorExpandName(group);
	}

	override protected void UpdateGroupNames()
	{
		super.UpdateGroupNames();
		Flt_ColorExpandName(GetPlayerGroup());
	}

	protected void Flt_ColorExpandName(SCR_AIGroup group)
	{
		if (!m_wExpandButtonName || !group)
			return;
		int argb = Flt_SquadColors.Resolve(group, SCR_GroupHelperUI.GetTranslatedGroupName(group));
		if (argb != 0)
			m_wExpandButtonName.SetColor(Color.FromInt(argb));
	}
}

// --- Menu du groupe (touche P) : lignes = SCR_GroupTileButton (PAS SCR_GroupButton) ----------
// Le nom = RichText "Callsign", le drapeau = Image "GroupImage". On teinte après InitiateGroupTile.
modded class SCR_GroupTileButton
{
	override void InitiateGroupTile()
	{
		super.InitiateGroupTile();
		Flt_TintTile();
	}

	protected void Flt_TintTile()
	{
		if (!m_GroupManager)
			return;
		SCR_AIGroup group = m_GroupManager.FindGroup(m_iGroupID);
		if (!group)
			return;
		int argb = Flt_SquadColors.Resolve(group, SCR_GroupHelperUI.GetTranslatedGroupName(group));
		if (argb == 0)
			return;

		Color col = Color.FromInt(argb);
		Widget root = GetRootWidget();
		if (!root)
			return;

		RichTextWidget name = RichTextWidget.Cast(root.FindAnyWidget("Callsign"));
		if (name)
			name.SetColor(col);
		ImageWidget flag = ImageWidget.Cast(root.FindAnyWidget("GroupImage"));
		if (flag)
			flag.SetColor(col);
	}
}

// --- Table nom -> couleur (override pour escouades auto-créées) --------------
class Flt_SquadColorEntry
{
	string name;	// nom / callsign de l'escouade (ex. "Alpha")
	int    color;	// ARGB
}

class Flt_SquadColorStore
{
	ref array<ref Flt_SquadColorEntry> entries = {};
}

class Flt_SquadColorTable
{
	protected static ref Flt_SquadColorTable s_Instance;
	const string FILE = "$profile:Fleet/SquadColors.json";
	protected ref map<string, int> m_mByName = new map<string, int>();

	static Flt_SquadColorTable GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new Flt_SquadColorTable();
			s_Instance.Load();
		}
		return s_Instance;
	}

	void Load()
	{
		m_mByName.Clear();
		if (!FileIO.FileExists("$profile:Fleet/"))
			FileIO.MakeDirectory("$profile:Fleet/");

		// Premier lancement : on crée un fichier vide-exemple pour montrer le format.
		if (!FileIO.FileExists(FILE))
		{
			Flt_SquadColorStore seed = new Flt_SquadColorStore();
			SCR_JsonSaveContext sctx = new SCR_JsonSaveContext();
			sctx.WriteValue("", seed);
			sctx.SaveToFile(FILE);
			Print("[SQUADCOL] Fichier créé (vide) : " + FILE, LogLevel.NORMAL);
			return;
		}

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		Flt_SquadColorStore store = new Flt_SquadColorStore();
		if (ctx.LoadFromFile(FILE) && ctx.ReadValue("", store))
		{
			foreach (Flt_SquadColorEntry e : store.entries)
			{
				if (e && e.name != "")
					m_mByName.Set(e.name, e.color);
			}
			Print(string.Format("[SQUADCOL] %1 couleur(s) forcée(s) chargée(s)", m_mByName.Count()), LogLevel.NORMAL);
		}
	}

	//! \return couleur ARGB pour ce nom, ou 0 si absent
	int GetByName(string name)
	{
		if (name == "")
			return 0;
		int c;
		if (m_mByName.Find(name, c))
			return c;
		return 0;
	}
}

// --- Résolution + palette auto ----------------------------------------------
class Flt_SquadColors
{
	protected static ref array<int> s_Palette;

	// Un ARGB packé depuis r,g,b (0-255). Variable temporaire obligatoire :
	// on ne peut PAS chaîner .PackToInt() directement sur Color.FromRGBA(...).
	protected static int RGB(int r, int g, int b)
	{
		Color c = Color.FromRGBA(r, g, b, 255);
		return c.PackToInt();
	}

	// Palette = couleurs VISIBLES de la palette marqueur Anarchy/vanilla (MapMarkerConfig.conf).
	// Ainsi la couleur d'escouade EST une couleur de palette -> le marqueur snappe pile dessus
	// (couleur exacte), le dessin l'utilise tel quel, et le web reste cohérent.
	protected static array<int> Palette()
	{
		if (!s_Palette)
		{
			s_Palette = {};
			s_Palette.Insert(RGB(255,   0, 236));	// magenta
			s_Palette.Insert(RGB(242, 166,  35));	// orange / or
			s_Palette.Insert(RGB(194, 100,  20));	// brun
			s_Palette.Insert(RGB(218,   7,   7));	// rouge
			s_Palette.Insert(RGB(  4, 113,  20));	// vert
			s_Palette.Insert(RGB(  1,  50, 216));	// bleu
			s_Palette.Insert(RGB(  4, 139, 228));	// bleu clair
			s_Palette.Insert(RGB(222,  10, 180));	// rose
			s_Palette.Insert(RGB( 72,  21,  90));	// violet foncé
		}
		return s_Palette;
	}

	//! Couleur ARGB à envoyer au web pour cette escouade.
	//! \param[in] g     groupe runtime
	//! \param[in] name  nom affiché de l'escouade (pour la table nom->couleur)
	static int Resolve(SCR_AIGroup g, string name)
	{
		if (!g)
			return 0;

		// 1. couleur forcée sur le groupe (via preset)
		int forced = g.Flt_GetForcedColor();
		if (forced != 0)
			return forced;

		// 2. table nom -> couleur (override des escouades auto-créées)
		int fromTable = Flt_SquadColorTable.GetInstance().GetByName(name);
		if (fromTable != 0)
			return fromTable;

		// 3. palette auto par GroupID
		array<int> pal = Palette();
		if (pal.IsEmpty())
			return 0;
		int id = g.GetGroupID();
		if (id < 0)
			id = -id;
		return pal[id % pal.Count()];
	}
}
