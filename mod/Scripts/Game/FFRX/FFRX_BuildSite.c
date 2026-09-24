// FF - REMIXED - PVE
// CHANTIER : construire redevient un GESTE, pas un achat.
//
// ======================================================================================
//  LE PROBLEME
// ======================================================================================
//
// Dans FF, on passe en camera libre, on valide, et le batiment APPARAIT. Personne ne
// donne un coup de pelle. Construire n'est donc pas une action : c'est une depense.
// Consequence de jeu : une base ne se defend pas pendant qu'elle se monte, et le genie
// n'a aucun metier -- n'importe qui clique aussi bien que lui.
//
// Decisions Benji du 2026-09-18 traitees ici :
//   D2  geste de construction  -> PELLE SUR CHANTIER
//   D4  qui a le droit         -> LE GENIE POSE, TOUS AIDENT
//   D5  duree                  -> PROPORTIONNELLE AU BATIMENT
// D1/D3 (materiaux transportes, facture selon la taille) sont l'etape suivante : ce
// fichier ne touche pas au cout en supplies, qui reste preleve par FF comme avant.
//
// ======================================================================================
//  CE QU'ON REUTILISE -- ET CE QU'ON N'ECRIT PAS
// ======================================================================================
//
// Le jeu de base a DEJA un systeme de chantier complet (celui des sacs de sable de
// Conflict). FF ne s'en sert pas : il a le sien, qui fait apparaitre le prefab final
// sur-le-champ. On branche donc l'un sur l'autre.
//
//   SCR_CampaignBuildingBuildUserAction.PerformAction
//     -> AddBuildingValue(GetBuildingToolValue(user))      // = m_iConstructionValue de l'outil
//        -> RpcAsk_AddBuildingValue                        // client -> serveur
//           -> SCR_CampaignBuildingLayoutComponent.AddBuildingValue
//              -> EvaluateBuildingStatus : >= m_iToBuildValue -> SpawnComposition()
//
// Rien de tout ca n'est a ecrire : progression, replication, action, barre de progression
// et pourcentages affiches existent deja. Le bonus de rendement du genie s'y greffe tout
// seul, il vit dans ce meme chemin (cf. FFRX_RoleBonus.c).
//
// ATTENTION : Une note du plan datee du 2026-09-17 affirmait que le jeu de base "ne fait pas
// pelleter non plus", au motif que `Build()` est commentee dans le composant de gadget.
// C'est FAUX et ca a failli couter la reecriture complete du systeme : l'appelant de
// GetToolConstructionValue() n'est pas ce composant, c'est l'ACTION UTILISATEUR. Verifie
// trois fois depuis (lecture directe, 2e passe du plan, et le bonus du genie qui tourne
// sur cette chaine).
//
// ======================================================================================
//  L'INTERCEPTION, EN TROIS TEMPS
// ======================================================================================
//
// 1. `JWK_BuildAreaControllerComponent.DoBuildItem` (serveur) : on verrouille le droit de
//    poser, puis on note QUEL batiment est demande, et on laisse FF faire son travail --
//    validation de zone, verification des supplies, prelevement, enregistrement.
// 2. `JWK_ConstructionManagerComponent.SpawnBuildItem` : au lieu du batiment, on pose le
//    CHANTIER. FF ne voit pas la difference ; la suite de DoBuildItem l'enregistre comme
//    un objet de la zone, ce qui est souhaitable (un chantier occupe une place).
// 3. `SCR_CampaignBuildingLayoutComponent.SpawnComposition` : a 100 %, le chantier fait
//    apparaitre le vrai batiment par le chemin normal de FF, puis s'efface.
//
// Aucune des deux classes FF n'est `sealed` ni citee dans un `[Friend]` -- verifie, c'est
// la condition qui a fait echouer d'autres greffes (cf. JWK_AIForce).
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict).

//! PILES DE MATERIAUX : ce qu'on voit d'un chantier avant qu'il n'ait pris forme.
//!
//! POURQUOI IL EN FAUT UNE. Un chantier de mur ne pose rien tant qu'on n'a pas pellete :
//! il n'y avait donc RIEN a viser, et l'action "Construire" restait introuvable -- on ne
//! peut pas interagir avec une entite invisible (constate en jeu le 2026-09-19).
//!
//! POURQUOI DES MATERIAUX ET PAS DES DEBRIS. Le dossier Prefabs/Structures/Debris/ etait
//! le premier reflexe, mais des gravats se lisent comme une DESTRUCTION : on croirait un
//! batiment detruit, pas un chantier a monter. Les accessoires de
//! Prefabs/Props/Construction/ disent exactement le contraire -- du materiel en attente
//! d'etre employe. Le choix se change sur une ligne si l'effet ne convient pas.
class FFRX_BuildMaterials
{
	// Nos versions REPLIQUEES. Les props du jeu de base n'ont aucun RplComponent : sur le
	// dedie le joueur ne verrait rien (cf. Prefabs/FFRX/Site/).
	static const ResourceName PLANKS     = "{6FFEC0DEB2710100}Prefabs/FFRX/Site/FFRX_Pile_Planks.et";
	static const ResourceName PLANKS_BIG = "{6FFEC0DEB2710200}Prefabs/FFRX/Site/FFRX_Pile_PlanksBig.et";
	static const ResourceName BRICKS     = "{6FFEC0DEB2710300}Prefabs/FFRX/Site/FFRX_Pile_Bricks.et";
	static const ResourceName SAND       = "{6FFEC0DEB2710400}Prefabs/FFRX/Site/FFRX_Pile_Sand.et";

	//------------------------------------------------------------------------------------------------
	//! La pile posee en i-eme position. On MELANGE les matieres plutot que d'empiler quatre
	//! fois le meme tas : un chantier credible a du bois, des briques et du sable cote a cote.
	static ResourceName Nth(int i, int supplies)
	{
		int v = i - (i / 3) * 3;   // pas d'operateur % en Enforce

		if (v == 0)
		{
			if (supplies >= 600)
				return PLANKS_BIG;
			return PLANKS;
		}

		if (v == 1)
			return BRICKS;

		return SAND;
	}

