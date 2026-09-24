// FF - REMIXED - PVE
// Guerre asymetrique -- CARCASSES DE VEHICULES PIEGEES.
//
// ======================================================================================
//  L'INTENTION
// ======================================================================================
// Une epave au bord de la route est du decor RASSURANT : le joueur la longe sans y penser,
// parfois il la fouille. En piegeant une minorite d'entre elles, chaque carcasse redevient
// une question -- et la fouille, un vrai pari. On ne veut surtout PAS toutes les piéger :
// une epave dangereuse a coup sur, ca ne cree pas du doute, ca cree une regle a apprendre.
//
// Complete les deux autres menaces posees : le champ de mines (FFRX_MinePlacement) et les
// IED sur les lieux (FFRX_IEDScatter). Meme philosophie que ce dernier -- le danger est une
// propriete du LIEU, il est la avant que le joueur arrive.
//
// ======================================================================================
//  CE QU'EST UNE EPAVE, TECHNIQUEMENT -- ET POURQUOI CE FICHIER EXISTE A PART
// ======================================================================================
// On aurait pu croire que `FFRX_BoobyTrapCars` suffisait. Non : il cherche des `Vehicle`
// CIVILS et INTACTS (role AMBIENT, sans occupant) parmi les entites DYNAMIQUES. Or une epave
// de decor n'est pas un vehicule du tout -- c'est un PROP STATIQUE
// (`PrefabLibrary/.../Props/Military/BMP1_wreck.et`, `UAZ452_wreck_static.et`,
// `M923A1_wreck.et`...). Elle n'apparait dans aucune requete `DYNAMIC`, et `Vehicle.Cast`
// y echoue. D'ou un scanner distinct.
//
// On les reconnait au NOM DE PREFAB contenant "wreck" : c'est la convention du jeu de base
// et elle est respectee par les bibliotheques Cain et Default. Un mod qui nommerait ses
// epaves autrement passerait au travers -- c'est accepte, on ne va pas maintenir une liste
// de GUID qui se perimerait a chaque mise a jour de carte.
//
// ======================================================================================
//  LE BALAYAGE EST DECOUPE EN TUILES -- NE PAS "SIMPLIFIER" EN UNE SEULE REQUETE
// ======================================================================================
// Trouver les props statiques impose `EQueryEntitiesFlags.STATIC`, qui ratisse des dizaines
// de milliers d'entites sur une carte entiere (chaque arbre, chaque muret). Une requete
// unique sur toute la carte ferait une pointe de charge d'un coup, sur le thread principal,
// donc un a-coup visible pour tous les joueurs connectes.
//
// On decoupe donc la carte en tuiles et on n'en traite QU'UNE par seconde. Le balayage
// complet prend une minute ou deux au demarrage, pendant que personne ne s'en apercoit.
// C'est fait UNE FOIS : les epaves de decor ne bougent pas, inutile de rescanner.
//
// ======================================================================================
//  LE PIEGE
// ======================================================================================
// `E_C4` d'IED Emporium : modele discret (une charge radiocommandee, credible scotchee a une
// carcasse) et rayon de 7 m herite de `E_Bin_IED_LowYield` -- assez pour punir qui vient
// FOUILLER l'epave, sans exploser au passage d'un vehicule sur la route d'a cote.
//
// Comme FFRX_IEDScatter, ils ne sautent QUE sous le camp resistance : c'est le modded
// `GMFX_MinePressureTriggerComponent` de FFRX_IEDScatter.c qui s'en charge, et il delegue a
// `FFRX_IsResistanceSide` (definie dans Mines/FFRX_MineTrigger.c). On enregistre donc nos
// charges aupres de FFRX_IEDScatter pour beneficier du meme filtre -- une seule regle
// "qui declenche quoi" dans tout le mod.
//
// Jamais en territoire tenu par la resistance : une epave piegee a cote de la FOB serait
// une punition arbitraire, pas du suspense.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict). Amorce depuis FFRX_Boot.c.

class FFRX_TrappedWrecksTuning
{
	//! Apres FFRX_IEDScatter (45 s) : on ne fait pas deux gros travaux en meme temps.
	static const int FIRST_SCAN_MS = 75000;

	//! Une tuile par seconde. Voir l'en-tete : c'est ce qui evite l'a-coup.
	static const int TILE_MS       = 1000;

	//! Cote d'une tuile de balayage, en metres.
	static const float TILE_SIZE   = 2000.0;

	//! Part des epaves eligibles reellement piegees, en 0..1. BAS volontairement : c'est le
	//! doute qu'on veut produire, pas une regle "toute epave explose".
	static const float TRAP_RATIO  = 0.18;

