// FF - REMIXED - PVE
// MURS EN DEUX POINTS : on pose un debut, on pose une fin, le mur se construit entre.
//
// ======================================================================================
//  CE QU'ON NE PEUT PAS REUTILISER, ET POURQUOI
// ======================================================================================
//
// Le jeu de base a bien un generateur de murs -- `WallGeneratorEntity`, celui qu'utilisent
// les prefabs `Prefabs/WEGenerators/Walls/...`. Il fait exactement ce qu'on veut : des
// groupes de longueurs, un objet de debut, un de fin, un objet intermediaire, du padding.
//
// IL EST INUTILISABLE EN JEU. Verifie ligne a ligne : sur les 854 lignes du fichier, TOUT
// ce qui genere est derriere `#ifdef WORKBENCH` (a partir de la ligne 133) et passe par
// `_WB_GetEditorAPI()` -- l'API de l'editeur de monde. En jeu, la classe existe mais ne
// sait rien faire. Meme chose pour `SCR_WallGroupContainer`.
//
// Il n'y a pas non plus de spline a l'execution. Verification faite dans le mod
// VehicleTrackSystem, qui pose des traces de roues : il ne fait PAS de spline non plus.
// Il calcule une distance parcourue, construit une matrice de transformation et spawne.
// C'est la bonne technique, et c'est celle qu'on reprend.
//
// ======================================================================================
//  COMMENT LE JOUEUR S'EN SERT
// ======================================================================================
//
//   1. menu de construction -> "Mur : point de depart"  -> il vise, il valide
//   2. il marche jusqu'au bout du futur mur
//   3. menu de construction -> "Mur : point d'arrivee"  -> le mur apparait
//
// AUCUNE INTERFACE NOUVELLE. On detourne le menu de construction de FF, qui fournit deja
// tout : la camera de placement, le fantome, la validation de zone, la verification des
// supplies. Ecrire notre propre selecteur de points aurait demande une UI, un mode de
// visee et de la replication -- pour un resultat moins bon.
//
// L'accroche est la meme que le chantier : `DoBuildItem`. Les deux items de mur ne
// posent AUCUNE entite -- on intercepte avant, on retient la position, et on rend null.
//
// ======================================================================================
//  LE CALCUL, ET LES TROIS PIEGES
// ======================================================================================
//
// 1. LES LONGUEURS SONT MESUREES, PAS DECLAREES. "TinWall_01_6m_A" laisse croire a 6 m,
//    mais les pieces ont des chevauchements et le nom ne fait pas foi. On pose donc une
//    fois chaque piece hors du monde, on lit sa boite englobante, et on retient la mesure.
//    Meme principe que la profondeur d'enfouissement du chantier.
//
// 2. REMPLISSAGE GLOUTON, du plus grand au plus petit, comme le generateur du jeu de base.
//    Le reliquat sous la plus petite piece est abandonne : mieux vaut un mur un peu court
//    qu'une piece qui depasse dans le decor.
//
// 3. LA HAUTEUR SUIT LE TERRAIN, piece par piece (`GetSurfaceY`). Poser tout le mur au Y
//    du premier point donnerait un mur flottant des qu'il y a la moindre pente.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict).

class FFRX_WallKit
{
	// LES PIECES SONT LES NOTRES, PAS CELLES DU JEU DE BASE.
	//
	// Celles du jeu de base sont du DECOR DE CARTE : aucun RplComponent sur toute leur
	// chaine d'heritage. Le serveur construisait le mur (le log l'annoncait) et le client
	// ne voyait RIEN -- une entite sans replication n'existe que chez celui qui l'a creee.
	// En Workbench on est serveur et client a la fois, d'ou l'illusion que ca marchait.
	//
	// Nos prefabs derivent des memes modeles et ajoutent ce qu'il faut : replication,
	// persistance EPF, et l'action de demontage de FF (cf. Prefabs/FFRX/Walls/).
	// Un mur est donc visible par tous, survit au redemarrage, et se demonte a la pelle.

	//! Poteau d'extremite et de jonction.
	static const ResourceName POLE     = "{6FFEC0DEB2610100}Prefabs/FFRX/Walls/FFRX_TinWall_Pole.et";

	//! Panneaux, du plus long au plus court -- l'ordre EST l'ordre de remplissage.
	static const ResourceName PANEL_6A = "{6FFEC0DEB2610200}Prefabs/FFRX/Walls/FFRX_TinWall_6m_A.et";
	static const ResourceName PANEL_6B = "{6FFEC0DEB2610300}Prefabs/FFRX/Walls/FFRX_TinWall_6m_B.et";
	static const ResourceName PANEL_6C = "{6FFEC0DEB2610400}Prefabs/FFRX/Walls/FFRX_TinWall_6m_C.et";
	static const ResourceName PANEL_3  = "{6FFEC0DEB2610500}Prefabs/FFRX/Walls/FFRX_TinWall_3m.et";
}