	//------------------------------------------------------------------------------------------------
	//! Combien de tas pour un ouvrage de ce prix ? Le cout est le seul indicateur de taille
	//! dont on dispose pour les 23 batiments sans ecrire 23 lignes a la main.
	static int Count(int supplies)
	{
		if (supplies >= 800)
			return 6;
		if (supplies >= 400)
			return 4;
		if (supplies >= 150)
			return 3;
		return 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Rayon d'eparpillement, en metres.
	static float Radius(int supplies)
	{
		if (supplies >= 800)
			return 7;
		if (supplies >= 400)
			return 5;
		return 3;
	}
}

class FFRX_BuildSite
{
	static const ResourceName SITE_PREFAB = "{6FFEC0DEB1115171}Prefabs/FFRX/Build/FFRX_BuildSite.et";

	// --- Contexte pose par DoBuildItem, consomme par SpawnBuildItem -----------------
	// Les deux appels sont separes par une seule ligne de code FF, sur le meme fil : une
	// variable statique suffit et evite de changer la signature de SpawnBuildItem, qui
	// est appelee ailleurs dans FF (et qu'on ne veut surtout pas casser).
	static bool         s_bPending;
	static ResourceName s_sTargetPrefab;
	static ResourceName s_sVisualPrefab;
	static int          s_iSuppliesCost;

	//------------------------------------------------------------------------------------------------
	//! Combien de points de pelle faut-il pour monter ce batiment ? -- D5.
	//!
	//! On derive du COUT EN SUPPLIES, deja equilibre par FF, plutot que d'ecrire 23
	//! valeurs a la main : 23 occasions de se tromper et un enfer a reequilibrer. Un seul
	//! curseur regle la lourdeur de tout le systeme.
	//!
	//! UNITE DE COMPTE, mesuree en jeu le 2026-09-18 : l'action est en boucle
	//! (`Duration -5`, `PerformPerFrame 1`), elle verse donc la valeur de l'outil TOUTES
	//! LES 5 SECONDES. Une pelle vaut 10 points, un sapeur 25 (bonus x2,5).
	//!
	//! PREMIER REGLAGE ERRONE : j'avais pris 0,1 point par supply -> un bunker valait 12
	//! points, soit UN SEUL versement de sapeur. Benji l'a monte en 2 coups et n'a rien
	//! vu se construire. A 2 points par supply :
	//!   FiringPosition (50)  =  100 pts =  10 versements = 50 s (20 s pour un sapeur)
	//!   Bunker         (120) =  240 pts =  24 versements =  2 min (50 s)
	//!   CommandPost    (500) = 1000 pts -> plafonne
	//!   VehicleDepot  (1500) = 3000 pts -> plafonne
	static const float POINTS_PER_SUPPLY = 2.0;

	//! COMPRESSION DES GROS OUVRAGES, plutot qu'un plafond plat.
	//!
	//! Un plafond dur a 900 points ecrasait les quatre plus gros au MEME temps de montage :
	//! poste de commandement (500 supplies), grand stockage (1000), helipad de reparation
	//! (600) et depot de vehicules (1500) tombaient tous a 900. Un depot trois fois plus
	//! cher qu'un PC se serait monte aussi vite -- l'echelle des ouvrages disparaissait.
	//!
	//! Au-dela du genou on prend donc la racine : ca reste ordonne (plus cher = toujours
	//! plus long) mais ca cesse de croitre lineairement.
	//!   Bunker (120)        ->  240 pts =  2 min seul
	//!   Petit stockage (200)->  400 pts =  3,3 min
	//!   PC (500)            ->  890 pts =  7,4 min  (3 min pour un sapeur)
	//!   Grand stockage(1000)-> 1200 pts = 10 min
	//!   Depot (1500)        -> 1420 pts = 12 min    (4,7 min pour un sapeur)
	//! Et a plusieurs, tout ca se divise d'autant -- c'est la que "tous aident" prend son
	//! sens sur les gros ouvrages.
	static const float BUILD_KNEE = 400;

	static int ToBuildValue(int suppliesCost)
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return 0;

		float pct = cache.m_fFFRX_BuildSiteTime;
		if (pct <= 0)
			return 0;   // 0 = desactive, on laisse FF poser le batiment directement

		float v = suppliesCost * POINTS_PER_SUPPLY * pct * 0.01;

		if (v > BUILD_KNEE)
			v = BUILD_KNEE + Math.Sqrt((v - BUILD_KNEE) * BUILD_KNEE);

		int rounded = Math.Round(v);
		if (rounded < 1)
			rounded = 1;   // meme le plus petit ouvrage demande un coup de pelle

		return rounded;
	}

	//------------------------------------------------------------------------------------------------
	//! La zone de construction qui contient ce point, ou null.
	//!
	//! JWK_BuildAreaControllerComponent s'indexe lui-meme (SetComponentIndexed), on passe
	//! donc par l'index FF plutot que par une requete spatiale, et on tranche avec son
	//! propre test d'appartenance `Contains()` -- la zone n'est pas forcement un cercle.
	static JWK_BuildAreaControllerComponent FindController(vector pos)
	{
		JWK_IndexSystem idx = JWK_IndexSystem.Get();
		if (!idx)
			return null;

		array<EntityID> ids = idx.GetAll(JWK_BuildAreaControllerComponent);
		if (!ids)
			return null;

		foreach (EntityID id : ids)
		{
			JWK_BuildAreaControllerComponent c = JWK_CompTU<JWK_BuildAreaControllerComponent>.FindIn(id);
			if (c && c.Contains(pos))
				return c;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Ouvre un chantier de MUR : les panneaux apparaitront un par un a la pelle.
	//!
	//! Un mur n'a pas de "batiment cible" unique, mais un PLAN de N pieces. Le chantier
	//! retient donc les trois parametres qui suffisent a rejouer ce plan (depart, direction,
	//! longueur) plutot que la liste des pieces -- c'est ce qui permet de le reconstruire
	//! apres un redemarrage sans rien sauvegarder de plus.
	static bool OpenWallSite(vector start, vector dir, float total, int pieces, int supplies)
	{
		if (pieces <= 0)
			return false;

		Resource res = Resource.Load(SITE_PREFAB);
		if (!res || !res.IsValid())
			return false;

		int toBuild = ToBuildValue(supplies);
		if (toBuild <= 0)
			return false;

		// Le chantier se place au MILIEU du mur : c'est la que le joueur ira pelleter, et
		// c'est le point le moins eloigne de l'ensemble des panneaux.
		vector mid = start + dir * (total * 0.5);
		mid[1] = GetGame().GetWorld().GetSurfaceY(mid[0], mid[2]);

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		float yaw = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), params.Transform);
		params.Transform[3] = mid;

		IEntity site = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!site)
			return false;