	//! Charge posee sur l'epave : modele discret, rayon 7 m (cf. en-tete).
	static const ResourceName CHARGE = "{D12D85CB180ED49A}PrefabsEditable/Auto/Props/IEDs/E_C4.et";
}

// ---------------------------------------------------------------------------
class FFRX_TrappedWrecks
{
	protected static ref FFRX_TrappedWrecks s_Instance;

	protected ref array<vector> m_aWrecks = {};   // epaves reperees pendant le balayage
	protected ref array<IEntity> m_aCharges = {}; // charges posees
	protected bool m_bScanDone;
	protected bool m_bUnavailable;

	// Progression du balayage par tuiles.
	protected vector m_vMin;
	protected vector m_vMax;
	protected int m_iTileX;
	protected int m_iTileZ;
	protected int m_iTilesX;
	protected int m_iTilesZ;
	protected int m_iSeen;   // entites examinees, pour juger du cout dans le log

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;

		s_Instance = new FFRX_TrappedWrecks();
		GetGame().GetCallqueue().CallLater(s_Instance.BeginScan, FFRX_TrappedWrecksTuning.FIRST_SCAN_MS, false);
		Print("[FFRX][Epaves] Balayage des carcasses programme.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Plafond de carcasses piegees. Reglage FF "Carcasses piegees : max".
	protected static int MaxTrapped()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return 0;

		return Math.Round(c.m_fFFRX_TrappedWrecksMax);
	}