class FFRX_WallBuilder
{
	//! Longueur maximale d'un mur, en metres. Garde-fou : sans lui, deux points aux
	//! extremites de la carte generreraient des milliers d'entites et tueraient le serveur.
	static const float MAX_LEN_M = 80;

	//! Plafond dur sur le nombre de pieces, seconde ceinture de securite.
	static const int MAX_PIECES = 40;

	//! Au-dela, le point de depart est considere comme oublie.
	static const float ANCHOR_TIMEOUT_MS = 600000;   // 10 min

	//! Cout en supplies par metre de mur.
	static const float SUPPLIES_PER_M = 8;

	//! Points de depart en attente, par joueur.
	protected static ref map<int, vector> s_mAnchor;
	protected static ref map<int, float>  s_mAnchorTime;

	//! Longueurs mesurees, par prefab. Mesure une seule fois par vie du serveur.
	protected static ref map<string, float> s_mLength;

	//! Correction de lacet par prefab : -90 si la piece est modelisee selon X.
	protected static ref map<string, float> s_mYawFix;

	//------------------------------------------------------------------------------------------------
	//! Le joueur vient de poser son point de depart.
	static void SetAnchor(int playerId, vector pos)
	{
		if (!s_mAnchor)
		{
			s_mAnchor = new map<int, vector>();
			s_mAnchorTime = new map<int, float>();
		}

		s_mAnchor.Set(playerId, pos);
		s_mAnchorTime.Set(playerId, GetGame().GetWorld().GetWorldTime());

		Print(string.Format("[FFRX][Mur] point de depart pose par le joueur %1.", playerId), LogLevel.NORMAL);
		FFRX_Chat.Local("Point de depart du mur pose. Va au bout et pose le point d'arrivee.");
	}

