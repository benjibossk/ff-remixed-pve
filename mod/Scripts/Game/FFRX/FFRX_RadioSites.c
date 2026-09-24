// FF - REMIXED - PVE
// CONTROLEURS DE SITE RADIO SUR ANIZAY -- ce qui transforme les antennes en objectifs.
//
// ======================================================================================
//  LE CONSTAT (2026-09-23)
// ======================================================================================
// Anizay ne contenait AUCUNE source de couverture radio : ni `TransmitterTower`, ni
// `JWK_RadioSiteEntity`. Verifie dans les layers de la carte ET dans le log du dedie (zero
// ligne "RadioSite", alors qu'on y voit bien les 9 sites RADAR).
//
// Consequences, toutes silencieuses -- rien n'echouait, ca ne faisait simplement rien :
//   - `NearestFriendlySiteDistance()` rendait -1 en permanence, donc notre signal (livemap,
//     balises, sac radio) etait fige en "hors de portee" ;
//   - les DEPECHES RADIO de Reoccupation, qui exigent d'aller sur un site radio TENU, sont
//     inertes depuis le premier jour sur cette carte.
//
// ======================================================================================
//  LES DEUX MOITIES D'UN SITE RADIO
// ======================================================================================
// FF en demande deux, et il faut les DEUX :
//   1. une TOUR physique portant `JWK_RadioTowerComponent` ;
//   2. un CONTROLEUR `JWK_RadioSiteEntity` a moins de `m_iTowerLinkRange` (30 m), qui porte
//      la capture, la garnison, l'icone de carte et la portee de 2500 m.
//
// La moitie 1 est reglee par les overrides `Prefabs/Models/houses/tem_antenna1|2.et` : on a
// repris les prefabs d'Anizay a l'identique (58 lignes, tout le bloc de destruction
// conserve) en y ajoutant le composant FF. Ce fichier-ci s'occupe de la moitie 2.
//
// ⚠️ On n'a PAS touche aux tours de controle d'aerodrome du jeu de base : leur prefab fait
// 693 lignes et 27 composants, et un override au meme GUID REMPLACE le fichier -- tout
// composant oublie disparait sans erreur. Les antennes d'Anizay, elles, sont assez petites
// pour que l'exercice soit sain, et c'est deja le motif employe pour les maisons.
//
// ======================================================================================
//  POURQUOI ON REJOUE L'AMORCAGE DE FF
// ======================================================================================
// FF cable ses sites en DEUX passes (JWK_RadioTowerManagerComponent) :
//   1. `OnWorldPostProcess` -> `LinkTowers_S()` : le site cherche les tours a moins de 30 m
//      et devient OPERABLE s'il en trouve ;
//   2. `PostGameModeStartDelayed_S` -> `OnPostGameModeStartDelayed_S()` : si operable et sans
//      controle, il passe a ENNEMI et fait apparaitre sa garnison.
//
// Ces deux passes ont lieu au DEMARRAGE DU MONDE. Nos controleurs, crees apres, les ont
// manquees : sans ce rappel ils resteraient inertes, sans garnison et sans couverture.
//
// ======================================================================================
//  CE QUE CA DEBLOQUE
// ======================================================================================
//   - le signal : livemap vivante autour des sites, balises qui transmettent, sac radio utile ;
//   - les depeches radio de Reoccupation, enfin collectables ;
//   - un OBJECTIF : le site est ENNEMI au depart, donc a prendre -- et l'antenne est
//     destructible, or abattre la tour rend le site inoperant et remet son controle a NONE
//     (`JWK_RadioSiteEntity.OnTowerDestroyed_S`). Prendre ou saboter, deux facons de jouer.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict).

class FFRX_RadioSites
{
	//! Le controleur de site de FF, tel quel -- on n'en derive pas.
	static const ResourceName SITE_PREFAB =
		"{14C2550C9BBE7476}Prefabs/Controllers/Loadtime/JWK_RadioSiteController.et";

	//! Apres le demarrage du monde et les passes de FF.
	static const int BOOT_DELAY_MS = 90000;

	//! Laisse le controleur s'inscrire dans l'index avant de chercher les tours.
	static const int LINK_DELAY_MS = 3000;

	//! Positions des CONTROLEURS, a une dizaine de metres des antennes d'Anizay (donc bien
	//! sous les 30 m de liaison). Releve dans les layers de la carte :
	//!   tem_antenna1 : 8373 / 6813  (aerodrome est)
	//!   tem_antenna2 : 8264 / 8837  (nord-est)
	// ⚠️ Pas d'initialiseur immediat sur un champ statique (meme avec des valeurs) : ils
	// sont hisses dans UNE fonction d'init partagee par vanilla et TOUS les mods, dont le
	// buffer de 64 Ko deborde en "Too many instructions per function" sur des fichiers
	// innocents. Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<vector> s_aSpots;