		SCR_CampaignBuildingLayoutComponent layout =
			SCR_CampaignBuildingLayoutComponent.Cast(site.FindComponent(SCR_CampaignBuildingLayoutComponent));

		if (!layout)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(site);
			return false;
		}

		layout.FFRX_InitWallSite(start, dir, total, toBuild, supplies, FFRX_BuildSiteStore.NewKey());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	static void ClearPending()
	{
		s_bPending = false;
		s_sTargetPrefab = "";
		s_sVisualPrefab = "";
		s_iSuppliesCost = 0;
	}
}

// ======================================================================================
//  1. QUI POSE, ET QUOI -- verrou D4 + capture du batiment demande
// ======================================================================================

modded class JWK_BuildAreaControllerComponent
{
	override IEntity DoBuildItem(
		string itemKey,
		int prefabIndex,
		vector pos,
		vector angles,
		int playerId,
		out JWK_EFeedback outReason
	) {
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();

		// Reglage a 0 : comportement FF d'origine, batiment instantane. C'est le
		// garde-fou exige par le plan -- si le systeme deplait en jeu, on le coupe sans
		// republier le mod.
		if (!cache || cache.m_fFFRX_BuildSiteTime <= 0)
			return super.DoBuildItem(itemKey, prefabIndex, pos, angles, playerId, outReason);

		// --- D4, volet AUTORITE : seul le genie ouvre un chantier.
		//
		// Un verrou existe deja dans FFRX_BuildDirect.c, mais il est CLIENT : il empeche
		// d'ouvrir le menu, ce qui est du confort, pas de la securite. Sur le dedie,
		// l'autorite doit revalider elle-meme -- c'est la lecon de la memoire
		// `dedie-useraction-canperform-client-gate`, ou une action gatee seulement chez
		// le client marchait en Workbench et ne faisait rien sur le serveur.
		//
		// ATTENTION : Les deux verrous ne reconnaissent pas le genie de la meme facon : celui-ci
		// lit le drapeau `genie` de ffrx-groups.json (fiable, pilotable depuis le site),
		// l'autre compare le NOM de l'escouade a un mot-cle. Renommer ECHO casserait le
		// verrou client sans toucher a celui-ci. A unifier quand le drapeau sera replique
		// jusqu'au client.
		if (!FFRX_GroupsManager.IsGenie(playerId))
		{
			Print(string.Format("[FFRX][Chantier] refus : le joueur %1 n'est pas du genie.", playerId), LogLevel.NORMAL);
			return null;
		}

		// --- Murs en deux points : ces deux items ne posent AUCUNE entite. On intercepte
		// ici, avant que FF ne fasse quoi que ce soit, et on rend null (cf.
		// FFRX_WallBuilder.c). Le cout est preleve par le constructeur de mur lui-meme,
		// car il depend de la LONGUEUR, que FF ne peut pas connaitre a la validation.
		if (itemKey == "FFRX_WallStart")
		{
			FFRX_WallBuilder.SetAnchor(playerId, pos);
			return null;
		}

		if (itemKey == "FFRX_WallEnd")
		{
			FFRX_WallBuilder.Finish(playerId, pos, this);
			return null;
		}

		// --- Capture du batiment demande, pour SpawnBuildItem qui suit immediatement.
		JWK_BuildItemConfig itemConfig =
			JWK_BuildItemConfig.Cast(JWK.GetConstruction().GetConstructionItemConfig(itemKey));

		if (!itemConfig || !itemConfig.m_aPrefabs.IsIndexValid(prefabIndex))
			return super.DoBuildItem(itemKey, prefabIndex, pos, angles, playerId, outReason);

		FFRX_BuildSite.s_bPending      = true;
		FFRX_BuildSite.s_sTargetPrefab = itemConfig.m_aPrefabs[prefabIndex];
		FFRX_BuildSite.s_iSuppliesCost = itemConfig.m_iSuppliesCost;

		// Le fantome FF sert d'apparence au chantier : c'est le seul modele "batiment
		// entier" disponible sans creer d'art. Absent sur certains items -> le chantier
		// restera invisible, d'ou le log plus bas.
		// 8 des 23 batiments FF n'ont AUCUN m_aGhosts (releve le 2026-09-19 : les deux
		// armureries, les deux magasins d'equipement, la rampe, les deux helipads et la
		// position de tir). Sans repli leur chantier serait invisible -- rien a viser,
		// donc injouable. On retombe alors sur le prefab reel : il fait le meme modele.
		FFRX_BuildSite.s_sVisualPrefab = "";
		if (itemConfig.m_aGhosts && itemConfig.m_aGhosts.IsIndexValid(prefabIndex))
			FFRX_BuildSite.s_sVisualPrefab = itemConfig.m_aGhosts[prefabIndex];

		if (FFRX_BuildSite.s_sVisualPrefab == "")
			FFRX_BuildSite.s_sVisualPrefab = FFRX_BuildSite.s_sTargetPrefab;

		IEntity result = super.DoBuildItem(itemKey, prefabIndex, pos, angles, playerId, outReason);

		// Toujours nettoyer : si DoBuildItem a refuse AVANT d'appeler SpawnBuildItem
		// (hors zone, supplies insuffisants), le contexte resterait arme et le prochain
		// batiment pose serait remplace par un chantier du mauvais type.
		FFRX_BuildSite.ClearPending();

		return result;
	}
}

// ======================================================================================
//  2. POSER LE CHANTIER A LA PLACE DU BATIMENT
// ======================================================================================

