// FF - REMIXED - PVE
// Guerre asymetrique -- IED DISPERSES SUR LES POINTS D'INTERET DE LA CARTE.
//
// ======================================================================================
//  L'INTENTION (Benji, 2026-09-20)
// ======================================================================================
// Nos menaces asymetriques sont toutes REACTIVES : elles apparaissent autour du joueur au
// moment ou il joue (kamikaze, voiture beliere, voiture piegee du coin). Le joueur finit
// par comprendre que le danger le SUIT, donc qu'un endroit ou il n'y a rien n'est sur.
//
// Ici c'est l'inverse : les IED sont poses A L'AVANCE, une fois pour toutes, sur les points
// d'interet de la carte. Le danger devient une propriete DU LIEU, pas du joueur. Approcher
// un site inconnu redevient tendu, et cette tension existe meme quand il ne se passe rien.
//
// L'idee de depart etait un sac d'IED en butin : ecartee par Benji, "ca sert a rien" -- le
// joueur n'a pas besoin de POSER des pieges, il a besoin d'en CRAINDRE.
//
// ======================================================================================
//  OU : LES ANCRES DE LA CARTE, PAS DES POSITIONS AU HASARD
// ======================================================================================
// FF seme sur chaque carte des `JWK_GenericWorldSlotComponent` : des emplacements
// pre-qualifies (avec des drapeaux IN_FOREST / OPEN_FIELD / ROAD_ACCESS / REMOTE_SITE /
// HIDDEN_SITE / ELEVATED / SKY_VIEW) dont il se sert pour poser ses camps dynamiques.
// C'est exactement ce qu'on cherche : des lieux que le level design a deja designes comme
// interessants, repartis sur toute la carte. On les LIT sans les consommer -- on ne touche
// pas a `IsUsed()`, donc FF continue d'y placer ses camps comme avant.
//
// On y ajoute les checkpoints ennemis (`JWK_CheckpointEntity`, indexe aussi).
//
// Le drapeau ROAD_ACCESS choisit le type de piege : accessible en vehicule -> IED ENTERRE
// (rayon 0,4 m, il faut rouler ou marcher dessus). Sinon -> poubelle / marmite / carton,
// qui se declenchent a 5-7 m et conviennent a une fouille a pied.
//
// ======================================================================================
//  CE QU'ON NE FAIT PAS, ET POURQUOI
// ======================================================================================
// PAS en territoire tenu par la resistance (role PLAYER). Un IED sous nos propres pieds a
// la FOB n'est pas du suspense, c'est une punition arbitraire. Territoire ennemi, ligne de
// front et zones neutres : oui -- l'ennemi a eu le temps de les poser.
//
// PAS de re-semis permanent. On pose au demarrage du monde, puis on complete lentement
// (RESEED_MS) ce qui a explose, jusqu'au plafond. Sans ce complement, une carte jouee
// longtemps redevient inerte ; avec un re-semis rapide, le joueur comprend que ca repousse
// et l'effet de lieu disparait.
//
// ILS NE SAUTENT QUE SOUS LE CAMP RESISTANCE (role PLAYER ou SUPPORTING). L'IA ennemie --
// a qui ces IED "appartiennent" -- et les civils sont epargnes. Voir la classe moddee en bas
// de ce fichier pour le comment et le pourquoi.
//
// ======================================================================================
//  DEPENDANCE : IED Emporium 2.0 (Workshop 5DD55EE55380FA5C)
// ======================================================================================
// Ce mod etait DEJA declare en dependance de REMIXED et n'etait utilise NULLE PART
// (verifie le 2026-09-20 : zero reference dans les .c et les .conf). On payait son cout de
// compilation pour rien. Ce fichier est la premiere chose qui s'en sert.
//
// Les prefabs employes sont les versions "editables" (`PrefabsEditable/.../IEDs/E_*.et`) :
// des `GMFX_MineTriggerEntity` AUTONOMES -- declencheur a pression, explosion, degats et
// modele 3D inclus. Rien a coder autour, on les fait apparaitre et ils fonctionnent.
//
// Si le mod est absent, `Resource.Load` echoue : on le dit une fois et on s'arrete, sans
// spam et sans casser le reste.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict). Amorce depuis FFRX_Boot.c.