	protected static array<vector> Spots()
	{
		if (!s_aSpots)
		{
			s_aSpots = new array<vector>();
			s_aSpots.Insert(Vector(8380, 0, 6820));
			s_aSpots.Insert(Vector(8270, 0, 8845));
		}

		return s_aSpots;
	}

	protected static bool s_bDone;

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(PlaceAll, BOOT_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void PlaceAll()
	{
		if (s_bDone)
			return;

		s_bDone = true;

		// Si la carte possede DEJA des sites radio, on ne touche a rien. Ce fichier est un
		// rattrapage pour Anizay, pas une regle generale : sur une carte correctement equipee
		// il doit rester silencieux. C'est AUSSI le garde-fou contre le doublon au
		// redemarrage -- le controleur ne se re-spawne pas tout seul depuis la sauvegarde
		// (`m_bSelfSpawn 0`), mais des sites poses dans l'editeur plus tard, eux, seraient
		// bien la.
		array<EntityID> existing = JWK_IndexSystem.Get(GetGame().GetWorld()).GetAll(JWK_RadioSiteEntity);
		if (existing && !existing.IsEmpty())
		{
			Print(string.Format("[FFRX][RadioSite] %1 site(s) deja presents : rien a poser.",
				existing.Count()), LogLevel.NORMAL);
			return;
		}

		int placed = 0;
		foreach (vector spot : Spots())
		{
			if (PlaceOne(spot))
				placed = placed + 1;
		}

		Print(string.Format("[FFRX][RadioSite] %1 controleur(s) pose(s).", placed), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool PlaceOne(vector spot)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		Resource res = Resource.Load(SITE_PREFAB);
		if (!res || !res.IsValid())
		{
			Print("[FFRX][RadioSite] Controleur de site introuvable (FF absent ?).", LogLevel.WARNING);
			return false;
		}

		// Les emplacements sont releves en 2D : on recale la hauteur sur le terrain.
		vector pos = spot;
		pos[1] = world.GetSurfaceY(pos[0], pos[2]);

		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(sp.Transform);
		sp.Transform[3] = pos;

		IEntity ent = GetGame().SpawnEntityPrefab(res, world, sp);
		if (!ent)
			return false;

		JWK_RadioSiteEntity site = JWK_RadioSiteEntity.Cast(ent);
		if (!site)
		{
			Print("[FFRX][RadioSite] L'entite posee n'est pas un JWK_RadioSiteEntity.", LogLevel.WARNING);
			return false;
		}

		GetGame().GetCallqueue().CallLater(Activate, LINK_DELAY_MS, false, site);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rejoue les deux passes de FF que ce site a manquees.
	protected static void Activate(JWK_RadioSiteEntity site)
	{
		if (!site)
			return;

		site.LinkTowers_S();

		if (!site.IsOperable())
		{
			// Cause la plus probable : aucune antenne portant JWK_RadioTowerComponent dans les
			// 30 m -- donc les overrides de tem_antenna1/2 ne sont pas charges, ou le
			// controleur est pose trop loin. On le dit, parce que l'echec est autrement
			// TOTALEMENT muet : le site existe, s'affiche sur la carte, et ne sert a rien.
			Print(string.Format("[FFRX][RadioSite] NON operable en %1 : aucune tour liee dans les 30 m.",
				site.GetOrigin()), LogLevel.WARNING);
			return;
		}

		site.OnPostGameModeStartDelayed_S();
		Print(string.Format("[FFRX][RadioSite] Site operable et garnisonne en %1.", site.GetOrigin()), LogLevel.NORMAL);
	}
}

// ---------------------------------------------------------------------------
//  #radiosites -- etat
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_RadioSitesCommand : ScrServerCommand
{
	override string GetKeyword() { return "radiosites"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return Report();
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return Report();
	}

	protected ScrServerCmdResult Report()
	{
		array<EntityID> sites = JWK_IndexSystem.Get(GetGame().GetWorld()).GetAll(JWK_RadioSiteEntity);
		if (!sites || sites.IsEmpty())
			return ScrServerCmdResult("Aucun site radio sur la carte.", EServerCmdResultType.OK);

		int operable = 0;
		int ours = 0;

		foreach (EntityID id : sites)
		{
			JWK_RadioSiteEntity s = JWK_RadioSiteEntity.Cast(GetGame().GetWorld().FindEntityByID(id));
			if (!s || !s.IsOperable())
				continue;

			operable = operable + 1;

			JWK_FactionControlComponent fc = s.GetFactionControl();
			if (fc && fc.IsPlayerFaction())
				ours = ours + 1;
		}

		return ScrServerCmdResult(
			"Sites radio : " + sites.Count().ToString() + " au total, "
			+ operable.ToString() + " operable(s), " + ours.ToString() + " tenu(s) par la resistance.",
			EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