modded class JWK_ConstructionManagerComponent
{
	override IEntity SpawnBuildItem(ResourceName prefab, vector pos, vector angles)
	{
		// Hors de notre sequence (FF spawne aussi des items ailleurs) : on ne touche a rien.
		if (!FFRX_BuildSite.s_bPending)
			return super.SpawnBuildItem(prefab, pos, angles);

		int toBuild = FFRX_BuildSite.ToBuildValue(FFRX_BuildSite.s_iSuppliesCost);
		if (toBuild <= 0)
			return super.SpawnBuildItem(prefab, pos, angles);

		Resource res = Resource.Load(FFRX_BuildSite.SITE_PREFAB);
		if (!res || !res.IsValid())
		{
			Print("[FFRX][Chantier] prefab de chantier introuvable -- pose directe du batiment.", LogLevel.ERROR);
			return super.SpawnBuildItem(prefab, pos, angles);
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(angles, params.Transform);
		params.Transform[3] = pos;

		IEntity site = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!site)
		{
			Print("[FFRX][Chantier] echec du spawn -- pose directe du batiment.", LogLevel.ERROR);
			return super.SpawnBuildItem(prefab, pos, angles);
		}

		SCR_CampaignBuildingLayoutComponent layout =
			SCR_CampaignBuildingLayoutComponent.Cast(site.FindComponent(SCR_CampaignBuildingLayoutComponent));

		if (!layout)
		{
			Print("[FFRX][Chantier] le prefab de chantier n'a pas de LayoutComponent.", LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(site);
			return super.SpawnBuildItem(prefab, pos, angles);
		}

		layout.FFRX_InitSite(prefab, FFRX_BuildSite.s_sVisualPrefab, toBuild, pos, angles,
			FFRX_BuildSite.s_iSuppliesCost, FFRX_BuildSiteStore.NewKey());

		Print(string.Format("[FFRX][Chantier] ouvert : %1 (%2 supplies) -- %3 points de pelle.",
			prefab, FFRX_BuildSite.s_iSuppliesCost, toBuild), LogLevel.NORMAL);

		return site;
	}
}

// ======================================================================================
//  3. LA PROGRESSION, PUIS LE VRAI BATIMENT
// ======================================================================================

modded class SCR_CampaignBuildingLayoutComponent
{
	//! Batiment a faire apparaitre a 100 %. Vide = chantier du jeu de base, on n'y touche pas.
	protected ResourceName m_sFFRX_Target;
	protected vector       m_vFFRX_Pos;
	protected vector       m_vFFRX_Angles;

	//! Apparence du chantier : le batiment enfonce dans le sol, qui monte au fil des coups.
	//! Les tas de materiaux du chantier. Ils se consomment au fil des coups de pelle.
	protected ref array<IEntity> m_aFFRX_Props;

	//! Prefab d'apparence : on le regarde a chaque palier pour REPOSER le visuel.
	protected ResourceName m_sFFRX_Visual;

	// --- Mode MUR : le chantier ne produit pas UN batiment mais N panneaux -----------
	//! Vide = chantier de batiment classique. Sinon, direction du mur.
	protected vector       m_vFFRX_WallDir;
	protected float        m_fFFRX_WallLen;
	protected int          m_iFFRX_WallDone;   // pieces deja posees

	//! Cout paye a la pose : sert au remboursement si on demonte le chantier.
	protected int          m_iFFRX_Supplies;
	//! Cle de sauvegarde (cf. FFRX_BuildSiteStore). Vide = chantier non persiste.
	protected string       m_sFFRX_Key;

	//------------------------------------------------------------------------------------------------
	//! Arme un chantier de MUR. Les panneaux apparaissent un par un au fil des coups de
	//! pelle, au lieu de sortir du sol : une piece de mur est une entite statique, on ne
	//! peut pas la DEPLACER apres coup. La faire apparaitre a sa place
	//! definitive est a la fois plus simple et plus lisible -- le mur s'assemble panneau
	//! par panneau, comme un vrai chantier.
	void FFRX_InitWallSite(vector start, vector dir, float total, int toBuildValue, int supplies, string key)
	{
		m_vFFRX_Pos      = start;
		m_vFFRX_WallDir  = dir;
		m_fFFRX_WallLen  = total;
		m_iFFRX_WallDone = 0;
		m_iToBuildValue  = toBuildValue;
		m_iFFRX_Supplies = supplies;
		m_sFFRX_Key      = key;

		// Pas de m_sFFRX_Target : c'est ce qui distingue les deux modes.
		m_sFFRX_Target = "";

		// Les tas de materiaux SONT le chantier tant qu'aucun panneau n'est pose : sans eux
		// le joueur n'a rien a viser, donc pas d'action -- c'est ce qui manquait.
		// On les pose autour du MILIEU du mur, la ou se trouve l'entite de chantier.
		vector mid = start + dir * (total * 0.5);
		mid[1] = GetGame().GetWorld().GetSurfaceY(mid[0], mid[2]);
		m_vFFRX_Pos = mid;
		FFRX_OpenSite();
		m_vFFRX_Pos = start;   // le plan du mur, lui, repart du DEPART

		FFRX_BuildSiteStore.NoteWall(m_sFFRX_Key, start, dir, total, toBuildValue, 0, supplies);
		GetOnAddBuildingValueInt().Insert(FFRX_OnProgress);
	}

	//------------------------------------------------------------------------------------------------
	//! Chantier de mur ? (par opposition a un chantier de batiment)
	protected bool FFRX_IsWall()
	{
		return m_fFFRX_WallLen > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce composant est-il sur NOTRE prefab de chantier ?
	//!
	//! On teste le PREFAB, pas `m_sFFRX_Target`. C'est indispensable : le champ cible est
	//! pose par le serveur dans SpawnBuildItem et n'est pas replique, donc il est VIDE chez
	//! le client -- or c'est le client qui decide d'afficher une action. Le nom de prefab,
	//! lui, est connu des deux cotes. (Meme piege que la memoire
	//! `dedie-useraction-canperform-client-gate`, pris dans l'autre sens.)
	protected bool FFRX_IsOurSite()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		EntityPrefabData pd = owner.GetPrefabData();
		if (!pd)
			return false;

		return pd.GetPrefabName().Contains("FFRX_BuildSite");
	}

	//------------------------------------------------------------------------------------------------
	//! Rendre l'action "Construire" visible sur nos chantiers.
	//!
	//! CE QUI BLOQUAIT (constate en jeu le 2026-09-18) : le bunker sortait bien du sol, mais
	//! aucune action n'apparaissait. En cause,
	//!     SCR_CampaignBuildingBuildUserAction.CanBeShownScript -> m_LayoutComponent.HasBuildingPreview()
	//! et `HasBuildingPreview()` se contente de `return m_PreviewEntity;`.
	//!
	//! Cette entite d'apercu n'est creee que par SpawnPreview(), qui va chercher le prefab de
	//! la composition via le gestionnaire de Conflict (GetCompositionResourceName(m_iPrefabId)).
	//! On n'emprunte pas ce chemin -- notre cible vient de FF -- donc m_PreviewEntity reste
	//! null et l'action ne s'affiche jamais.
	//!
	//! On ne fabrique pas un SCR_BasePreviewEntity pour contenter ce test : notre visuel
	//! (le fantome FF enfonce dans le sol) joue deja ce role, et un vrai apercu ajouterait
	//! un modele translucide par-dessus. On repond simplement "oui, il y a de quoi voir".
	override bool HasBuildingPreview()
	{
		if (super.HasBuildingPreview())
			return true;

		return FFRX_IsOurSite();
	}

	//------------------------------------------------------------------------------------------------
	//! Arme un chantier FFRX. Appele juste apres le spawn, AVANT toute interaction --
	//! l'ordre compte : `m_iToBuildValue` vaut 0 a la naissance, et EvaluateBuildingStatus
	//! ferait apparaitre le batiment au tout premier coup de pelle (0 >= 0).
	void FFRX_InitSite(ResourceName target, ResourceName visual, int toBuildValue, vector pos, vector angles,
	                   int supplies, string key)
	{
		m_sFFRX_Target   = target;
		m_vFFRX_Pos      = pos;
		m_vFFRX_Angles   = angles;
		m_iToBuildValue  = toBuildValue;
		m_iFFRX_Supplies = supplies;
		m_sFFRX_Key      = key;
		m_sFFRX_Visual   = visual;

		FFRX_OpenSite();

		FFRX_BuildSiteStore.Note(m_sFFRX_Key, target, visual, pos, angles, toBuildValue, 0, supplies);

		// Le meme invocateur que celui qui declenche EvaluateBuildingStatus : la hauteur
		// du batiment suit donc exactement la progression, sans tick a nous.
		GetOnAddBuildingValueInt().Insert(FFRX_OnProgress);
	}

	//------------------------------------------------------------------------------------------------
	//! Ouvre le chantier : on eparpille des TAS DE MATERIAUX autour du point.
	//!
	//! CE QU'ON A ABANDONNE, ET POURQUOI (arbitrage Benji, 2026-09-19). La premiere version
	//! posait le batiment enfonce dans le sol et le faisait monter par paliers. Ca marchait,
	//! mais trois defauts :
	//!   - une piece statique ne se DEPLACE pas apres coup, il fallait la reposer a chaque
	//!     palier -- lourd, et visuellement un "pop" a chaque etape ;
	//!   - il fallait mesurer la hauteur de chaque batiment pour savoir de combien l'enterrer ;
	//!   - un batiment a moitie sorti de terre reste un batiment : on pouvait potentiellement
	//!     s'en servir avant la fin.
	//!
	//! Un chantier, c'est d'abord du MATERIEL au sol. On pose donc des tas, ils se consomment
	//! au fil des coups de pelle, et l'ouvrage apparait d'un coup a la fin. Plus simple, plus
	//! lisible, et sans aucun des trois defauts ci-dessus.
	protected void FFRX_OpenSite()
	{
		int n = FFRX_BuildMaterials.Count(m_iFFRX_Supplies);
		float radius = FFRX_BuildMaterials.Radius(m_iFFRX_Supplies);

		m_aFFRX_Props = {};

		BaseWorld world = GetGame().GetWorld();

		for (int i = 0; i < n; i++)
		{
			// Repartition en etoile plutot qu'aleatoire : le resultat est REPRODUCTIBLE,
			// donc un chantier repris apres redemarrage retrouve exactement la meme allure.
			float ang = (i * 360.0 / n) * Math.DEG2RAD;
			float d = radius * 0.6;
			if (i - (i / 2) * 2 == 1)
				d = radius;   // on alterne deux couronnes pour que ca ne fasse pas un cercle

			vector at = Vector(m_vFFRX_Pos[0] + Math.Sin(ang) * d,
			                   0,
			                   m_vFFRX_Pos[2] + Math.Cos(ang) * d);
			at[1] = world.GetSurfaceY(at[0], at[2]);

			Resource res = Resource.Load(FFRX_BuildMaterials.Nth(i, m_iFFRX_Supplies));
			if (!res || !res.IsValid())
				continue;

			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix(Vector(i * 47, 0, 0), params.Transform);   // orientations variees
			params.Transform[3] = at;

			IEntity pile = GetGame().SpawnEntityPrefab(res, world, params);
			if (pile)
				m_aFFRX_Props.Insert(pile);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Supprime tous les tas encore presents.
	protected void FFRX_ClearSite()
	{
		if (!m_aFFRX_Props)
			return;

		foreach (IEntity e : m_aFFRX_Props)
		{
			if (e)
				SCR_EntityHelper.DeleteEntityAndChildren(e);
		}

		m_aFFRX_Props.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Reprend un chantier de batiment apres redemarrage.
	void FFRX_RestoreProgress(float current)
	{
		if (m_iToBuildValue <= 0)
			return;

		float v = Math.Clamp(current, 0, m_iToBuildValue - 1);
		SetBuildingValue(v);
		FFRX_ConsumeSite(v);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire les tas au fur et a mesure : le materiel part dans l'ouvrage.
	//!
	//! C'est ce qui rend l'avancement LISIBLE sans rien deplacer. Un joueur qui arrive voit
	//! d'un coup d'oeil s'il reste beaucoup a faire -- et le pourcentage affiche par l'action
	//! est faux cote client (m_iToBuildValue n'est pas replique), donc c'est le seul repere
	//! honnete dont il dispose.
	protected void FFRX_ConsumeSite(float currentBuildValue)
	{
		if (!m_aFFRX_Props || m_iToBuildValue <= 0)
			return;

		float ratio = currentBuildValue / m_iToBuildValue;
		ratio = Math.Clamp(ratio, 0, 1);

		int total = FFRX_BuildMaterials.Count(m_iFFRX_Supplies);
		int shouldRemain = Math.Round(total * (1 - ratio));

		for (int i = m_aFFRX_Props.Count() - 1; i >= shouldRemain; i--)
		{
			if (m_aFFRX_Props[i])
				SCR_EntityHelper.DeleteEntityAndChildren(m_aFFRX_Props[i]);
			m_aFFRX_Props.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A chaque coup de pelle : le batiment sort de terre un peu plus.
	protected void FFRX_OnProgress(int currentBuildValue)
	{
		if (m_iToBuildValue <= 0)
			return;

		if (FFRX_IsWall())
		{
			FFRX_WallProgress(currentBuildValue);
			return;
		}

		// L'avance part dans la sauvegarde (ecriture groupee, cf. FFRX_BuildSiteStore).
		if (m_sFFRX_Key != "")
			FFRX_BuildSiteStore.Note(m_sFFRX_Key, m_sFFRX_Target, m_sFFRX_Visual, m_vFFRX_Pos, m_vFFRX_Angles,
				m_iToBuildValue, currentBuildValue, m_iFFRX_Supplies);

		float ratio = currentBuildValue / m_iToBuildValue;
		ratio = Math.Clamp(ratio, 0, 1);

		FFRX_ConsumeSite(currentBuildValue);
	}

	//------------------------------------------------------------------------------------------------
	//! Avancement d'un chantier de mur : on pose les panneaux manquants.
	protected void FFRX_WallProgress(int currentBuildValue)
	{
		if (m_sFFRX_Key != "")
			FFRX_BuildSiteStore.NoteWall(m_sFFRX_Key, m_vFFRX_Pos, m_vFFRX_WallDir, m_fFFRX_WallLen,
				m_iToBuildValue, currentBuildValue, m_iFFRX_Supplies);

		int total = FFRX_WallBuilder.PieceCount(m_fFFRX_WallLen);
		if (total <= 0)
			return;

		float ratio = currentBuildValue / m_iToBuildValue;
		ratio = Math.Clamp(ratio, 0, 1);

		int want = Math.Floor(ratio * total);
		if (want <= m_iFFRX_WallDone)
			return;

		// On ne pose QUE les manquants : rejouer tout le plan a chaque coup de pelle
		// empilerait les panneaux les uns sur les autres.
		FFRX_WallBuilder.BuildRange(m_vFFRX_Pos, m_vFFRX_WallDir, m_fFFRX_WallLen, m_iFFRX_WallDone, want);
		m_iFFRX_WallDone = want;
	}

	//------------------------------------------------------------------------------------------------
	//! Reprend un chantier de mur apres redemarrage, a l'avance ou il en etait.
	void FFRX_RestoreWall(vector start, vector dir, float total, int toBuildValue, float current,
	                      int supplies, string key)
	{
		FFRX_InitWallSite(start, dir, total, toBuildValue, supplies, key);

		float v = Math.Clamp(current, 0, m_iToBuildValue - 1);
		SetBuildingValue(v);
		FFRX_WallProgress(v);
	}

	//------------------------------------------------------------------------------------------------
	//! Abandonne le chantier et rend les supplies. Serveur uniquement.
	//!
	//! Le remboursement est INTEGRAL : FF preleve la totalite du cout a la pose, et un
	//! chantier n'a encore rien consomme. Rembourser au prorata ferait payer une taxe a
	//! qui corrige une erreur de placement -- ce n'est pas le comportement qu'on veut
	//! encourager. (Quand les materiaux arriveront (D1/D3), c'est la qu'un abattement du
	//! type "50 % au demontage" aura un sens : il y aura vraiment de la matiere perdue.)
	void FFRX_CancelSite()
	{
		if (!Replication.IsServer())
			return;

		if (m_iFFRX_Supplies > 0)
		{
			// On retrouve la zone par son test d'appartenance plutot que de garder une
			// reference : apres un redemarrage la reference serait morte, alors que la
			// position, elle, est sauvegardee.
			JWK_BuildAreaControllerComponent ctrl = FFRX_BuildSite.FindController(m_vFFRX_Pos);
			if (ctrl)
			{
				JWK_LogisticsStorageControllerComponent storage = ctrl.GetLogisticsStorage();
				if (storage)
					storage.AddResources(JWK_ELogisticsResourceType.SUPPLIES, m_iFFRX_Supplies);
			}
			else
			{
				Print("[FFRX][Chantier] demontage : aucune zone trouvee, supplies NON rembourses.", LogLevel.WARNING);
			}
		}

		Print(string.Format("[FFRX][Chantier] demonte : %1 (+%2 supplies rendus).",
			m_sFFRX_Target, m_iFFRX_Supplies), LogLevel.NORMAL);

		if (m_sFFRX_Key != "")
		{
			FFRX_BuildSiteStore.Forget(m_sFFRX_Key);
			m_sFFRX_Key = "";
		}

		m_sFFRX_Target = "";
		GetOnAddBuildingValueInt().Remove(FFRX_OnProgress);

		FFRX_ClearSite();

		SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	//! Est-ce un chantier FFRX en cours ? (pour l'action de demontage)
	bool FFRX_IsActiveSite()
	{
		return FFRX_IsOurSite();
	}

	//------------------------------------------------------------------------------------------------
	//! 100 % : le vrai batiment remplace le chantier.
	override void SpawnComposition()
	{
		if (FFRX_IsWall())
		{
			// Dernier coup de pelle : on pose ce qui manque, puis le chantier s'efface.
			int total = FFRX_WallBuilder.PieceCount(m_fFFRX_WallLen);
			if (total > m_iFFRX_WallDone)
				FFRX_WallBuilder.BuildRange(m_vFFRX_Pos, m_vFFRX_WallDir, m_fFFRX_WallLen, m_iFFRX_WallDone, total);

			Print(string.Format("[FFRX][Mur] termine : %1 m, %2 pieces.", (int)m_fFFRX_WallLen, total), LogLevel.NORMAL);

			// Le materiel a ete consomme par le mur : les tas disparaissent avec lui.
			FFRX_ClearSite();

			m_fFFRX_WallLen = 0;
			GetOnAddBuildingValueInt().Remove(FFRX_OnProgress);

			if (m_sFFRX_Key != "")
			{
				FFRX_BuildSiteStore.Forget(m_sFFRX_Key);
				m_sFFRX_Key = "";
			}

			SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
			return;
		}

		if (m_sFFRX_Target == "")
		{
			super.SpawnComposition();   // chantier du jeu de base, pas le notre
			return;
		}

		ResourceName target = m_sFFRX_Target;
		vector pos    = m_vFFRX_Pos;
		vector angles = m_vFFRX_Angles;

		// Couper le lien AVANT de spawner : le chemin FF peut retomber ici, et un
		// chantier qui se relance produirait deux batiments.
		m_sFFRX_Target = "";
		GetOnAddBuildingValueInt().Remove(FFRX_OnProgress);

		if (m_sFFRX_Key != "")
		{
			FFRX_BuildSiteStore.Forget(m_sFFRX_Key);
			m_sFFRX_Key = "";
		}

		FFRX_ClearSite();

		// `s_bPending` est faux a cet instant : l'appel passe donc par le super de notre
		// override et pose bien le BATIMENT, pas un second chantier.
		JWK_ConstructionManagerComponent mgr = JWK_ConstructionManagerComponent.GetInstance();
		if (mgr)
			mgr.SpawnBuildItem(target, pos, angles);

		Print(string.Format("[FFRX][Chantier] termine : %1.", target), LogLevel.NORMAL);

		SCR_EntityHelper.DeleteEntityAndChildren(GetOwner());
	}
}

// ======================================================================================
//  4. PERSISTANCE -- un chantier survit au redemarrage
// ======================================================================================
//
// POURQUOI IL FAUT L'ECRIRE NOUS-MEMES. FF conserve ses batiments parce que LEURS PREFABS
// portent la persistance EPF. Le notre derive de CompositionLayoutBase (jeu de base) et
// n'en a aucune : au redemarrage le chantier disparaissait purement et simplement, en
// emportant les supplies deja preleves. `m_aBuildItems` du controleur de zone n'aide pas,
// c'est une liste d'execution (des RplId), rien n'y est sauvegarde.
//
// On reprend donc le patron deja eprouve chez nous pour les identites civiles : un JSON
// dans le profil, ecrit quand l'etat change, relu a l'amorcage. Pas d'EPF, pas de
// composant a greffer sur un prefab du jeu de base.
//
// CE QUI EST SAUVE : de quoi RECONSTRUIRE le chantier, pas l'entite elle-meme -- batiment
// cible, apparence, position, orientation, total a atteindre, avance, et le cout en
// supplies (indispensable pour rembourser un demontage apres redemarrage).

class FFRX_BuildSiteEntry
{
	string key;
	string target;
	string visual;
	float  x, y, z;
	float  yaw;
	int    toBuild;
	float  current;
	int    supplies;
	float  wallLen;   // > 0 = chantier de MUR (target vaut alors "WALL")
}

class FFRX_BuildSiteFile
{
	ref array<ref FFRX_BuildSiteEntry> sites;
	void FFRX_BuildSiteFile() { sites = {}; }
}

class FFRX_BuildSiteStore
{
	protected static const string SAVE_PATH = "$profile:FFRX_buildsites.json";

	//! Ecriture groupee : un coup de pelle change l'avance, et il y en a des dizaines.
	//! Sauver a chaque coup martyriserait le disque pour rien.
	protected static const int FLUSH_MS = 10000;

	protected static bool s_bStarted;
	protected static bool s_bDirty;
	protected static int  s_iNextKey;
	protected static ref map<string, ref FFRX_BuildSiteEntry> s_mSites;

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_bStarted)
			return;

		s_bStarted = true;
		if (!s_mSites)
			s_mSites = new map<string, ref FFRX_BuildSiteEntry>();

		Load();
		Restore();

		GetGame().GetCallqueue().CallLater(Flush, FLUSH_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	static string NewKey()
	{
		s_iNextKey++;
		return string.Format("s%1", s_iNextKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Enregistre ou met a jour un chantier.
	static void Note(string key, ResourceName target, ResourceName visual, vector pos, vector angles,
	                 int toBuild, float current, int supplies)
	{
		if (!s_mSites)
			s_mSites = new map<string, ref FFRX_BuildSiteEntry>();

		FFRX_BuildSiteEntry e = s_mSites.Get(key);
		if (!e)
		{
			e = new FFRX_BuildSiteEntry();
			e.key = key;
			s_mSites.Set(key, e);
		}

		e.target   = target;
		e.visual   = visual;
		e.x        = pos[0];
		e.y        = pos[1];
		e.z        = pos[2];
		e.yaw      = angles[1];
		e.toBuild  = toBuild;
		e.current  = current;
		e.supplies = supplies;

		s_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Variante MUR : on sauve les trois parametres qui permettent de REJOUER le plan
	//! (depart, direction, longueur) plutot que la liste des panneaux. Le plan etant
	//! deterministe, c'est suffisant -- et ca evite de serialiser N pieces.
	//! `target` sert de marqueur de mode : la chaine "WALL" au lieu d'un prefab.
	static void NoteWall(string key, vector start, vector dir, float total,
	                     int toBuild, float current, int supplies)
	{
		if (!s_mSites)
			s_mSites = new map<string, ref FFRX_BuildSiteEntry>();

		FFRX_BuildSiteEntry e = s_mSites.Get(key);
		if (!e)
		{
			e = new FFRX_BuildSiteEntry();
			e.key = key;
			s_mSites.Set(key, e);
		}

		e.target   = "WALL";
		e.visual   = "";
		e.x        = start[0];
		e.y        = start[1];
		e.z        = start[2];
		// La direction tient dans le champ d'angle : un mur est horizontal, son cap suffit.
		e.yaw      = Math.Atan2(dir[0], dir[2]) * Math.RAD2DEG;
		e.toBuild  = toBuild;
		e.current  = current;
		e.supplies = supplies;
		e.wallLen  = total;

		s_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le chantier n'existe plus : termine, ou demonte.
	static void Forget(string key)
	{
		if (!s_mSites)
			return;
		if (!s_mSites.Contains(key))
			return;

		s_mSites.Remove(key);
		s_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Flush()
	{
		if (!s_bDirty)
			return;
		if (!s_mSites)
			return;

		FFRX_BuildSiteFile f = new FFRX_BuildSiteFile();
		foreach (string k, FFRX_BuildSiteEntry e : s_mSites)
			f.sites.Insert(e);

		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", f);
		ctx.SaveToFile(SAVE_PATH);
		s_bDirty = false;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Load()
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile(SAVE_PATH))
			return;

		FFRX_BuildSiteFile f = new FFRX_BuildSiteFile();
		if (!ctx.ReadValue("", f))
			return;
		if (!f.sites)
			return;

		foreach (FFRX_BuildSiteEntry e : f.sites)
		{
			if (!e || e.key == "")
				continue;
			s_mSites.Set(e.key, e);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Repose les chantiers interrompus, a l'avance ou ils en etaient.
	protected static void Restore()
	{
		if (!s_mSites || s_mSites.Count() == 0)
			return;

		Resource res = Resource.Load(FFRX_BuildSite.SITE_PREFAB);
		if (!res || !res.IsValid())
		{
			Print("[FFRX][Chantier] restauration impossible : prefab de chantier introuvable.", LogLevel.ERROR);
			return;
		}

		int restored = 0;
		array<string> dead = {};

		foreach (string k, FFRX_BuildSiteEntry e : s_mSites)
		{
			// Une cle plus haute que celles relues eviterait de reutiliser un identifiant
			// encore present dans le fichier.
			string digits = k.Substring(1, k.Length() - 1);
			int n = digits.ToInt();
			if (n > s_iNextKey)
				s_iNextKey = n;

			if (e.toBuild <= 0 || e.target == "")
			{
				dead.Insert(k);
				continue;
			}

			vector pos    = Vector(e.x, e.y, e.z);
			vector angles = Vector(0, e.yaw, 0);

			// Le chantier de mur s'ancre au MILIEU du mur, alors que `pos` est son DEPART :
			// c'est la position de depart qu'on sauve, car c'est elle qui sert a rejouer le
			// plan. On decale donc l'entite de chantier a mi-longueur.
			vector spawnAt = pos;
			if (e.wallLen > 0)
			{
				vector d = Vector(Math.Sin(e.yaw * Math.DEG2RAD), 0, Math.Cos(e.yaw * Math.DEG2RAD));
				spawnAt = pos + d * (e.wallLen * 0.5);
				spawnAt[1] = GetGame().GetWorld().GetSurfaceY(spawnAt[0], spawnAt[2]);
			}

			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			Math3D.AnglesToMatrix(angles, params.Transform);
			params.Transform[3] = spawnAt;

			IEntity site = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
			if (!site)
			{
				dead.Insert(k);
				continue;
			}

			SCR_CampaignBuildingLayoutComponent layout =
				SCR_CampaignBuildingLayoutComponent.Cast(site.FindComponent(SCR_CampaignBuildingLayoutComponent));

			if (!layout)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(site);
				dead.Insert(k);
				continue;
			}

			if (e.wallLen > 0)
			{
				// Chantier de MUR : on rejoue le plan depuis ses trois parametres.
				vector dir = Vector(Math.Sin(e.yaw * Math.DEG2RAD), 0, Math.Cos(e.yaw * Math.DEG2RAD));
				layout.FFRX_RestoreWall(pos, dir, e.wallLen, e.toBuild, e.current, e.supplies, k);
			}
			else
			{
				layout.FFRX_InitSite(e.target, e.visual, e.toBuild, pos, angles, e.supplies, k);
				layout.FFRX_RestoreProgress(e.current);
			}

			restored++;
		}

		foreach (string k : dead)
			s_mSites.Remove(k);

		if (restored > 0)
			Print(string.Format("[FFRX][Chantier] %1 chantier(s) repris apres redemarrage.", restored), LogLevel.NORMAL);
	}
}


// ======================================================================================
//  4 bis. LE GARDE QUI MANQUE DANS LE JEU DE BASE
// ======================================================================================
//
// BUG AMONT (jeu de base, pas FF). SCR_CampaignBuildingBuildUserAction.ProcesXPreward :
//
//     SCR_CampaignBuildingManagerComponent mgr = ...gameMode.FindComponent(...);
//     mgr.ProcesXPreward();        // <-- aucun test de nullite
//
// Ce composant appartient au mode de jeu CONFLICT. Le mode FF ne le porte pas, donc le
// pointeur est nul et l'appel jette une exception VM a CHAQUE coup de pelle -- constate en
// jeu le 2026-09-19, des dizaines de fois par bunker.
//
// L'exception ne casse rien de visible (elle part apres AddBuildingValue, le chantier
// avance quand meme), mais elle noie le log et coute du temps a chaque frame d'action.
//
// On remet simplement le garde absent. Aucun effet de bord : chez nous cette recompense
// n'a de toute facon rien a distribuer -- l'XP vient du temps de jeu compte par Fleet
// (cf. memoire ff-no-base-xp-awarder : FF n'a aucun SCR_XPHandlerComponent).

modded class SCR_CampaignBuildingBuildUserAction
{
	override void ProcesXPreward()
	{
		BaseGameMode gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return;

		SCR_CampaignBuildingManagerComponent mgr =
			SCR_CampaignBuildingManagerComponent.Cast(gameMode.FindComponent(SCR_CampaignBuildingManagerComponent));

		if (!mgr)
			return;   // le garde qui manque en amont

		mgr.ProcesXPreward();
	}
}

// ======================================================================================
//  5. DEMONTER UN CHANTIER EN COURS
// ======================================================================================
//
// ON N'AJOUTE PAS D'ACTION. CompositionLayoutBase en porte deja une
// (SCR_CampaignBuildingDisassemblyUserAction), et elle est remappee dans le MEME contexte
// "userAction" que l'action de construction. Une action a nous en plus donnait donc DEUX
// entrees "Demonter" dans le menu -- constate en jeu le 2026-09-19. On surcharge celle qui
// existe.
//
// SA DUREE EST UN MAINTIEN SIMPLE. Releve dans FreeRoamBuilding_Action_Disassemble.conf :
// `Duration 20`, valeur POSITIVE, sans PerformPerFrame -- on maintient 20 s et l'action
// part une fois. Rien a voir avec l'accumulation par coups de l'action de construction.
//
// CONSEQUENCE, ET C'EST UNE LIMITE ASSUMEE : la duree ne peut PAS dependre du batiment.
// `GetActionDuration()` est `proto external` cote moteur, non surchargeable, et il n'existe
// aucun setter. On fixe donc 10 s dans le prefab -- la moitie des 20 s vanilla. Un
// demontage proportionnel exigerait de repasser sur une action en boucle, donc de
// reintroduire le doublon qu'on vient de supprimer.

modded class SCR_CampaignBuildingDisassemblyUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		SCR_CampaignBuildingLayoutComponent layout;
		if (pOwnerEntity)
			layout = SCR_CampaignBuildingLayoutComponent.Cast(
				pOwnerEntity.FindComponent(SCR_CampaignBuildingLayoutComponent));

		// Pas un de nos chantiers : comportement d'origine, on ne touche a rien.
		if (!layout || !layout.FFRX_IsActiveSite())
		{
			super.PerformAction(pOwnerEntity, pUserEntity);
			return;
		}

		if (!Replication.IsServer())
			return;

		// Reserve au genie, comme la pose : pouvoir defaire le chantier d'un autre serait
		// une porte ouverte au sabotage entre joueurs. L'autorite tranche ici, le client
		// ne connaissant pas les drapeaux d'escouade.
		int pid = FFRX_RoleBonus.PlayerIdOf(pUserEntity);
		if (pid <= 0 || !FFRX_GroupsManager.IsGenie(pid))
		{
			Print("[FFRX][Chantier] demontage refuse : demandeur hors du genie.", LogLevel.NORMAL);
			return;
		}

		layout.FFRX_CancelSite();
	}
}
