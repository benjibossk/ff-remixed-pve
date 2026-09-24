// FF - REMIXED - PVE
// Theatre des dotations : detecter la carte / le scenario en cours et servir la BONNE
// tenue, sans dupliquer les dotations une fois par carte.
//
// LE PROBLEME
//   Les dotations vivent sur le site, rangees par CLE DE FACTION, et FFRX_LoadoutRegistry
//   filtre sur la cle EXACTE. Sur Anizay le joueur appartient a "FIA_DESERT" ; les 7
//   dotations existantes sont enregistrees sous "FIA" -> la caisse d'arsenal d'Anizay
//   n'affichait RIEN.
//
// LA DETECTION EST DEJA LA -- on n'a rien a inventer
//   Chaque scenario choisit sa faction joueur (Everon = FIA, Anizay = FIA_DESERT). La cle
//   de faction EST donc le selecteur de theatre : pas de lecture de monde, pas de table de
//   correspondance carte->theatre a maintenir. Une nouvelle carte desertique reutilise
//   FIA_DESERT et fonctionne sans une ligne de code.
//
// DEUX MECANISMES, DANS CET ORDRE
//   1. REPLI DE FACTION : une faction sans dotation propre herite de celles de sa faction
//      de base ("FIA_DESERT" -> "FIA"). La caisse n'est donc jamais vide.
//   2. SUBSTITUTION DE CAMO : les pieces de camo de la dotation heritee sont remplacees a
//      l'equipement par leur equivalent du theatre (CE/CCE/BME/Tundra/OD -> DA en desert).
//
// POURQUOI PAS SIMPLEMENT DUPLIQUER LES 7 DOTATIONS EN "FIA_DESERT" ?
//   Parce qu'il faudrait ensuite les maintenir en double : changer une arme ou un chargeur
//   sur la version Everon ne toucherait pas la version Anizay, et les deux divergeraient
//   en silence. Ici il n'y a qu'UNE source de verite ; le theatre n'agit que sur le camo.
//   Une dotation VRAIMENT specifique a un theatre reste possible : il suffit de
//   l'enregistrer sous "FIA_DESERT" sur le site, elle prend alors le pas sur le repli.
//
// AJOUTER UN THEATRE (ex. hiver) : creer la faction "FIA_<SUFFIXE>", ajouter le suffixe
//   dans TheatreOfFaction() et une table de substitution. Rien d'autre.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

enum FFRX_ETheatre
{
	DEFAULT,	// Everon / woodland : aucune substitution, les dotations sont deja en CE
	DESERT		// Anizay / Daguet
}

class FFRX_LoadoutTheatre
{
	// Suffixe de cle de faction -> theatre. La faction de BASE est la cle privee de son
	// suffixe ("FIA_DESERT" -> "FIA"), d'ou le repli des dotations.
	protected static const string SUFFIX_DESERT = "_DESERT";

	//! Theatre deduit de la cle de faction du joueur.
	static FFRX_ETheatre TheatreOfFaction(string factionKey)
	{
		if (EndsWith(factionKey, SUFFIX_DESERT))
			return FFRX_ETheatre.DESERT;
		return FFRX_ETheatre.DEFAULT;
	}

	//! Faction dont `factionKey` herite les dotations, "" si elle n'herite de personne.
	//! "FIA_DESERT" -> "FIA" ; "FIA" -> "".
	static string BaseFactionKey(string factionKey)
	{
		if (EndsWith(factionKey, SUFFIX_DESERT))
			return factionKey.Substring(0, factionKey.Length() - SUFFIX_DESERT.Length());
		return "";
	}

	protected static bool EndsWith(string s, string suffix)
	{
		int sl = s.Length();
		int fl = suffix.Length();
		if (fl == 0 || sl < fl)
			return false;
		return s.Substring(sl - fl, fl) == suffix;
	}

