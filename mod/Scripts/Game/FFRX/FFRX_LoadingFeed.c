// FF - REMIXED - PVE
// L'ecran de chargement affiche les nouvelles du site.
//
// ======================================================================================
//  POURQUOI ON REUTILISE LE CARROUSEL D'ASTUCES PLUTOT QUE DE CREER DES WIDGETS
// ======================================================================================
//
// L'ecran de chargement possede deja un TextWidget stylé, place et anime :
// SCR_LoadingHintComponent fait tourner les astuces du jeu toutes les 10 s
// (Configs/UI/LoadingScreenHints.conf). On y injecte nos lignes.
//
// Creer nos propres widgets etait l'autre voie, et elle est plus couteuse qu'il n'y parait :
//   - un widget cree sans slot fait 0x0 et reste INVISIBLE (cf. memoire enfusion-ui-inject-widgets) ;
//   - une mise en page chargee par CreateWidgets() a une racine de taille NULLE, donc des
//     enfants ancres 0..1 remplissent du vide (cf. memoire enfusion-createwidgets-root-fill) ;
//   - un TextWidget cree par script n'a pas de police tant qu'on ne lui en donne pas une.
// Les trois nous ont deja coute du temps. Le carrousel evite les trois d'un coup.
//
// Un panneau dedie reste possible ensuite, mais il demandera un vrai .layout importe dans
// le Workbench -- c'est la facon robuste de maitriser police et placement.
//
// ======================================================================================
//  D'OU VIENT LE TEXTE
// ======================================================================================
//
// D'un fichier PUBLIC du site : https://arma.collectifxxl.fr/static/loadinginfo.json
//
// Public, et c'est necessaire : pendant le chargement, le client n'a AUCUNE cle d'API. La
// cle Bearer vit dans $profile:Fleet/GTG.json, cote SERVEUR uniquement. Un endpoint
// authentifie serait donc inutilisable ici.
//
// ======================================================================================
//  LE TIMING (MESURE) ET LE CACHE
// ======================================================================================
//
// Une requete reseau est asynchrone : on craignait qu'un chargement se termine avant la
// reponse, et donc de toujours afficher les nouvelles de la session precedente.
//
// MESURE DU 2026-09-18 (dedie, deux connexions) : la reponse arrive en ~200 ms pour un
// chargement de ~10 s. Les nouvelles s'affichent donc DES le chargement en cours.
//
// Le cache ($profile:FFRX_loadinfo.json) reste utile, mais comme FILET et non comme
// mecanisme principal : site lent, site injoignable, ou premier affichage avant l'arrivee
// de la reponse. Il garantit qu'on n'a jamais d'ecran vide.
//
// NOTE : ASCII uniquement dans les chaines (le dedie compile en strict).

//! Forme du JSON attendu : {"lines":["...","..."]}
class FFRX_LoadFeedData : JsonApiStruct
{
	ref array<string> lines;

	void FFRX_LoadFeedData()
	{
		lines = {};
		RegV("lines");
	}
}

class FFRX_LoadFeedCb : RestCallback
{
	override void OnSuccess(string data, int dataSize)
	{
		FFRX_LoadingFeed.OnFetched(data);
	}

	override void OnError(int errorCode)
	{
		// Silencieux a dessein : le site injoignable ne doit pas polluer l'ecran de
		// chargement d'un joueur. Le cache prend le relais.
	}

	override void OnTimeout()
	{
	}
}


// ======================================================================================
//  MEMORISATION DE L'UID  --  pour personnaliser l'ecran de chargement SUIVANT
// ======================================================================================
//
// Mesure du 2026-09-18 : pendant le chargement, aucune session joueur n'existe encore, donc
// impossible de savoir QUI charge. Mais une fois EN PARTIE, l'identite est disponible.
//
// On l'ecrit donc dans le profil du joueur ($profile:FFRX_me.json) depuis le
// PlayerController local (cf. FFRX_IntroCinematic.c), et l'ecran de chargement la relit au
// demarrage suivant pour demander sa fiche au site.
//
// Consequence assumee : un joueur qui se connecte pour la PREMIERE fois ne verra que les
// nouvelles generiques -- on ne le connait pas encore. Des sa deuxieme connexion, il a sa
// ligne personnelle. Arbitrage valide par Benji le 18/09.
// ======================================================================================

class FFRX_MeData : JsonApiStruct
{
	string uid;
	void FFRX_MeData() { RegV("uid"); }
}

class FFRX_MeCache
{
	static const string PATH = "$profile:FFRX_me.json";

