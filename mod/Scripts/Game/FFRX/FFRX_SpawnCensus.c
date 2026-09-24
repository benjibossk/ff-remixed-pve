// FF - REMIXED - PVE
// Recensement des SPAWNS : quel type de groupe, quels soldats, quels vehicules apparaissent.
//
// But : repondre a "est-ce que ce qui spawne est representatif / equilibre ?". On ne change
// RIEN au jeu, on compte et on imprime.
//
// Ou on se branche, et pourquoi :
//   1. GROUPES -- override de JWK_AIForce.DoSpawn(), la fonction qui spawne reellement le
//      prefab de groupe. C'est le passage OBLIGATOIRE de FF *et* de Reoccupation (qui
//      partage la file), streaming compris puisque JWK_StreamableAIForce en herite. On
//      ecoutait avant l'event OnAiSpawnRequestDone_S : il n'a jamais rien donne, tout FF
//      s'y abonne en ConnectEventFiltered sur UNE requete precise (cf. le hook en bas).
//   2. VEHICULES -- JWK_AIForceSystem.OnForceCrewedVehicleAttached_S pour les vehicules
//      d'une force FF, plus le vehicule porteur de chaque ennemi recense (les blindes et
//      helicos amenes par DARC ne sont attaches a aucune force FF).
//   3. SOLDATS -- SCR_BaseGameMode.GetOnControllableSpawned() : tout perso qui apparait,
//      quel que soit le producteur. Indispensable pour DARC, qui court-circuite la file FF
//      (il appelle SDRC_AIHelper, une classe `sealed` donc non interceptable). Chaque
//      perso est etiquete "file FF/Reoccupation" ou "hors file FF" selon qu'un groupe
//      vient ou non d'etre annonce par (1).
//
// Lecture des resultats : un tableau est imprime toutes les CENSUS_REPORT_MIN minutes dans
// le log serveur, prefixe [FFRX][Census]. La commande admin #census le sort a la demande.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict, cf. memoire).

//! Echappement JSON. Le parseur de Go refuse les caracteres de controle bruts, et un
//! nom de prefab peut contenir n'importe quoi -- meme regle que FFRX_FFStateSender.Esc().
class FFRX_CensusEsc
{
	static string Esc(string s)
	{
		string txt = "";
		int n = s.Length();
		for (int i = 0; i < n; i++)
		{
			string ch = s.Get(i);
			if (ch == "\\" || ch == "\"")
			{
				txt = txt + "\\" + ch;
				continue;
			}
			txt = txt + ch;
		}
		return txt;
	}
}

//! Callback REST partage. On NE surcharge PAS OnSuccess/OnError : on les branche par
//! SetOnSuccess/SetOnError, sinon le moteur crache "Function was not set for event".
class FFRX_CensusRestCb : RestCallback
{
	void FFRX_CensusRestCb()
	{
		SetOnSuccess(OnOk);
		SetOnError(OnErr);
	}

	// La signature imposee par le prototype `RestCallbackFunc` est UN seul parametre
	// `RestCallback` -- pas (string data) ni (int errorCode). Le code HTTP se lit sur le
	// callback lui-meme. Meme forme que FFRX_FFRestCb dans FFRX_FFStateSender.c.
	void OnOk(RestCallback cb)
	{
		// Silencieux : le rapport est deja dans le log juste au-dessus.
	}

	void OnErr(RestCallback cb)
	{
		int http = 0;
		if (cb)
			http = (int)cb.GetHttpCode();

		Print(string.Format("[FFRX][Census] POST /ffcensus echoue (HTTP %1).", http), LogLevel.WARNING);
	}
}

// ---------------------------------------------------------------------------
class FFRX_CensusTally
{
	ref map<string, int> m_mCounts = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	void Add(string key)
	{
		if (key == "")
			key = "(inconnu)";

		int n = 0;
		m_mCounts.Find(key, n);
		m_mCounts.Set(key, n + 1);
	}