	// ------------------------------------------------------------------------------------
	//  Substitution de camo
	// ------------------------------------------------------------------------------------
	// Theatre courant, pose par ApplyJson avant de poser les pieces et remis a DEFAULT
	// apres. L'equipement se fait sur le serveur, sequentiellement, pour un joueur a la
	// fois : un etat statique est sur (meme raisonnement que le scan de stock de
	// FFRX_LoadoutAction).
	protected static FFRX_ETheatre s_eCurrent = FFRX_ETheatre.DEFAULT;

	// GUID source (sans accolades) -> ResourceName complet de destination.
	// Construite a la demande : la table n'est utile que si un joueur desertique equipe
	// une dotation heritee.
	protected static ref map<string, string> s_mDesert;

	static void SetCurrent(FFRX_ETheatre t) { s_eCurrent = t; }

	//! Prefab a poser reellement, une fois le theatre applique. Tout ce qui n'est pas dans
	//! la table passe INCHANGE : armes, chargeurs, medical, sacs, berets (insigne de
	//! regiment, pas du camo -- cf. le catalogue desert qui les garde volontairement).
	static string Remap(string prefab)
	{
		if (s_eCurrent != FFRX_ETheatre.DESERT || prefab == "")
			return prefab;

		if (!s_mDesert)
			BuildDesertTable();

		string guid = GuidOf(prefab);
		if (guid == "")
			return prefab;

		string repl;
		if (s_mDesert.Find(guid, repl))
			return repl;
		return prefab;
	}

	//! Substitution de camo sur la TENUE ENTIERE, avant de la donner au jeu.
	//!
	//! Depuis la refonte, on ne pose plus les objets un par un : c'est le serialiseur du jeu
	//! qui rhabille le personnage d'un bloc. Le seul point ou l'on peut encore intervenir est
	//! donc la chaine elle-meme -- et ca tombe bien, elle cite les prefabs en clair.
	//! On remplace chaque piece de camo par son equivalent du theatre AVANT l'apply.
	//!
	//! No-op hors desert, et no-op pour tout ce qui n'est pas dans la table.
	//!
	//! ⚠️ LE SERIALISEUR DU JEU N'ECRIT PAS DE CHEMIN, IL ECRIT DES GUID NUS :
	//!     {"prefab":"9C495D014D582C64", ...}
	//! et NON pas "{GUID}Prefabs/.../Truc.et". Une premiere version cherchait la forme
	//! longue : elle ne trouvait jamais rien et le camo desert ne s'appliquait pas du tout,
	//! en silence (constate le 2026-09-18 sur le premier gabarit au nouveau format).
	//!
	//! On substitue donc GUID -> GUID, et on cible `"prefab":"..."` plutot que le GUID seul :
	//! les conteneurs s'ecrivent "SCR_EquipmentStorageComponent:56B49DA9722B635D" et
	//! porteraient sinon le risque d'etre reecrits eux aussi.
	static string RemapJson(string json)
	{
		if (s_eCurrent != FFRX_ETheatre.DESERT || json == "")
			return json;

		if (!s_mDesert)
			BuildDesertTable();

		if (!s_mDesert)
			return json;

		int swapped = 0;
		foreach (string guid, string repl : s_mDesert)
		{
			string replGuid = GuidOf(repl);
			if (replGuid == "" || replGuid == guid)
				continue;

			string from = "\"prefab\":\"" + guid + "\"";
			string to   = "\"prefab\":\"" + replGuid + "\"";

			int n = json.Replace(from, to);
			if (n > 0)
				swapped = swapped + n;
		}

		if (swapped > 0)
			Print("[FFRX][Theatre] desert : " + swapped.ToString() + " piece(s) de camo substituee(s).", LogLevel.NORMAL);

		return json;
	}

	//! "{7326973EDED9139D}Prefabs/..." -> "7326973EDED9139D" ("" si pas de GUID).
	protected static string GuidOf(string prefab)
	{
		if (prefab.Length() < 2 || prefab.Get(0) != "{")
			return "";
		int close = prefab.IndexOf("}");
		if (close <= 1)
			return "";
		return prefab.Substring(1, close - 1);
	}