	//------------------------------------------------------------------------------------------------
	//! Appele EN PARTIE, depuis le PlayerController local.
	static void Remember()
	{
		BackendApi ba = GetGame().GetBackendApi();
		if (!ba)
			return;

		int pid = SCR_PlayerController.GetLocalPlayerId();
		if (pid <= 0)
			return;

		string uid = ba.GetPlayerIdentityId(pid);
		if (uid == "")
			return;

		if (uid == Read())
			return;   // deja a jour : on n'ecrit pas le profil a chaque respawn

		FFRX_MeData me = new FFRX_MeData();
		me.uid = uid;

		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", me);
		ctx.SaveToFile(PATH);

		Print("[FFRX][Loading] identite memorisee pour le prochain ecran de chargement.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! UID memorise, "" si on n'a jamais joue sur ce profil.
	static string Read()
	{
		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile(PATH))
			return "";

		FFRX_MeData me = new FFRX_MeData();
		if (!ctx.ReadValue("", me))
			return "";

		return me.uid;
	}
}

//! Reponse de /api/v1/public/soldiercard/:uid
class FFRX_CardData : JsonApiStruct
{
	bool   known;
	string name;
	string rankName;
	int    rank;
	int    xp;
	int    playtimeSeconds;

	void FFRX_CardData()
	{
		RegV("known"); RegV("name"); RegV("rankName");
		RegV("rank"); RegV("xp"); RegV("playtimeSeconds");
	}
}

class FFRX_CardCb : RestCallback
{
	override void OnSuccess(string data, int dataSize) { FFRX_LoadingFeed.OnCard(data); }
	override void OnError(int errorCode) { }
	override void OnTimeout() { }
}

class FFRX_LoadingFeed
{
	protected static const string URL        = "https://arma.collectifxxl.fr/static/loadinginfo.json";
	protected static const string CACHE_PATH = "$profile:FFRX_loadinfo.json";
	protected static const string CARD_URL   = "https://arma.collectifxxl.fr/api/v1/public/soldiercard/";

	//! Ligne personnelle du joueur. Vide tant qu'on ne le connait pas (1re connexion) ou
	//! si le site n'a pas repondu -- le carrousel se rabat alors sur les nouvelles generiques.
	protected static string s_sMine;
	protected static ref FFRX_CardCb s_CardCb;

	//! Plafond de lignes retenues. Au-dela, l'ecran de chargement devient un mur de texte
	//! que personne ne lit -- et un chargement dure rarement plus d'une minute.
	protected static const int MAX_LINES = 6;

	protected static ref array<string> s_aLines;
	protected static ref FFRX_LoadFeedCb s_Cb;
	protected static bool s_bFetched;

	//------------------------------------------------------------------------------------------------
	//! Lignes a afficher (depuis le cache). Vide si on n'a jamais rien recu.
	static array<string> GetLines()
	{
		if (!s_aLines)
			LoadCache();
		return s_aLines;
	}

	//------------------------------------------------------------------------------------------------
	//! Lance UNE requete par session de jeu. Mesure : ~200 ms, donc le resultat est
	//! disponible pendant le chargement en cours (cf. en-tete).
	static void Fetch()
	{
		if (s_bFetched)
			return;
		s_bFetched = true;

		RestApi api = GetGame().GetRestApi();
		if (!api)
			return;

		s_Cb = new FFRX_LoadFeedCb();
		RestContext rc = api.GetContext(URL);
		rc.GET(s_Cb, "");

		// Fiche personnelle, si on connait deja ce joueur (cf. FFRX_MeCache).
		string uid = FFRX_MeCache.Read();
		if (uid == "")
			return;

		s_CardCb = new FFRX_CardCb();
		RestContext rcCard = api.GetContext(CARD_URL + uid);
		rcCard.GET(s_CardCb, "");
	}