	//------------------------------------------------------------------------------------------------
	int Total()
	{
		int sum = 0;
		foreach (string k, int n : m_mCounts)
			sum += n;
		return sum;
	}

	//------------------------------------------------------------------------------------------------
	//! Entrees JSON "{"n":"<nom>","c":<count>}", concatenees avec '+' (string.Format tronque a 8 Ko).
	//! Le nom passe par Esc : le parseur JSON de Go rejette tout caractere de controle.
	string Json(string category)
	{
		string txt = "";
		foreach (string k, int n : m_mCounts)
		{
			if (txt != "")
				txt = txt + ",";
			txt = txt + "{\"cat\":\"" + category + "\",\"n\":\"" + FFRX_CensusEsc.Esc(k) + "\",\"c\":" + n.ToString() + "}";
		}
		return txt;
	}

	//------------------------------------------------------------------------------------------------
	//! Lignes "  <count>  <pct>%  <nom>", triees du plus frequent au moins frequent.
	//! Concatenation avec '+' : string.Format tronque sa sortie a ~8 Ko.
	string Render(string title)
	{
		int total = Total();
		string txt = "[FFRX][Census] " + title + " -- total " + total.ToString() + "\n";
		if (total <= 0)
			return txt + "  (rien)\n";

		// Tri par insertion sur des tableaux paralleles : les volumes sont petits
		// (quelques dizaines de prefabs distincts), inutile de sortir l'artillerie.
		array<string> names = {};
		array<int> counts = {};
		foreach (string k, int n : m_mCounts)
		{
			int pos = 0;
			while (pos < counts.Count() && counts[pos] >= n)
				pos++;
			names.InsertAt(k, pos);
			counts.InsertAt(n, pos);
		}

		for (int i = 0; i < names.Count(); i++)
		{
			int pct = (counts[i] * 100) / total;
			txt = txt + "  " + counts[i].ToString() + "  " + pct.ToString() + "%  " + names[i] + "\n";
		}
		return txt;
	}
}

// ---------------------------------------------------------------------------
class FFRX_SpawnCensus
{
	protected static ref FFRX_SpawnCensus s_Instance;

	static const int CENSUS_REPORT_MIN = 10;   // periodicite du rapport, en minutes

	// Delai avant de classer un perso qui vient d'apparaitre. Sa faction n'est pas encore
	// posee au moment de l'event ; 5 s est large (FFRX_AIDifficulty se contente d'un
	// re-test toutes les 10 s) et reste court devant le rapport de 10 min.
	static const int CENSUS_CLASSIFY_DELAY_MS = 5000;
	// Fenetre pendant laquelle un perso qui apparait est attribue au dernier groupe sorti
	// de la file FF. Au-dela, on le compte comme venu d'ailleurs.
	static const float CENSUS_QUEUE_ATTRIBUTION_MS = 1000;

	protected ref FFRX_CensusTally m_Groups   = new FFRX_CensusTally();   // prefabs de groupe
	protected ref FFRX_CensusTally m_Soldiers = new FFRX_CensusTally();   // prefabs de perso
	protected ref FFRX_CensusTally m_Vehicles = new FFRX_CensusTally();   // prefabs de vehicule
	protected ref FFRX_CensusTally m_Sources  = new FFRX_CensusTally();   // file FF vs hors file

	protected int m_iGroupSpawns;
	protected int m_iSinceStartSec;

	// Instant du dernier groupe sorti de la file FF : sert a attribuer les persos qui
	// apparaissent juste apres (cf. ClassifyLate).
	protected float m_fLastQueueSpawnTime;
	// Persos apparus qui ne sont PAS ennemis apres classification (allies, civils,
	// joueurs). Publie dans le rapport : un total eleve ici avec SOLDATS a 0 designe le
	// filtre, pas l'absence de spawns.
	protected int m_iNonEnemySpawns;
	// Vehicules deja comptes (cle = EntityID en texte), pour ne pas compter un transport
	// une fois par occupant.
	protected ref map<string, bool> m_mSeenVehicles = new map<string, bool>();