class FFRX_IEDScatterTuning
{
	//! Delai avant le premier semis. L'index du monde et le controle de territoire ne sont
	//! pas prets a l'init -- meme precaution que pour le reseau routier (cf. CLAUDE.md).
	static const int  FIRST_SEED_MS = 45000;

	//! Cadence du complement. Lent VOLONTAIREMENT : voir l'en-tete.
	static const int  RESEED_MS     = 900000;   // 15 min

	//! Ecart minimum entre deux IED, pour ne pas en grouper trois sur le meme site.
	static const float MIN_SPACING  = 60.0;

	//! Distance de securite autour d'un joueur au moment du semis : on ne fait pas
	//! apparaitre un piege sous les yeux de quelqu'un.
	static const float PLAYER_CLEAR = 150.0;

	// --- Les prefabs (IED Emporium 2.0) ---
	//! Sur un site accessible en vehicule : rayon 0,4 m, il faut rouler/marcher dessus.
	static const ResourceName IED_BURIED     = "{42CF6F4B6FB30484}PrefabsEditable/Auto/Props/IEDs/E_Buried_IED.et";
	//! Fouille a pied : rayons 5-7 m, modeles urbains credibles.
	static const ResourceName IED_BIN        = "{311494B0C547FBB2}PrefabsEditable/Auto/Props/IEDs/E_Bin_IED.et";
	static const ResourceName IED_BIN_LOW    = "{F8AD1B7E13425B91}PrefabsEditable/Auto/Props/IEDs/E_Bin_IED_LowYield.et";
	static const ResourceName IED_POT        = "{1A7FA8B33DE0DF1A}PrefabsEditable/Auto/Props/IEDs/E_CookPot_IED.et";
	static const ResourceName IED_POT_SMALL  = "{B80D6F612526BDDF}PrefabsEditable/Auto/Props/IEDs/E_CookPot_Small_LowRadius.et";
	static const ResourceName IED_BOX_LOW    = "{3A9331B21EC6CE79}PrefabsEditable/Auto/Props/IEDs/E_Box_IED_LowYield.et";
}

// ---------------------------------------------------------------------------
class FFRX_IEDScatter
{
	protected static ref FFRX_IEDScatter s_Instance;

	//! Les IED poses. Sert au plafond et a l'espacement ; on ne les supprime jamais nous-memes
	//! (un IED qui disparait tout seul n'a aucun sens), on nettoie juste les entrees mortes.
	protected ref array<IEntity> m_aPlaced = {};

	//! Mis a true si le mod IED Emporium n'est pas charge : on cesse d'essayer.
	protected bool m_bUnavailable;

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;