	//------------------------------------------------------------------------------------------------
	protected void BeginScan()
	{
		if (MaxTrapped() <= 0)
		{
			Print("[FFRX][Epaves] Reglage a 0 : balayage annule.", LogLevel.NORMAL);
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		world.GetBoundBox(m_vMin, m_vMax);

		float spanX = m_vMax[0] - m_vMin[0];
		float spanZ = m_vMax[2] - m_vMin[2];

		m_iTilesX = Math.Ceil(spanX / FFRX_TrappedWrecksTuning.TILE_SIZE);
		m_iTilesZ = Math.Ceil(spanZ / FFRX_TrappedWrecksTuning.TILE_SIZE);

		if (m_iTilesX < 1)
			m_iTilesX = 1;
		if (m_iTilesZ < 1)
			m_iTilesZ = 1;

		m_iTileX = 0;
		m_iTileZ = 0;

		Print(string.Format("[FFRX][Epaves] Balayage : %1 x %2 tuiles de %3 m, une par seconde.",
			m_iTilesX, m_iTilesZ, FFRX_TrappedWrecksTuning.TILE_SIZE), LogLevel.NORMAL);

		GetGame().GetCallqueue().CallLater(ScanTile, FFRX_TrappedWrecksTuning.TILE_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void ScanTile()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float x0 = m_vMin[0] + m_iTileX * FFRX_TrappedWrecksTuning.TILE_SIZE;
		float z0 = m_vMin[2] + m_iTileZ * FFRX_TrappedWrecksTuning.TILE_SIZE;

		vector mins = Vector(x0, -1000, z0);
		vector maxs = Vector(x0 + FFRX_TrappedWrecksTuning.TILE_SIZE, 3000, z0 + FFRX_TrappedWrecksTuning.TILE_SIZE);

		world.QueryEntitiesByAABB(mins, maxs, CollectWreck, null, EQueryEntitiesFlags.STATIC);

		// Tuile suivante.
		m_iTileX = m_iTileX + 1;
		if (m_iTileX >= m_iTilesX)
		{
			m_iTileX = 0;
			m_iTileZ = m_iTileZ + 1;
		}

		if (m_iTileZ < m_iTilesZ)
			return;

		// Balayage termine.
		GetGame().GetCallqueue().Remove(ScanTile);
		m_bScanDone = true;

		Print(string.Format("[FFRX][Epaves] Balayage termine : %1 carcasse(s) reperee(s) (%2 entites examinees).",
			m_aWrecks.Count(), m_iSeen), LogLevel.NORMAL);

		TrapSome();
	}

	//------------------------------------------------------------------------------------------------
	//! Retenu si le nom de prefab contient "wreck" (convention du jeu de base, cf. en-tete).
	protected bool CollectWreck(IEntity ent)
	{
		m_iSeen = m_iSeen + 1;

		if (!ent)
			return true;

		EntityPrefabData pd = ent.GetPrefabData();
		if (!pd)
			return true;

		string name = pd.GetPrefabName();
		if (name == "")
			return true;

		name.ToLower();
		if (!name.Contains("wreck"))
			return true;

		// Les epaves navales sont dans l'eau : personne ne va les fouiller a pied.
		if (name.Contains("shipwreck"))
			return true;

		m_aWrecks.Insert(ent.GetOrigin());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose des charges sur une MINORITE des epaves reperees, en territoire non tenu.
	protected void TrapSome()
	{
		int cap = MaxTrapped();
		if (cap <= 0 || m_aWrecks.IsEmpty())
			return;

		// Melange : sinon on piegerait toujours les epaves de la meme tuile (le coin de la
		// carte balaye en premier), donc toujours la meme region.
		for (int i = m_aWrecks.Count() - 1; i > 0; i--)
		{
			int j = Math.RandomInt(0, i + 1);
			vector tmp = m_aWrecks[i];
			m_aWrecks[i] = m_aWrecks[j];
			m_aWrecks[j] = tmp;
		}

		int wanted = Math.Round(m_aWrecks.Count() * FFRX_TrappedWrecksTuning.TRAP_RATIO);
		if (wanted > cap)
			wanted = cap;

		int done = 0;
		foreach (vector w : m_aWrecks)
		{
			if (done >= wanted)
				break;

			if (!AllowedHere(w))
				continue;

			if (TrapAt(w))
				done = done + 1;
		}

		Print(string.Format("[FFRX][Epaves] %1 carcasse(s) piegee(s) sur %2 reperee(s) (plafond %3).",
			done, m_aWrecks.Count(), cap), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected bool TrapAt(vector wreckPos)
	{
		Resource res = Resource.Load(FFRX_TrappedWrecksTuning.CHARGE);
		if (!res || !res.IsValid())
		{
			m_bUnavailable = true;
			Print("[FFRX][Epaves] Charge introuvable -- mod IED Emporium absent ou GUID change. Carcasses piegees desactivees.", LogLevel.WARNING);
			return false;
		}

		// Legerement a cote de l'epave : pose au centre exact, la charge se retrouverait dans
		// la carcasse (donc invisible, et potentiellement sans contact possible).
		float ang = Math.RandomFloat(0, Math.PI2);

		vector p = wreckPos;
		p[0] = p[0] + Math.Cos(ang) * 1.8;
		p[2] = p[2] + Math.Sin(ang) * 1.8;

		vector pos = p;
		if (!SCR_WorldTools.FindEmptyTerrainPosition(pos, p, 6))
			return false;

		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(sp.Transform);
		sp.Transform[3] = pos;

		IEntity charge = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		if (!charge)
			return false;

		m_aCharges.Insert(charge);

		// Meme filtre de faction que les IED disperses : seul le camp resistance declenche.
		FFRX_IEDScatter.RegisterForeign(charge);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Territoire ennemi, front ou terrain vague. Jamais chez nous (voir l'en-tete).
	protected bool AllowedHere(vector pos)
	{
		JWK_TerritoryControlSystem tc = JWK.GetTerritoryControl();
		if (!tc)
			return false;

		JWK_TerritoryControlNodeComponent node = tc.GetNodeAt(pos);
		if (!node)
			return true;

		JWK_EFactionRole role = node.GetFactionRole();
		if (role == JWK_EFactionRole.PLAYER || role == JWK_EFactionRole.SUPPORTING)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Etat, pour la commande #epaves.
	static string Report()
	{
		if (!s_Instance)
			return "Carcasses piegees : pas demarre.";

		if (s_Instance.m_bUnavailable)
			return "Carcasses piegees : DESACTIVEES (mod IED Emporium absent ou GUID change).";

		if (!s_Instance.m_bScanDone)
			return "Carcasses piegees : balayage en cours ("
				+ s_Instance.m_aWrecks.Count().ToString() + " reperee(s) jusqu'ici).";

		// Les charges explosees sont retirees du compte.
		int live = 0;
		foreach (IEntity e : s_Instance.m_aCharges)
		{
			if (e && !e.IsDeleted())
				live = live + 1;
		}

		return "Carcasses : " + s_Instance.m_aWrecks.Count().ToString() + " reperee(s) sur la carte, "
			+ live.ToString() + " piegee(s) encore armee(s) (plafond " + MaxTrapped().ToString() + ").";
	}
}

// ---------------------------------------------------------------------------
//  #epaves -- etat du balayage et des carcasses piegees.
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_TrappedWrecksCommand : ScrServerCommand
{
	override string GetKeyword() { return "epaves"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return ScrServerCmdResult(FFRX_TrappedWrecks.Report(), EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult(FFRX_TrappedWrecks.Report(), EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