	// Transport vers le site (optionnel : sans GTG.json le recensement reste local aux logs).
	protected string m_sUrl;
	protected string m_sApiKey;
	protected ref FFRX_CensusRestCb m_Cb;

	//------------------------------------------------------------------------------------------------
	//! Remise a zero demandee depuis le site (ordre "census_reset", cf. FFRX_GroupsFleet)
	//! ou par la commande admin "#census reset".
	//!
	//! Indispensable pour comparer un AVANT et un APRES : le recensement cumule depuis le
	//! demarrage du serveur, donc apres un changement de reglage les anciens chiffres
	//! continuent de peser dans les pourcentages -- et vider la table du site ne servirait
	//! a rien, le prochain rapport la reecraserait avec le cumul complet.
	//!
	//! On repart de compteurs neufs et on renvoie IMMEDIATEMENT un rapport vide, pour que
	//! le site reflete la remise a zero sans attendre les 10 minutes suivantes.
	void Reset()
	{
		m_Groups   = new FFRX_CensusTally();
		m_Soldiers = new FFRX_CensusTally();
		m_Vehicles = new FFRX_CensusTally();
		m_Sources  = new FFRX_CensusTally();

		m_iGroupSpawns     = 0;
		m_iSinceStartSec   = 0;
		m_iNonEnemySpawns  = 0;
		m_mSeenVehicles    = new map<string, bool>();

		Print("[FFRX][Census] Remise a zero du recensement.", LogLevel.NORMAL);
		Send();
	}

	//! Point d'entree pour l'ordre venu du site (le singleton peut ne pas exister encore).
	static void ResetFromWeb()
	{
		if (s_Instance)
			s_Instance.Reset();
	}

	//------------------------------------------------------------------------------------------------
	static FFRX_SpawnCensus Get()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;