	//------------------------------------------------------------------------------------------------
	//! Le joueur pose son point d'arrivee : on construit.
	//! Rend true si un mur a ete produit.
	static bool Finish(int playerId, vector endPos, JWK_BuildAreaControllerComponent ctrl)
	{
		vector start;
		if (!s_mAnchor || !s_mAnchor.Find(playerId, start))
		{
			Print("[FFRX][Mur] refus : aucun point de depart pour ce joueur.", LogLevel.NORMAL);
			return false;
		}

		float when;
		s_mAnchorTime.Find(playerId, when);
		if (GetGame().GetWorld().GetWorldTime() - when > ANCHOR_TIMEOUT_MS)
		{
			Forget(playerId);
			Print("[FFRX][Mur] refus : point de depart trop ancien, oublie.", LogLevel.NORMAL);
			return false;
		}

		Forget(playerId);

		// Direction a plat : un mur est vertical meme en pente, seule l'orientation au sol
		// compte. Garder la composante Y inclinerait chaque panneau.
		vector delta = endPos - start;
		delta[1] = 0;

		float total = delta.Length();
		if (total < 2)
		{
			Print("[FFRX][Mur] refus : les deux points sont trop proches.", LogLevel.NORMAL);
			return false;
		}

		if (total > MAX_LEN_M)
			total = MAX_LEN_M;

		// --- PAIEMENT PARTIEL : on construit ce qu'on peut payer, et on le dit.
		//
		// Refuser tout le mur parce qu'il manque 20 supplies etait frustrant et peu
		// realiste : un chantier qu'on n'a pas les moyens de finir, on le commence quand
		// meme par un bout. Le joueur revient avec du ravitaillement et pose la suite --
		// deux murs bout a bout donnent le meme resultat qu'un seul.
		JWK_LogisticsStorageControllerComponent storage;
		if (ctrl)
			storage = ctrl.GetLogisticsStorage();

		if (!storage)
		{
			Print("[FFRX][Mur] refus : aucun stockage logistique dans cette zone.", LogLevel.NORMAL);
			return false;
		}

		int cost = Math.Round(total * SUPPLIES_PER_M);
		float built = total;

		if (!storage.HasResources(JWK_ELogisticsResourceType.SUPPLIES, cost))
		{
			// On cherche la plus grande longueur payable. Dichotomie plutot que boucle au
			// metre : HasResources est un appel, autant en faire 7 que 80.
			float lo = 0;
			float hi = total;
			for (int i = 0; i < 7; i++)
			{
				float mid = (lo + hi) * 0.5;
				if (storage.HasResources(JWK_ELogisticsResourceType.SUPPLIES, Math.Round(mid * SUPPLIES_PER_M)))
					lo = mid;
				else
					hi = mid;
			}

			built = lo;
			cost = Math.Round(built * SUPPLIES_PER_M);

			if (built < 3)
			{
				Print("[FFRX][Mur] refus : meme un troncon de 3 m est hors de prix.", LogLevel.NORMAL);
				return false;
			}

			Print(string.Format("[FFRX][Mur] ravitaillement insuffisant : %1 m demandes, %2 m construits.",
				(int)total, (int)built), LogLevel.NORMAL);
			FFRX_Chat.Local(string.Format("Ravitaillement insuffisant : le mur s arrete a %1 m sur %2.",
				(int)built, (int)total));
		}

		delta.Normalize();

		int pieces = PieceCount(built);
		if (pieces <= 0)
			return false;

		storage.TakeResources(JWK_ELogisticsResourceType.SUPPLIES, cost);

		// --- Le mur ne sort plus du sol d'un coup : il se monte a la pelle.
		// On pose un chantier, qui fera apparaitre les panneaux un par un (cf.
		// FFRX_BuildSite.c). Un mur qui apparait instantanement etait en contradiction
		// avec tout le reste de la construction.
		if (!FFRX_BuildSite.OpenWallSite(start, delta, built, pieces, cost))
		{
			// Repli : si le chantier ne peut pas s'ouvrir, on pose le mur directement
			// plutot que de prendre les supplies sans rien livrer.
			Print("[FFRX][Mur] chantier impossible, pose directe.", LogLevel.WARNING);
			BuildRange(start, delta, built, 0, pieces);
		}

		Print(string.Format("[FFRX][Mur] chantier ouvert : %1 m, %2 pieces, %3 supplies.",
			(int)built, pieces, cost), LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Forget(int playerId)
	{
		if (!s_mAnchor)
			return;
		s_mAnchor.Remove(playerId);
		s_mAnchorTime.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Etablit le PLAN du mur : quelle piece a quelle distance, dans l'ordre.
	//!
	//! Separe de la pose pour une raison de fond : le mur ne sort plus du sol d'un coup, il
	//! se monte a la pelle panneau par panneau (cf. FFRX_BuildSite.c). Il faut donc pouvoir
	//! REJOUER le meme plan a l'identique -- a chaque palier d'avancement, et encore apres un
	//! redemarrage du serveur. Un plan deterministe, recalcule des memes trois parametres
	//! (depart, direction, longueur), evite d'avoir a sauvegarder la liste des pieces.
	static void Plan(float total, out array<ResourceName> outPrefabs, out array<float> outDist)
	{
		outPrefabs = {};
		outDist = {};

		float lPole = Measure(FFRX_WallKit.POLE);
		float l6    = Measure(FFRX_WallKit.PANEL_6A);
		float l3    = Measure(FFRX_WallKit.PANEL_3);

		if (l6 < 0.5 || l3 < 0.5)
		{
			Print("[FFRX][Mur] mesure des panneaux impossible, abandon.", LogLevel.ERROR);
			return;
		}

		float at = 0;

		outPrefabs.Insert(FFRX_WallKit.POLE);
		outDist.Insert(at);
		at = at + lPole;

		while (outPrefabs.Count() < MAX_PIECES)
		{
			float left = total - at;

			if (left >= l6)
			{
				// Alternance des trois variantes : un mur d'un seul motif repete se voit.
				ResourceName panel = FFRX_WallKit.PANEL_6A;
				int v = outPrefabs.Count() - (outPrefabs.Count() / 3) * 3;   // pas de % en Enforce
				if (v == 1)
					panel = FFRX_WallKit.PANEL_6B;
				else if (v == 2)
					panel = FFRX_WallKit.PANEL_6C;

				// MESURER LA VARIANTE QU'ON POSE, pas seulement la premiere. Bug constate le
				// 2026-09-19 : B et C n'etant jamais mesurees, leur correction d'axe valait 0
				// et deux panneaux sur trois sortaient a 90 degres.
				float pl = Measure(panel);
				if (pl < 0.5 || left < pl)
				{
					panel = FFRX_WallKit.PANEL_6A;
					pl = l6;
				}

				outPrefabs.Insert(panel);
				outDist.Insert(at);
				at = at + pl;
				continue;
			}

			if (left >= l3)
			{
				outPrefabs.Insert(FFRX_WallKit.PANEL_3);
				outDist.Insert(at);
				at = at + l3;
				continue;
			}

			break;   // reliquat plus court que la plus petite piece
		}

		// Poteau d'arrivee, colle a la derniere piece et non au point vise : sinon il
		// flotterait dans le reliquat abandonne.
		if (outPrefabs.Count() < MAX_PIECES)
		{
			outPrefabs.Insert(FFRX_WallKit.POLE);
			outDist.Insert(at);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pose les pieces d'indice [from, to[ du plan. Rend le nombre reellement pose.
	static int BuildRange(vector startPos, vector dir, float total, int from, int to)
	{
		array<ResourceName> prefabs;
		array<float> dist;
		Plan(total, prefabs, dist);

		if (!prefabs || prefabs.IsEmpty())
			return 0;

		if (to > prefabs.Count())
			to = prefabs.Count();

		float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;

		int placed = 0;
		for (int i = from; i < to; i++)
		{
			if (Spawn(prefabs[i], startPos, dir, dist[i], yaw))
				placed++;
		}

		return placed;
	}

	//------------------------------------------------------------------------------------------------
	//! Combien de pieces compte ce mur ?
	static int PieceCount(float total)
	{
		array<ResourceName> prefabs;
		array<float> dist;
		Plan(total, prefabs, dist);
		if (!prefabs)
			return 0;
		return prefabs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Pose une piece a `dist` metres du depart, orientee le long de la ligne et posee sur
	//! le terrain.
	protected static bool Spawn(ResourceName prefab, vector start, vector dir, float dist, float yaw)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return false;

		vector p = start + dir * dist;

		// La hauteur est relue A CHAQUE PIECE. Reprendre le Y du point de depart donnerait
		// un mur flottant ou enterre des la moindre pente.
		BaseWorld world = GetGame().GetWorld();
		p[1] = world.GetSurfaceY(p[0], p[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(Vector(yaw + YawFix(prefab), 0, 0), params.Transform);
		params.Transform[3] = p;

		return GetGame().SpawnEntityPrefab(res, world, params) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Longueur reelle d'une piece, ET l'axe sur lequel elle est modelisee.
	//!
	//! POURQUOI L'AXE COMPTE (bug constate en jeu le 2026-09-19 : les panneaux sortaient
	//! perpendiculaires a la ligne). Un prefab de mur est modelise soit le long de Z, soit
	//! le long de X, et rien dans son nom ne le dit. Le generateur du jeu de base a
	//! exactement ce reglage -- `UseXAsForward`, qui applique `rotationOffset = -90`.
	//! On le DEDUIT au lieu de le declarer : la plus grande extension horizontale locale
	//! donne l'axe long, donc l'axe de pose.
	//!
	//! Bounds LOCALES (`GetBounds`), pas mondiales : la piece de mesure est posee sans
	//! rotation, mais `GetWorldBounds` melangerait les axes des le moindre pivot.
	protected static float Measure(ResourceName prefab)
	{
		if (!s_mLength)
		{
			s_mLength = new map<string, float>();
			s_mYawFix = new map<string, float>();
		}

		float cached;
		if (s_mLength.Find(prefab, cached))
			return cached;

		float len = 0;
		float yawFix = 0;

		Resource res = Resource.Load(prefab);
		if (res && res.IsValid())
		{
			// Loin sous la carte : la piece de mesure ne doit etre vue par personne.
			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix(Vector(0, 0, 0), params.Transform);
			params.Transform[3] = Vector(0, -500, 0);

			IEntity probe = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
			if (probe)
			{
				vector mins, maxs;
				probe.GetBounds(mins, maxs);
				SCR_EntityHelper.DeleteEntityAndChildren(probe);

				float dx = maxs[0] - mins[0];
				float dz = maxs[2] - mins[2];

				if (dx > dz)
				{
					len = dx;
					yawFix = -90;   // modelisee selon X : meme correction que le moteur
				}
				else
				{
					len = dz;
				}
			}
		}

		s_mLength.Set(prefab, len);
		s_mYawFix.Set(prefab, yawFix);

		string axis = "Z";
		if (yawFix != 0)
			axis = "X (corrige de -90 deg)";

		Print(string.Format("[FFRX][Mur] piece mesuree : %1 m, axe %2 -- %3", len, axis, prefab), LogLevel.NORMAL);
		return len;
	}

	//------------------------------------------------------------------------------------------------
	//! Correction de lacet de la piece. Measure() doit avoir ete appele avant.
	protected static float YawFix(ResourceName prefab)
	{
		float f;
		if (s_mYawFix && s_mYawFix.Find(prefab, f))
			return f;
		return 0;
	}
}