	//------------------------------------------------------------------------------------------------
	//! Construit la ligne personnelle a partir de la reponse du site.
	static void OnCard(string data)
	{
		if (data == "")
			return;

		FFRX_CardData card = new FFRX_CardData();
		card.ExpandFromRAW(data);

		// known=false = joueur jamais vu cote site. Ce n'est PAS une panne : on se tait et
		// le joueur voit les nouvelles generiques, comme prevu.
		if (!card.known)
			return;

		string who = card.name;
		if (who == "")
			who = "Soldat";

		string grade = card.rankName;
		if (grade == "")
			grade = "Grade " + card.rank.ToString();

		int hours = card.playtimeSeconds / 3600;

		s_sMine = grade + " " + who + " -- " + card.xp.ToString() + " XP, " + hours.ToString() + " h au front.";
		Print("[FFRX][Loading] fiche personnelle recue : " + s_sMine, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static string GetMine()
	{
		return s_sMine;
	}

	//------------------------------------------------------------------------------------------------
	static void OnFetched(string data)
	{
		if (data == "")
			return;

		FFRX_LoadFeedData feed = new FFRX_LoadFeedData();
		feed.ExpandFromRAW(data);
		if (!feed.lines || feed.lines.IsEmpty())
			return;

		s_aLines = {};
		foreach (string l : feed.lines)
		{
			if (l == "")
				continue;
			s_aLines.Insert(l);
			if (s_aLines.Count() >= MAX_LINES)
				break;
		}

		SaveCache();
		Print("[FFRX][Loading] " + s_aLines.Count().ToString() + " ligne(s) de nouvelles mises en cache.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static void LoadCache()
	{
		s_aLines = {};

		SCR_JsonLoadContext ctx = new SCR_JsonLoadContext();
		if (!ctx.LoadFromFile(CACHE_PATH))
			return;

		FFRX_LoadFeedData feed = new FFRX_LoadFeedData();
		if (!ctx.ReadValue("", feed))
			return;
		if (!feed.lines)
			return;

		foreach (string l : feed.lines)
			if (l != "")
				s_aLines.Insert(l);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SaveCache()
	{
		FFRX_LoadFeedData feed = new FFRX_LoadFeedData();
		feed.lines = s_aLines;

		SCR_JsonSaveContext ctx = new SCR_JsonSaveContext();
		ctx.WriteValue("", feed);
		ctx.SaveToFile(CACHE_PATH);
	}
}

// ------------------------------------------------------------------------------------
//  Injection dans le carrousel de l'ecran de chargement
// ------------------------------------------------------------------------------------
modded class SCR_LoadingHintComponent
{
	protected int m_iFFRXNext;

	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		// On demande les nouvelles au site (~200 ms mesures). En attendant la reponse, le
		// carrousel affiche ce qui est en cache.
		FFRX_LoadingFeed.Fetch();
	}

	//------------------------------------------------------------------------------------------------
	//  MESURE FAITE LE 2026-09-18 -- l'UID du joueur n'est PAS lisible pendant le chargement.
	//
	//  Sondes posees puis retirees. Resultat identique aux 4 mesures (debut et fin de l'ecran,
	//  sur deux connexions successives au dedie) :
	//      GetPlayerIdentityId(0)=''   GetPlayerIdentityId(1)=''
	//      PlayerController=0          LocalPlayerId=0
	//      PlayerManager=1 mais 0 joueur connu     BackendApi=1, IsActive=1
	//  Le backend est actif, mais AUCUNE session joueur n'existe encore : l'identite n'arrive
	//  qu'une fois en partie, trop tard pour cet ecran.
	//
	//  => Afficher des infos PERSONNELLES ici suppose de MEMORISER l'UID entre deux sessions
	//     (l'ecrire dans le profil une fois en partie, le relire au chargement suivant).
	//     Un tout premier joueur ne verrait donc rien a sa premiere connexion.
	//
	//  A noter aussi : la requete au site a mis 200 ms pour un chargement de ~10 s. Les
	//  nouvelles s'affichent donc DES le chargement en cours, pas au suivant comme redoute ;
	//  le cache ne sert plus que de filet si le site est lent ou injoignable.
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Alterne NOS lignes et les astuces du jeu.
	//!
	//! On surcharge ShowHint plutot que de verser nos lignes dans `m_aAllHints`, et c'est
	//! important : le jeu marque chaque astuce comme LUE et ne la represente plus jamais
	//! (m_aReadHints, persiste dans les reglages utilisateur). Nos nouvelles auraient donc
	//! disparu apres un seul affichage, par joueur et pour toujours.
	override void ShowHint(int entryIndex = -1)
	{
		array<string> lines = FFRX_LoadingFeed.GetLines();
		if (!lines || lines.IsEmpty() || !m_wText)
		{
			super.ShowHint(entryIndex);
			return;
		}

		// Une fois sur deux : on garde les astuces du jeu, qui restent utiles aux nouveaux.
		if ((m_iFFRXNext % 2) == 1)
		{
			m_iFFRXNext++;
			super.ShowHint(entryIndex);
			return;
		}

		int idx = (m_iFFRXNext / 2) % lines.Count();
		m_iFFRXNext++;

		// DEUX EMPLACEMENTS DANS UN SEUL WIDGET.
		//
		// On voulait un panneau a plusieurs zones de texte. Un vrai .layout serait la voie
		// propre, mais il faut y declarer une POLICE par son GUID -- et un TextWidget sans
		// police ne rend rien du tout. Les polices du jeu de base vivent dans un pak dont on
		// n'a pas l'extraction, donc ce GUID n'est pas verifiable ici : on livrerait un
		// panneau invisible sans pouvoir le constater.
		//
		// Le widget d'astuces, lui, a deja sa police, son placement et son animation. On y
		// met donc DEUX lignes separees par un saut : la fiche personnelle au-dessus, la
		// nouvelle en dessous. Le joueur voit les deux d'un coup au lieu d'attendre 10 s.
		//
		// Si un vrai panneau multi-zones est voulu ensuite, il doit etre compose dans
		// l'editeur d'interface du Workbench (ou les polices se choisissent visuellement) ;
		// il suffira ensuite de me donner le GUID du layout et le nom des widgets.
		string mine = FFRX_LoadingFeed.GetMine();
		if (mine != "")
		{
			m_wText.SetText(mine + "\n\n" + lines[idx]);
			return;
		}

		m_wText.SetText(lines[idx]);
	}
}