		s_Instance = new FFRX_SpawnCensus();
		s_Instance.Start();
	}

	//------------------------------------------------------------------------------------------------
	void Start()
	{
		BaseWorld world = GetGame().GetWorld();
		JWK_AIForceSystem sys = JWK_AIForceSystem.Get(world);
		if (sys)
		{
			// Les groupes ne passent PLUS par OnAiSpawnRequestDone_S mais par le hook sur
			// JWK_AIForce.DoSpawn (voir le bas du fichier). On ne garde ici que les
			// vehicules avec equipage, qui n'ont pas d'equivalent cote fonction.
			EventProvider.ConnectEvent(sys.OnForceCrewedVehicleAttached_S, OnCrewedVehicle);
			Print("[FFRX][Census] Branche sur les vehicules de la file FF.", LogLevel.NORMAL);
		}
		else
		{
			Print("[FFRX][Census] JWK_AIForceSystem introuvable -- recensement partiel.", LogLevel.WARNING);
		}

		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gm)
			gm.GetOnControllableSpawned().Insert(OnControllableSpawned);

		LoadEndpoint();

		GetGame().GetCallqueue().CallLater(Report, CENSUS_REPORT_MIN * 60000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe vient d'etre spawne. Appele depuis le hook sur JWK_AIForce.DoSpawn, qui
	//! est LE point de passage obligatoire de tout spawn de groupe FF ET Reoccupation
	//! (JWK_StreamableAIForce herite de JWK_AIForce, donc le streaming passe par la aussi).
	//!
	//! On ne compte PAS les membres ici. La composition n'est connue que si la requete
	//! porte m_aGroupMembers ; quand elle vient du prefab de groupe, ce tableau est vide.
	//! Les soldats sont donc comptes uniquement par ClassifyLate, sur les persos REELLEMENT
	//! apparus -- une seule source de verite, et pas de double comptage entre les deux.
	// --- Attribution de source ------------------------------------------------------
	// EOnInit voit TOUS les groupes mais ignore QUI les a demandes. On ne peut pas
	// intercepter le spawn natif (SpawnEntityPrefab est `proto`, donc C++), ni les
	// helpers DARC (SDRC_AIHelper est `sealed`), ni JWK_AIForce (le modder casse les
	// [Friend] de FF). On attrape donc les APPELANTS que l'on peut modder, juste avant
	// qu'ils spawnent : ils posent un indice, et le groupe qui nait dans la foulee le
	// ramasse. Purement declaratif -- si l'indice manque, on retombe sur "non attribue".
	protected static string s_sSourceHint;
	protected static float s_fSourceHintTime;

	//! Fenetre d'attribution. Large assez pour couvrir le spawn des membres d'un groupe,
	//! courte assez pour ne pas coller l'etiquette au groupe suivant.
	static const float SOURCE_HINT_TTL_MS = 3000;

	//------------------------------------------------------------------------------------------------
	//! Pose par un appelant identifie (SDRC_Mission.MissionStart, etc.) juste avant son spawn.
	static void SetSourceHint(string source)
	{
		s_sSourceHint = source;

		World world = GetGame().GetWorld();
		if (world)
			s_fSourceHintTime = world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Consomme l'indice s'il est encore frais. "" sinon.
	protected static string TakeSourceHint()
	{
		if (s_sSourceHint == "")
			return "";

		World world = GetGame().GetWorld();
		if (!world)
			return "";

		if (world.GetWorldTime() - s_fSourceHintTime > SOURCE_HINT_TTL_MS)
		{
			s_sSourceHint = "";
			return "";
		}

		return s_sSourceHint;
	}

	//------------------------------------------------------------------------------------------------
	//! Source d'un groupe : indice de l'appelant, sinon deduction par la faction.
	protected static string ResolveSource(SCR_AIGroup group)
	{
		string hint = TakeSourceHint();
		if (hint != "")
			return hint;

		// Reoccupation etiquette ses death squads par une FACTION dediee
		// (FFRO_DeathSquadFaction) : c'est lisible sur le groupe lui-meme, sans avoir a
		// hooker quoi que ce soit. On caste la faction plutot que d'appeler le helper
		// FFRO_TerrorFactionVisuals.IsDeathSquadFaction : moins de surface de rupture si
		// Reoccupation reorganise ses classes utilitaires.
		if (group && FFRO_DeathSquadFaction.Cast(group.GetFaction()))
			return "Reoccupation (death squad)";

		return "non attribue (FF / garnison / autre)";
	}

	//------------------------------------------------------------------------------------------------
	//! Point d'entree appele depuis `modded class SCR_AIGroup.EOnInit` (FFRX_AIAssault.c).
	//! Statique et tolerante au recensement non demarre : EOnInit tourne pour des groupes
	//! crees avant notre Boot() (garnisons posees a l'ouverture du monde).
	static void NoteGroupFromEOnInit(SCR_AIGroup group, IEntity owner)
	{
		if (!group || !owner)
			return;

		FFRX_SpawnCensus census = Get();
		if (!census)
			return;

		ResourceName rn;
		EntityPrefabData data = owner.GetPrefabData();
		if (data)
			rn = data.GetPrefabName();

		census.NoteGroupSpawn(rn, group);
	}

	//------------------------------------------------------------------------------------------------
	void NoteGroupSpawn(ResourceName groupPrefab, SCR_AIGroup group)
	{
		m_iGroupSpawns++;
		m_Groups.Add(ShortName(groupPrefab));
		m_Sources.Add(ResolveSource(group));

		// Horodatage : les persos qui apparaissent dans la seconde qui suit seront
		// attribues a ce groupe plutot qu'a un producteur hors file (cf. ClassifyLate).
		World world = GetGame().GetWorld();
		if (world)
			m_fLastQueueSpawnTime = world.GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicule avec equipage attache a une force.
	[ReceiverAttribute()]
	void OnCrewedVehicle(JWK_AIForce force, JWK_CrewedVehicle crewed)
	{
		if (!crewed || !crewed.m_Vehicle)
			return;

		m_Vehicles.Add(EntPrefabName(crewed.m_Vehicle));
	}

	//------------------------------------------------------------------------------------------------
	//! Filet de securite : tout perso controlable qui apparait, quel que soit le producteur.
	//! Sert surtout a voir ce que DARC injecte (il court-circuite la file FF).
	//!
	//! ATTENTION : ON NE TESTE PAS LA FACTION ICI. Quand cet event tire, l'affiliation de faction
	//! n'est pas encore posee sur le perso : GetEntityRole() renvoie UNDEFINED, donc
	//! IsEnemy() etait faux pour TOUT LE MONDE et le recensement des soldats restait a
	//! zero alors que des ennemis spawnaient bel et bien (constate sur le dedie le
	//! 2026-09-10 : 0 soldat recense pendant que FFRX_AIDifficulty logguait des dizaines
	//! d'ennemis). FFRX_AIDifficulty tombe dans le meme piege et s'en sort en re-testant
	//! 10 s plus tard -- on fait pareil, en differant la classification.
	void OnControllableSpawned(IEntity entity)
	{
		if (!entity)
			return;

		// L'instant de l'APPARITION est fige ici et transporte tel quel : c'est lui qu'il
		// faut comparer au dernier spawn de groupe, pas l'instant (plus tardif) ou la
		// classification s'execute.
		float seenAt = 0;
		World world = GetGame().GetWorld();
		if (world)
			seenAt = world.GetWorldTime();

		// Le callqueue tient une reference sur l'entite le temps du delai, donc pas de
		// pointeur mort si le perso meurt entre-temps (on revalide quand meme).
		GetGame().GetCallqueue().CallLater(ClassifyLate, CENSUS_CLASSIFY_DELAY_MS, false, entity, seenAt);
	}

	//------------------------------------------------------------------------------------------------
	//! Classification differee d'un perso apparu : maintenant sa faction est resolue.
	void ClassifyLate(IEntity entity, float seenAt)
	{
		if (!entity)
			return;

		if (!IsEnemy(entity))
		{
			// Allies, civils, joueurs... et eventuellement un ennemi dont la faction n'a
			// jamais ete resolue. Compteur de garde-fou : s'il grimpe alors que SOLDATS
			// reste a 0, c'est le FILTRE qui est en cause, pas l'absence de spawns.
			m_iNonEnemySpawns++;
			return;
		}

		// GetOnControllableSpawned tire pour TOUT ce qui est "controlable", vehicules
		// compris : sans ce filtre, un BRDM2 ou un Mi8 se retrouve compte comme un
		// SOLDAT (constate dans le rapport du 2026-09-11 : BRDM2, BTR70, Mi8MT et
		// volhaWZSSR listes parmi les 95 soldats). Meme garde que le killfeed de
		// FFRX_FFStateSender, qui filtre lui aussi sur SCR_ChimeraCharacter.
		// Le vehicule, lui, est bien compte -- mais dans la liste VEHICULES, par
		// CountCarrierVehicle ci-dessous.
		if (!SCR_ChimeraCharacter.Cast(entity))
			return;

		m_Soldiers.Add(EntPrefabName(entity));

		// D'ou vient-il ? La file FF annonce ses groupes via NoteGroupSpawn juste avant que
		// les persos apparaissent. Si aucun spawn de groupe n'a ete annonce dans la
		// seconde qui precede l'apparition, le perso vient d'ailleurs (DARC, garnison
		// pre-placee, script tiers). Heuristique temporelle assumee : on n'a aucun lien
		// direct perso -> requete de spawn, et DARC passe par une classe `sealed`.
		if (m_fLastQueueSpawnTime > 0 && (seenAt - m_fLastQueueSpawnTime) <= CENSUS_QUEUE_ATTRIBUTION_MS)
			m_Sources.Add("file FF/Reoccupation (perso)");
		else
			m_Sources.Add("hors file FF (DARC/garnison)");

		// Vehicule porteur. OnForceCrewedVehicleAttached_S ne voit QUE les vehicules
		// attaches a une force FF : un blinde ou un helico amene par DARC n'y apparait
		// jamais. On regarde donc dans quoi l'ennemi se trouve, et on dedoublonne par
		// entite -- sinon un camion a 8 places serait compte 8 fois.
		CountCarrierVehicle(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Compte une seule fois le vehicule dans lequel se trouve un ennemi recense.
	//!
	//! ATTENTION AU PROPRIETAIRE DU COMPARTIMENT. slot.GetOwner() ne rend PAS le vehicule
	//! mais la sous-entite qui porte ce compartiment : une tourelle, une banquette, un
	//! module de cargo. Pris tel quel, le recensement listait "BRDM2_turret",
	//! "VehPart_UAZ452_rearCargo_van" ou "Mi8MT_PKMT_mount_rear" au lieu du vehicule
	//! (constate dans le rapport du 2026-09-10), ce qui faussait aussi les pourcentages.
	//! On remonte donc a la racine de la hierarchie -- meme forme que FF dans
	//! JWK_SCR_GetInUserAction : GetMainParent(compartment.GetOwner(), true) puis cast.
	protected void CountCarrierVehicle(IEntity occupant)
	{
		CompartmentAccessComponent cac = JWK_CompTU<CompartmentAccessComponent>.FindIn(occupant);
		if (!cac)
			return;

		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot)
			return;

		IEntity owner = slot.GetOwner();
		if (!owner)
			return;

		// onlyRoot=true : on veut la racine, pas le premier parent rencontre.
		Vehicle vehicle = Vehicle.Cast(SCR_EntityHelper.GetMainParent(owner, true));
		// Le cast ecarte au passage ce qui n'est pas un vehicule -- typiquement un servant
		// de tourelle STATIQUE, qui n'a rien a faire dans un parc de vehicules.
		if (!vehicle)
			return;

		// EntityID n'est pas une cle de map utilisable telle quelle : on passe par sa
		// representation texte, stable pour la duree de vie de l'entite.
		string id = vehicle.GetID().ToString();
		if (m_mSeenVehicles.Contains(id))
			return;

		m_mSeenVehicles.Insert(id, true);
		m_Vehicles.Add(EntPrefabName(vehicle));
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsEnemy(IEntity entity)
	{
		if (!JWK.GetFactions())
			return false;

		return JWK.GetFactions().GetEntityRole(entity) == JWK_EFactionRole.ENEMY;
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}Prefabs/.../Rifle_k98k.et" -> "Rifle_k98k". Meme regle que FFRX_FFStateSender.
	protected string ShortName(ResourceName rn)
	{
		string s = rn;
		if (s == "")
			return "";

		int cut = s.LastIndexOf("/");
		if (cut >= 0)
			s = s.Substring(cut + 1, s.Length() - cut - 1);

		cut = s.LastIndexOf(".");
		if (cut > 0)
			s = s.Substring(0, cut);

		return s;
	}

	//------------------------------------------------------------------------------------------------
	protected string EntPrefabName(IEntity e)
	{
		if (!e)
			return "";

		EntityPrefabData data = e.GetPrefabData();
		if (!data)
			return "";

		return ShortName(data.GetPrefabName());
	}

	//------------------------------------------------------------------------------------------------
	//! Rapport periodique dans le log serveur.
	void Report()
	{
		m_iSinceStartSec += CENSUS_REPORT_MIN * 60;

		string head = "[FFRX][Census] ===== RECENSEMENT DES SPAWNS ENNEMIS ====="
			+ "  duree " + (m_iSinceStartSec / 60).ToString() + " min"
			+ "  spawns de groupe " + m_iGroupSpawns.ToString()
			// Garde-fou : si SOLDATS est a 0 alors que ce compteur monte, des persos
			// apparaissent bien mais ne sont pas vus comme ennemis -> c'est le filtre
			// qu'il faut regarder, pas le jeu. Si les DEUX sont a 0, rien n'a spawne.
			+ "  (non-ennemis ignores " + m_iNonEnemySpawns.ToString() + ")";
		// PrintFormat et non Print : `Print(variable)` affiche la DECLARATION de la variable
		// ("string head = '...'") au lieu de son contenu -- c'est un print de debogage, pas
		// un print de texte. Les autres lignes du rapport s'affichent proprement parce
		// qu'elles passent un appel de fonction (une expression n'a pas de nom a afficher).
		PrintFormat("%1", head);

		// Un Print par tableau : un seul Print geant se ferait tronquer.
		Print(m_Groups.Render("GROUPES"), LogLevel.NORMAL);
		Print(m_Soldiers.Render("SOLDATS"), LogLevel.NORMAL);
		Print(m_Vehicles.Render("VEHICULES"), LogLevel.NORMAL);
		Print(m_Sources.Render("SOURCES"), LogLevel.NORMAL);

		// Les tableaux ci-dessus comptent des spawns REUSSIS. Celui-ci compte les DECISIONS
		// de la ponderation des equipes specialistes (tirages, tirages sautes faute de
		// budget, repartition) -- c'est ce qui permet de dire si un curseur a 6 % qui ne
		// produit rien a ete ignore, saute, ou tire sans aboutir. Cf. FFRX_SpecialistWeights.
		Print(FFRX_SpecialistWeights.Render(), LogLevel.NORMAL);

		Send();
	}

	//------------------------------------------------------------------------------------------------
	//! POST du recensement complet vers le site (endpoint /ffcensus). Meme transport que
	//! FFRX_FFStateSender : URL derivee de $profile:Fleet/GTG.json, header Bearer.
	//! L'etat envoye est CUMULATIF depuis le demarrage du serveur ; le site remplace son
	//! instantane a chaque envoi, donc rien a nettoyer et un site redemarre se re-remplit
	//! tout seul au prochain rapport.
	protected void Send()
	{
		if (m_sUrl == "")
			return;

		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		string body = BuildJson();

		RestContext rc = api.GetContext(m_sUrl);
		rc.SetHeaders(string.Format("Authorization, Bearer %1", m_sApiKey));
		rc.POST(m_Cb, "", body);
	}

	//------------------------------------------------------------------------------------------------
	//! Assemblage a la main avec '+' : string.Format tronque sa sortie a ~8 Ko et le
	//! recensement depasse largement ca sur une longue session.
	protected string BuildJson()
	{
		string entries = m_Groups.Json("group");
		string s = m_Soldiers.Json("soldier");
		string v = m_Vehicles.Json("vehicle");
		string src = m_Sources.Json("source");

		if (s != "")
		{
			if (entries != "") entries = entries + ",";
			entries = entries + s;
		}
		if (v != "")
		{
			if (entries != "") entries = entries + ",";
			entries = entries + v;
		}
		if (src != "")
		{
			if (entries != "") entries = entries + ",";
			entries = entries + src;
		}

		return "{\"duration_sec\":" + m_iSinceStartSec.ToString()
			+ ",\"group_spawns\":" + m_iGroupSpawns.ToString()
			+ ",\"entries\":[" + entries + "]}";
	}

	//------------------------------------------------------------------------------------------------
	//! Remplace le suffixe d'endpoint dans l'URL Fleet ("/positions" -> "/ffcensus").
	protected string DeriveUrl(string base, string suffix)
	{
		int idx = base.IndexOf("positions");
		if (idx < 0)
			return "";
		return base.Substring(0, idx) + suffix;
	}

	//------------------------------------------------------------------------------------------------
	protected void LoadEndpoint()
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile("$profile:Fleet/GTG.json"))
		{
			Print("[FFRX][Census] $profile:Fleet/GTG.json absent -> envoi au site desactive (les logs restent).", LogLevel.WARNING);
			return;
		}

		string posUrl = "";
		ctx.ReadValue("url", posUrl);
		ctx.ReadValue("apiKey", m_sApiKey);
		if (posUrl == "" || m_sApiKey == "")
		{
			Print("[FFRX][Census] url/apiKey manquants dans GTG.json -> envoi au site desactive.", LogLevel.WARNING);
			return;
		}

		m_sUrl = DeriveUrl(posUrl, "ffcensus");
		m_Cb = new FFRX_CensusRestCb();
	}

	//------------------------------------------------------------------------------------------------
	//! Rendu compact pour la commande chat (une seule ligne par tableau, tete de liste).
	string TopLine(string which, int howMany)
	{
		FFRX_CensusTally t = m_Groups;
		if (which == "soldats")
			t = m_Soldiers;
		else if (which == "vehicules")
			t = m_Vehicles;

		string full = t.Render(which);
		array<string> lines = {};
		full.Split("\n", lines, true);

		string txt = "";
		int shown = 0;
		foreach (string line : lines)
		{
			if (line == "")
				continue;
			if (txt != "")
				txt = txt + " | ";
			txt = txt + line;
			shown++;
			if (shown > howMany)
				break;
		}
		return txt;
	}
}

// ---------------------------------------------------------------------------
//  Comment on capte les groupes -- et pourquoi PAS autrement.
//
//  1. PAS l'event JWK_AIForceSystem.OnAiSpawnRequestDone_S. Il existe et il est bien
//     jete a chaque spawn, mais tout le code FF s'y abonne via ConnectEventFiltered
//     (filtre sur UNE requete precise). Notre ConnectEvent global n'a jamais rien
//     recu : GROUPES est reste a 0 sur 30 min de session alors que des ennemis
//     spawnaient. Constate, pas suppose.
//
//  2. PAS `modded class JWK_AIForce` (override de DoSpawn). C'etait la solution
//     evidente et elle FONCTIONNAIT... mais elle CASSE LE JEU au demarrage a froid.
//     FF donne acces a ses membres proteges par l'attribut [Friend(JWK_AIForce)] :
//         [Friend(JWK_AIForce)]
//         protected void NotifyForceGroupAttached_S(...) { ... force.m_aModules ... }
//     Modder une classe citee dans un [Friend] fait perdre l'amitie, et TOUS les
//     fichiers FF qui en dependaient perdent l'acces d'un coup : 48 erreurs dans
//     JWK_AIForceSystem.c et JWK_AIForceDTDS.c, module "Game" qui refuse de compiler.
//     Piege sournois : le hot-reload ne revalide PAS les amities, seul un demarrage
//     a froid le voit -- ca passe donc en dev et ca casse au lancement reel.
//
//  3. DONC : SCR_AIGroup.EOnInit, appele pour CHAQUE groupe cree, tous producteurs
//     confondus (FF, Reoccupation, DARC, base-game). Aucune amitie en jeu.
//     Ce qu'on perd : le contexte "quelle force FF a demande ce groupe". Ce qu'on
//     garde, et c'est la question posee : quel TYPE de groupe apparait, et en quelle
//     proportion. Le hook lui-meme vit dans FFRX_AIAssault.c, qui declare deja le
//     seul `modded class SCR_AIGroup` autorise pour cet addon.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  #census -- sortir le recensement a la demande (admin).
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_CensusCommand : ScrServerCommand
{
	override string GetKeyword() { return "census"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Handle(argv); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return Handle(argv); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }

	//------------------------------------------------------------------------------------------------
	protected ScrServerCmdResult Handle(array<string> argv)
	{
		FFRX_SpawnCensus census = FFRX_SpawnCensus.Get();
		if (!census)
			return ScrServerCmdResult("Recensement non demarre.", EServerCmdResultType.ERR);

		string which = "groupes";
		if (argv.Count() >= 2)
			which = argv[1];

		// "#census reset" : repart de zero, comme le bouton de la page /recensement.
		if (which == "reset")
		{
			census.Reset();
			return ScrServerCmdResult("Recensement remis a zero.", EServerCmdResultType.OK);
		}

		// Le rapport complet part dans le LOG (le chat ne tiendrait pas) ; le chat recoit
		// la tete de liste de la categorie demandee.
		census.Report();

		return ScrServerCmdResult(census.TopLine(which, 5), EServerCmdResultType.OK);
	}
}