		s_Instance = new FFRX_IEDScatter();
		GetGame().GetCallqueue().CallLater(s_Instance.Seed, FFRX_IEDScatterTuning.FIRST_SEED_MS, false);
		GetGame().GetCallqueue().CallLater(s_Instance.Seed, FFRX_IEDScatterTuning.RESEED_MS, true);
		Print("[FFRX][IED] Dispersion sur les points d'interet : semis programme.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Plafond d'IED simultanes sur la carte. Reglage FF "IED sur les lieux : max".
	protected static int MaxPlaced()
	{
		JWK_GameSettingsCache c = JWK.GameSettingsCache();
		if (!c)
			return 0;

		return Math.Round(c.m_fFFRX_IedScatterMax);
	}

	//------------------------------------------------------------------------------------------------
	//! Pose ce qui manque pour atteindre le plafond. Appele au demarrage puis en complement.
	protected void Seed()
	{
		if (m_bUnavailable)
			return;

		int cap = MaxPlaced();
		if (cap <= 0)
			return; // reglage a 0 = mecanique desactivee

		Prune();

		int missing = cap - m_aPlaced.Count();
		if (missing <= 0)
			return;

		array<vector> anchors = {};
		array<bool> road = {};
		CollectAnchors(anchors, road);

		if (anchors.IsEmpty())
		{
			Print("[FFRX][IED] Aucun point d'interet trouve (index pas encore peuple ?).", LogLevel.WARNING);
			return;
		}

		// Melange : sans ca on poserait toujours sur les memes premiers slots de l'index,
		// donc toujours au meme endroit de la carte d'une partie a l'autre.
		// Les deux tableaux sont permutes ENSEMBLE pour rester alignes.
		for (int i = anchors.Count() - 1; i > 0; i--)
		{
			int j = Math.RandomInt(0, i + 1);

			vector tmp = anchors[i];
			anchors[i] = anchors[j];
			anchors[j] = tmp;

			bool tmpR = road[i];
			road[i] = road[j];
			road[j] = tmpR;
		}

		int placed = 0;
		for (int i = 0; i < anchors.Count(); i++)
		{
			if (placed >= missing)
				break;

			if (TryPlaceAt(anchors[i], road[i]))
				placed = placed + 1;
		}

		Print(string.Format("[FFRX][IED] Semis : %1 pose(s), %2 au total sur la carte (plafond %3, %4 lieux candidats).",
			placed, m_aPlaced.Count(), cap, anchors.Count()), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Les lieux candidats : les ancres de placement de FF + les checkpoints ennemis.
	//!
	//! `outRoad` est rempli en PARALLELE de `outList` (meme index) : le drapeau ROAD_ACCESS
	//! ne se lit que sur le composant, et le relire plus tard obligerait a reparcourir tout
	//! l'index pour chaque pose. On le capture donc ici, une fois.
	protected void CollectAnchors(notnull array<vector> outList, array<bool> outRoad = null)
	{
		JWK_IndexSystem idx = JWK_IndexSystem.Get();
		if (!idx)
			return;

		// Ancres generiques du level design. On lit leur position sans les reserver.
		array<GenericComponent> slots = idx.GetAllGC(JWK_GenericWorldSlotComponent);
		foreach (GenericComponent gc : slots)
		{
			// Le cast est OBLIGATOIRE : GenericComponent n'expose pas GetOwner().
			JWK_GenericWorldSlotComponent slot = JWK_GenericWorldSlotComponent.Cast(gc);
			if (!slot)
				continue;

			IEntity owner = slot.GetOwner();
			if (!owner)
				continue;

			outList.Insert(owner.GetOrigin());

			if (outRoad)
				outRoad.Insert((slot.m_iFlags & JWK_EGenericWorldSlotFlags.ROAD_ACCESS) != 0);
		}

		// Checkpoints ennemis : des lieux que le joueur approche forcement. Un checkpoint est
		// sur une route par definition -> drapeau routier a true.
		array<EntityID> cps = idx.GetAll(JWK_CheckpointEntity);
		foreach (EntityID id : cps)
		{
			IEntity e = GetGame().GetWorld().FindEntityByID(id);
			if (!e)
				continue;

			outList.Insert(e.GetOrigin());

			if (outRoad)
				outRoad.Insert(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool TryPlaceAt(vector anchor, bool roadAccess)
	{
		if (!AllowedHere(anchor))
			return false;

		if (TooClose(anchor))
			return false;

		if (PlayerNear(anchor))
			return false;

		// On decale un peu : pose pile sur l'ancre, l'IED serait au centre exact du futur
		// camp de FF, ce qui est a la fois voyant et genant.
		float ang = Math.RandomFloat(0, Math.PI2);
		float off = Math.RandomFloat(3, 12);

		vector p = anchor;
		p[0] = p[0] + Math.Cos(ang) * off;
		p[2] = p[2] + Math.Sin(ang) * off;

		vector pos = p;
		if (!SCR_WorldTools.FindEmptyTerrainPosition(pos, p, 15))
			return false;

		ResourceName prefab = PickType(roadAccess);
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			// Une seule fois : soit le mod IED Emporium n'est pas charge, soit un GUID a
			// bouge avec une mise a jour du mod. Dans les deux cas, insister ne sert a rien.
			m_bUnavailable = true;
			Print("[FFRX][IED] Prefab introuvable (" + prefab + ") -- mod IED Emporium absent ou GUID change. Dispersion desactivee.", LogLevel.WARNING);
			return false;
		}

		EntitySpawnParams sp = new EntitySpawnParams();
		sp.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(sp.Transform);
		sp.Transform[3] = pos;

		IEntity ied = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), sp);
		if (!ied)
			return false;

		m_aPlaced.Insert(ied);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Choix du type selon le lieu : sur un site routier, un IED enterre qu'il faut ecraser ;
	//! ailleurs, un objet de fouille a plus grand rayon.
	protected ResourceName PickType(bool roadAccess)
	{
		if (roadAccess)
			return FFRX_IEDScatterTuning.IED_BURIED;

		int r = Math.RandomInt(0, 5);
		if (r == 0)
			return FFRX_IEDScatterTuning.IED_BIN;
		if (r == 1)
			return FFRX_IEDScatterTuning.IED_BIN_LOW;
		if (r == 2)
			return FFRX_IEDScatterTuning.IED_POT;
		if (r == 3)
			return FFRX_IEDScatterTuning.IED_POT_SMALL;

		return FFRX_IEDScatterTuning.IED_BOX_LOW;
	}

	//------------------------------------------------------------------------------------------------
	//! Territoire ennemi, front ou zone neutre. Jamais chez nous (voir l'en-tete).
	protected bool AllowedHere(vector pos)
	{
		JWK_TerritoryControlSystem tc = JWK.GetTerritoryControl();
		if (!tc)
			return false;

		JWK_TerritoryControlNodeComponent node = tc.GetNodeAt(pos);
		if (!node)
			return true; // hors de tout territoire = terrain vague, on autorise

		JWK_EFactionRole role = node.GetFactionRole();
		if (role == JWK_EFactionRole.PLAYER || role == JWK_EFactionRole.SUPPORTING)
			return false;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool TooClose(vector pos)
	{
		float minSq = FFRX_IEDScatterTuning.MIN_SPACING * FFRX_IEDScatterTuning.MIN_SPACING;

		foreach (IEntity e : m_aPlaced)
		{
			if (!e)
				continue;

			if (vector.DistanceSq(e.GetOrigin(), pos) < minSq)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlayerNear(vector pos)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;

		float clearSq = FFRX_IEDScatterTuning.PLAYER_CLEAR * FFRX_IEDScatterTuning.PLAYER_CLEAR;

		array<int> ids = {};
		pm.GetPlayers(ids);

		foreach (int pid : ids)
		{
			IEntity pe = pm.GetPlayerControlledEntity(pid);
			if (!pe)
				continue;

			if (vector.DistanceSq(pe.GetOrigin(), pos) < clearSq)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire les entrees dont l'IED a explose ou a ete supprime, pour que le complement
	//! puisse repasser derriere.
	protected void Prune()
	{
		for (int i = m_aPlaced.Count() - 1; i >= 0; i--)
		{
			IEntity e = m_aPlaced[i];
			if (!e || e.IsDeleted())
				m_aPlaced.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! true si cette entite est un de NOS IED disperses. Sert au filtre de faction : tout ce
	//! qui n'est pas a nous garde le comportement d'origine du mod.
	static bool IsOurs(IEntity ent)
	{
		if (!ent || !s_Instance)
			return false;

		if (s_Instance.m_aPlaced.Find(ent) != -1)
			return true;

		return Foreign().Find(ent) != -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Declare une charge posee par un AUTRE systeme de REMIXED (aujourd'hui les carcasses
	//! piegees, cf. FFRX_TrappedWrecks.c) pour qu'elle beneficie du meme filtre de faction.
	//!
	//! On centralise ici plutot que de dupliquer le modded `GMFX_MinePressureTriggerComponent` :
	//! une classe ne peut etre moddee qu'une fois par addon, et surtout la regle "seul le camp
	//! resistance declenche" doit rester definie a un seul endroit.
	//!
	//! Le registre sert AUSSI de garde-fou : une charge non enregistree garde le comportement
	//! d'origine du mod, donc un piege pose a la main par un Game Master n'est jamais filtre.
	//!
	//! ⚠️ REGISTRE SEPARE, ET C'EST IMPORTANT : ces charges ne doivent PAS entrer dans
	//! `m_aPlaced`, qui sert au plafond ("IED sur les lieux : max") et a l'espacement de 60 m.
	//! Une carcasse piegee consommerait alors un IED disperse et fausserait le reglage -- deux
	//! mecaniques distinctes, deux comptages distincts. Ce registre-ci ne sert qu'au filtre.
	//! `static` car il doit survivre meme si la dispersion d'IED est reglee a 0.
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<IEntity> s_aForeign;

	protected static array<IEntity> Foreign()
	{
		if (!s_aForeign)
			s_aForeign = new array<IEntity>();

		return s_aForeign;
	}

	static void RegisterForeign(IEntity charge)
	{
		if (!charge)
			return;

		if (Foreign().Find(charge) == -1)
			Foreign().Insert(charge);
	}

	//------------------------------------------------------------------------------------------------
	//! Etat, pour la commande #ied.
	static string Report()
	{
		if (!s_Instance)
			return "Dispersion d'IED : pas demarree.";

		s_Instance.Prune();

		if (s_Instance.m_bUnavailable)
			return "Dispersion d'IED : DESACTIVEE (mod IED Emporium absent ou GUID change).";

		array<vector> anchors = {};
		s_Instance.CollectAnchors(anchors);

		return "IED sur les lieux : " + s_Instance.m_aPlaced.Count().ToString()
			+ " poses / plafond " + MaxPlaced().ToString()
			+ "  --  " + anchors.Count().ToString() + " lieux candidats sur la carte.";
	}

	//------------------------------------------------------------------------------------------------
	//! Force un semis immediat (commande #ied).
	static void ForceSeed()
	{
		if (!s_Instance)
			return;

		s_Instance.Seed();
	}
}

// ---------------------------------------------------------------------------
//  IMMUNITE DE FACTION : NOS IED NE SAUTENT QUE SOUS LA RESISTANCE
// ---------------------------------------------------------------------------
// Sans ca, un IED pose sur un lieu ennemi finit par tuer la garnison qui l'entoure : au bout
// d'un moment la carte se vide toute seule et le piege n'a jamais servi contre personne.
//
// POURQUOI LE FILTRE EXISTANT NE S'APPLIQUE PAS ICI. On a DEJA cette immunite, dans
// `Mines/FFRX_MineTrigger.c`, pose sur la classe DE BASE `SCR_PressureTriggerComponent`. Mais
// `GMFX_MinePressureTriggerComponent` (GameMasterFX) surcharge `EOnContact` et **n'appelle
// jamais super** : notre filtre est donc court-circuite pour tous les IED du mod. Il faut
// intervenir sur la classe DERIVEE, ici.
//
// En revanche on REUTILISE `FFRX_IsResistanceSide` : elle est definie dans notre modded
// `SCR_PressureTriggerComponent`, dont ce composant herite. (Tentative de la redefinir ici =
// "Overriding function but not marked as override" -- c'est ce qui a mis la reutilisation sur
// la piste.) Un seul endroit definit donc la regle "qui est du camp resistance".
// A noter : FFMI n'est PAS une dependance de REMIXED (verifie le 2026-09-20) ; c'est bien notre
// propre portage qui fait foi.
//
// POURQUOI MODDER CETTE CLASSE EST SUR : GameMasterFX n'est pas une dependance directe de
// REMIXED, mais IED Emporium -- qui l'est -- en depend (`Dependencies { "58D0..." "5994..." }`).
// GameMasterFX est donc toujours charge quand nous le sommes.
//
// ON NE TOUCHE QU'A NOS IED. Un piege pose a la main par un Game Master, ou une mine de
// GameMasterFX, garde son comportement d'origine : le test `IsOurs` protege tout le reste.
//
// Un vehicule est juge d'apres son PILOTE (vehicule vide = ne declenche pas), comme FFMI.
// ---------------------------------------------------------------------------

modded class GMFX_MinePressureTriggerComponent
{
	override void EOnContact(IEntity owner, IEntity other, Contact contact)
	{
		// Pas un de nos IED disperses -> comportement du mod, intact.
		if (!FFRX_IEDScatter.IsOurs(owner))
		{
			super.EOnContact(owner, other, contact);
			return;
		}

		if (!FFRX_IsResistanceSide(other))
			return;

		super.EOnContact(owner, other, contact);
	}
}

// ---------------------------------------------------------------------------
//  #ied -- etat de la dispersion, et semis force.
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_IEDCommand : ScrServerCommand
{
	override string GetKeyword() { return "ied"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		return Handle(argv);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return Handle(argv);
	}

	protected ScrServerCmdResult Handle(array<string> argv)
	{
		if (argv && argv.Count() >= 2 && argv[1] == "go")
		{
			FFRX_IEDScatter.ForceSeed();
			return ScrServerCmdResult("Semis force. " + FFRX_IEDScatter.Report(), EServerCmdResultType.OK);
		}

		return ScrServerCmdResult(FFRX_IEDScatter.Report(), EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
