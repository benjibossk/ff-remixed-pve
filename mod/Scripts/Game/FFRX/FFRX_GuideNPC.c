// FF - REMIXED - PVE
// PNJ GUIDE -- un officier pres du drapeau, qui fait le briefing d'accueil SUR DEMANDE.
//
// ======================================================================================
//  D'OU IL VIENT
// ======================================================================================
// Il derive de l'instructeur du TUTORIEL du jeu de base
// (`{5F45655033ECBFF7}Prefabs/Characters/Tutorial/Instructors/Main_Instructor.et`), qui sait
// deja se tourner vers le joueur et jouer une animation de discours -- tout le travail
// d'animation est fait, on n'ecrit rien de tout ca.
//
// ⚠️ LE RISQUE ETAIT LA DEPENDANCE AU MODE TUTORIEL. Il est leve : `SCR_NarrativeComponent`
// n'interroge le tutoriel qu'a un seul endroit, `IsActualCourseFreeRoam()`, et cette
// fonction **rend `true` quand le composant de tutoriel est absent**. Hors tutoriel, le PNJ
// se comporte donc comme en mode libre. Et `Main_Instructor.et` porte deja
// `SCR_TutorialInstructorComponent { Enabled 0 }`.
//
// ======================================================================================
//  UNE ACTION, PAS LA PROXIMITE (decision Benji, 22/09)
// ======================================================================================
// Le prefab d'origine parle des qu'on s'approche (`m_bProximityTalk 1`). Sur une base ou
// tout le monde passe devant le drapeau vingt fois par session, ce serait insupportable :
// le briefing se declencherait tout seul, en boucle, au milieu d'autre chose.
//
// On coupe donc la proximite et on expose une ACTION explicite -- le joueur decide. Elle
// reste disponible a volonte : un briefing qu'on ne peut ecouter qu'une fois est un briefing
// qu'on rate.
//
// ======================================================================================
//  LA TENUE
// ======================================================================================
// Beret ROUGE + veste F3 en CE + pantalon CE + rangers. Le GALON DE COLONEL (grade 17, le
// plus haut de l'echelle FFRX) est pose PAR SCRIPT a l'apparition, et pas dans le prefab :
// il va dans un slot velcro NOMME de la veste (`Torso_Rectangle_Velcro`), et seul du code
// sait retrouver ce slot par son nom. C'est exactement ce que fait deja FFRX_RankPatch.c
// pour les joueurs -- on reutilise sa table plutot que de recopier un GUID de galon.
//
// ======================================================================================
//  OU IL APPARAIT
// ======================================================================================
// A cote du drapeau de la base de depart. On ne code pas une position en dur : on demande
// sa position a la FOB posee par FFRX_DefaultFob, et on se decale de quelques metres.
// Une position en dur casserait au premier changement de carte ou de placement de FOB.
//
// Serveur uniquement (le PNJ est une entite repliquee). Chaines ASCII.

class FFRX_GuideNPC
{
	//! Le prefab du guide. Importe dans le Workbench le 22/09.
	static const ResourceName NPC_PREFAB =
		"{2A26C4E3D2007B2D}Prefabs/Characters/FFRX_GuideNPC.et";

	//! Grade affiche : 17 = Colonel, le sommet de l'echelle (cf. FFRX_RankPatchTable).
	static const int GUIDE_RANK = 17;

	//! Distance a laquelle il se place du drapeau.
	static const float OFFSET_M = 3.5;

	//! On laisse la FOB se poser avant de chercher son drapeau (FFRX_DefaultFob reessaie
	//! plusieurs fois au demarrage).
	static const int BOOT_DELAY_MS = 60000;

	protected static IEntity s_Npc;

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(TrySpawn, BOOT_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void TrySpawn()
	{
		if (s_Npc && !s_Npc.IsDeleted())
			return;

		vector flag;
		if (!FindFlag(flag))
		{
			// La FOB n'est pas encore posee : on repasse plus tard plutot que d'abandonner.
			Print("[FFRX][Guide] Drapeau introuvable, nouvelle tentative dans 60 s.", LogLevel.NORMAL);
			GetGame().GetCallqueue().CallLater(TrySpawn, BOOT_DELAY_MS, false);
			return;
		}

		float ang = Math.RandomFloat(0, Math.PI2);

		vector p = flag;
		p[0] = p[0] + Math.Cos(ang) * OFFSET_M;
		p[2] = p[2] + Math.Sin(ang) * OFFSET_M;

		vector pos = p;
		if (!SCR_WorldTools.FindEmptyTerrainPosition(pos, p, 8))
			pos = p;

		Resource res = Resource.Load(NPC_PREFAB);
		if (!res || !res.IsValid())
		{
			Print("[FFRX][Guide] Prefab introuvable -- import fait ? GUID a jour ?", LogLevel.WARNING);
			return;
		}

		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(sp.Transform);
		sp.Transform[3] = pos;

		s_Npc = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		if (!s_Npc)
		{
			Print("[FFRX][Guide] Echec du spawn.", LogLevel.WARNING);
			return;
		}

		// Le galon ne peut pas vivre dans le prefab : il se pose dans un slot velcro nomme.
		// On laisse une seconde a la tenue pour etre reellement equipee.
		GetGame().GetCallqueue().CallLater(PinRank, 1000, false);

		Print(string.Format("[FFRX][Guide] Officier place pres du drapeau, en %1.", pos), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Position du drapeau de la base de depart.
	protected static bool FindFlag(out vector outPos)
	{
		vector fob;
		if (!FFRX_DefaultFob.FFRX_GetFobPos(fob))
			return false;

		outPos = fob;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose le galon de colonel sur la veste, en reutilisant la table des grades des joueurs.
	protected static void PinRank()
	{
		if (!s_Npc)
			return;

		EquipedLoadoutStorageComponent loadout = EquipedLoadoutStorageComponent.Cast(
			s_Npc.FindComponent(EquipedLoadoutStorageComponent));
		if (!loadout)
			return;

		IEntity jacket = loadout.GetClothFromArea(LoadoutJacketArea);
		if (!jacket)
			return;

		// Pas de gilet de combat sur lui -> galon HAUTE VISIBILITE, le galon de parade.
		FFRX_RankPatch.FFRX_PinOn(s_Npc, jacket, FFRX_RankPatchTable.VestHV(GUIDE_RANK));
	}

	//------------------------------------------------------------------------------------------------
	//! Declenche le briefing. Appele par l'action, cote serveur.
	static void Brief(IEntity npc)
	{
		if (!npc)
			return;

		SCR_NarrativeComponent nar = SCR_NarrativeComponent.Cast(npc.FindComponent(SCR_NarrativeComponent));
		if (!nar)
			return;

		nar.PlayAnimation(true, "CMD_Narrative", 0);
	}
}

// ---------------------------------------------------------------------------
//  L'action : « Ecouter le briefing »
// ---------------------------------------------------------------------------
class FFRX_GuideBriefingAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Ecouter le briefing de l'officier";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! ⚠️ On ne gate RIEN sur un etat client ici. Une ScriptedUserAction dont
	//! `CanBePerformedScript` depend d'un etat local marche en Workbench et ne fait RIEN sur
	//! le dedie : l'autorite revalide avec un registre vide (memoire
	//! `dedie-useraction-canperform-client-gate`, bug deja paye sur "Prendre dotation").
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Tout ce qui compte se decide cote serveur.
		if (!Replication.IsServer())
			return;

		FFRX_GuideNPC.Brief(pOwnerEntity);
	}
}
