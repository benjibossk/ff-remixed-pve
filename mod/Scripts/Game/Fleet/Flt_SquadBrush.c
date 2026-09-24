// ============================================================================
//  Flt_SquadBrush — couleur PAR DÉFAUT des marqueurs/dessins Anarchy = couleur
//  de l'escouade du joueur.
//
//  Dans Anarchy, marqueurs ET dessins prennent leur couleur du "pinceau"
//  (SM_DrawCanvas.s_iColor) : le dessin l'utilise tel quel, le marqueur via
//  SM_PresetBrushColor (qui snappe à la couleur de palette la plus proche).
//  => On règle le pinceau sur la couleur d'escouade quand la carte s'ouvre,
//     tant que le joueur n'a pas choisi une couleur lui-même.
//
//  Client-side (l'UI carte est locale). Couleur = palette par GroupID (répliqué)
//  -> cohérente avec ce que Fleet envoie au web. Les couleurs forcées (preset)
//  ne sont pas répliquées au client : en host/solo OK, sinon la palette prime.
// ============================================================================
modded class SM_DrawCanvas
{
	protected static bool s_bFltUserPicked;	// le joueur a choisi une couleur -> on ne force plus

	//------------------------------------------------------------------------------------------------
	// Clic joueur sur une couleur de la palette (SM_DrawPanel) -> on respecte son choix.
	override void SetColor(int argb)
	{
		s_bFltUserPicked = true;
		super.SetColor(argb);
	}

	//------------------------------------------------------------------------------------------------
	override void Init(notnull CanvasWidget canvas, notnull SCR_MapEntity mapEnt, Widget mapFrame, bool editorMap = false)
	{
		super.Init(canvas, mapEnt, mapFrame, editorMap);
		Flt_ApplySquadBrush();
	}

	//------------------------------------------------------------------------------------------------
	protected void Flt_ApplySquadBrush()
	{
		if (s_bFltUserPicked)
			return;	// choix manuel déjà fait cette session

		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return;
		int pid = pc.GetPlayerId();
		if (pid <= 0)
			return;

		SCR_GroupsManagerComponent gm = SCR_GroupsManagerComponent.GetInstance();
		if (!gm)
			return;
		SCR_AIGroup group = gm.GetPlayerGroup(pid);
		if (!group)
			return;

		string name = SCR_GroupHelperUI.GetTranslatedGroupName(group);
		int c = Flt_SquadColors.Resolve(group, name);
		if (c != 0)
			s_iColor = c;	// pinceau = couleur escouade -> défaut markers + dessins
	}
}