	protected static void Add(string srcGuid, string dstResource)
	{
		s_mDesert.Set(srcGuid, dstResource);
	}

	// GUIDs releves dans le resourceDatabase.rdb du mod AMF-FANTASSIN (64CF25A41DCBDBE0),
	// pas par recherche de references croisees : le rdb est la seule source fiable.
	//
	// Regle de correspondance : on conserve la COUPE (manches roulees ou non) et on
	// degrade le GRADE quand la variante Daguet correspondante n'existe pas -- le camo
	// prime, l'insigne de grade est secondaire (et l'uniforme de spawn est deja sans
	// galon, cf. Loadout_FIA_DESERT.conf).
	protected static void BuildDesertTable()
	{
		s_mDesert = new map<string, string>();

		string ubasDA        = "{8FC954BD80F7B3CF}Prefabs/Characters/Uniforms/F3_Ubas/DA/F3_Ubas_DA.et";
		string ubasDAPatch   = "{20C69D98851FDEA1}Prefabs/Characters/Uniforms/F3_Ubas/DA/F3_Ubas_DA_Patch_FRANCE_BV01.et";
		string ubasDACapi    = "{76588BC5EDE04C22}Prefabs/Characters/Uniforms/F3_Ubas/DA/F3_Ubas_DA_Patch_FRANCE_BV01_Capitaine.et";
		string ubasDAAdj     = "{A1AA6E855BFC95AC}Prefabs/Characters/Uniforms/F3_Ubas/DA/F3_Ubas_DA_Patch_FRANCE_BV01_AdjudantChef.et";
		string ubasRolledDA  = "{9C495D014D582C64}Prefabs/Characters/Uniforms/F3_Ubas/DA/F3_Ubas_Rolled_DA.et";

		// --- Vestes F3_Ubas, manches longues ---
		Add("D96EBF342FD90F15", ubasDA);        // F3_Ubas_CE
		Add("14B6A42342F0D700", ubasDAPatch);   // F3_Ubas_CE_Patch_FRANCE
		Add("4A28AD8F2E2BA0A2", ubasDAPatch);   // F3_Ubas_CE_Patch_FRANCE_BV02
		Add("381D1EA7D11042F6", ubasDAPatch);   // ..._BV02_1erClasse   (pas de DA 1erClasse non roule)
		Add("5D5AC2EEDD098A02", ubasDACapi);    // ..._BV02_Capitaine
		Add("22813DA803FB35A8", ubasDAPatch);   // ..._BV02_Lieutnant   (pas de DA Lieutnant non roule)
		Add("E1B3E5BA66B2A40A", ubasDA);        // F3_Ubas_BME
		Add("D89CE19042660331", ubasDAPatch);   // F3_Ubas_BME_Patch_FRANCE
		Add("D89CE19042660332", ubasDAPatch);   // F3_Ubas_BME_Patch_FRANCE_BV02
		Add("AB6181CCC1051814", ubasDAPatch);   // ..._BV02_1erClasse
		Add("026419417923FBFF", ubasDAAdj);     // ..._BV02_AdjudantChef
		Add("CE265D85CD1CD0E0", ubasDACapi);    // ..._BV02_Capitaine
		Add("B1FDA2C313EE6F4A", ubasDAPatch);   // ..._BV02_Lieutnant
		Add("0DEC40371B86E2C3", ubasDA);        // F3_Ubas_Tundra
		Add("70124083557AC666", ubasDAPatch);   // F3_Ubas_Tundra_Patch_FRANCE_BV02
		Add("85922F869AE254A9", ubasDAPatch);   // ..._BV02_1erClasse
		Add("E7773EBCE72DD9DC", ubasDAAdj);     // ..._BV02_AdjudantChef
		Add("C81D3B7CFBA10EE4", ubasDACapi);    // ..._BV02_Capitaine
		Add("0A5388C9091EA6E6", ubasDAPatch);   // ..._BV02_Caporal
		Add("9F0E0C89480923F7", ubasDAPatch);   // ..._BV02_Lieutnant
		Add("07C5ABCAE631876E", ubasDAPatch);   // ..._BV02_Sergent
		Add("DD69D67E07743318", ubasDA);        // F3_Ubas_OD

		// --- Vestes F3_Ubas, manches roulees ---
		Add("CAEEB688E27690BE", ubasRolledDA);  // F3_Ubas_Rolled_CE
		Add("6E3E16650D659DA2", ubasRolledDA);  // F3_Ubas_Rolled_CE_AdjudantChef
		Add("7326973EDED9139D", ubasRolledDA);  // F3_Ubas_Rolled_CE_Caporal
		Add("C09603B4A3F2D150", ubasRolledDA);  // F3_Ubas_Rolled_BME
		Add("A8FBC2BCC5F1F38F", ubasRolledDA);  // F3_Ubas_Rolled_BME_..._Caporal
		Add("CEE9DFC2CADBACB3", ubasRolledDA);  // F3_Ubas_Rolled_OD

		// --- Pantalons ---
		string pantDA = "{F277891AB79F482D}Prefabs/Characters/Uniforms/F3_Pantalon/F3_Pantalon_DA.et";
		Add("ADBA42284222F1BA", pantDA);        // F3_Pantalon_CE
		Add("B68982BCEE2AEAE6", pantDA);        // F3_Pantalon_BME
		Add("AFFA424D3F916C2B", pantDA);        // F3_Pantalon_TUNDRA
		Add("7ACBEAC605222C8A", "{C095E929FA798DB3}Prefabs/Characters/Uniforms/FELIN_T4S2_Pantalon/Pants_FELIN_T4S2_DA_base.et");

		// --- Vestes de combat F3_Vest / FELIN ---
		string vestDA       = "{BFDF3D893388DDF8}Prefabs/Characters/Uniforms/F3_Vest/F3_Vest_DA.et";
		string vestRolledDA = "{89CA23B61048FC7B}Prefabs/Characters/Uniforms/F3_Vest/F3_Vest_Rolled_DA.et";
		Add("E978D6009CA66122", vestDA);        // F3_Vest_CE
		Add("F77F86CFFB4A5F98", vestDA);        // F3_Vest_CE_Patch_FRANCE_BV02
		Add("5D9D5809A8FE1D3B", vestDA);        // F3_Vest_BME
		Add("2EED68835F6248EB", vestDA);        // F3_Vest_Tundra
		Add("DD84DFBAD0BEE359", vestRolledDA);  // F3_Vest_Rolled_CCE
		Add("2E164BB8846B91F3", vestRolledDA);  // F3_Vest_Rolled_BME
		Add("32F56D356565CE93", "{80D272002E5F2A80}Prefabs/Characters/Uniforms/FELIN_T4S2_Vest/FELIN_T4S2_Vest_DA.et");
		Add("8968F9EE1C987770", "{48F8D5B1422E6BE3}Prefabs/Characters/Uniforms/FELIN_T4S2_Vest/FELIN_T4S2_Vest_Rolled_DA.et");

		// --- Casques ---
		Add("52A62092DB95EB57", "{8F0BF779AF43D7A0}Prefabs/Characters/HeadGear/F3_HELMET/MSA_FELIN_TAN_Base.et");
		Add("A15A18EDE068DB93", "{291E1064839410F4}Prefabs/Characters/HeadGear/F3_HELMET/MSA_FELIN_TAN_SquadLeader.et");
		Add("371F3544B6C4BCEE", "{D0384575A45621D9}Prefabs/Characters/HeadGear/TCNVG_Helmet/MSA_TCNVG_DA.et");

		// --- Chaussures ---
		Add("47893449DFFB82E4", "{253C75176D3BF9A4}Prefabs/Characters/Footwear/Haix_Sable_Base.et");
	}
}
