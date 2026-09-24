// FF - REMIXED - PVE
// #help -- la liste des commandes admin, en jeu.
//
// ======================================================================================
//  ⚠️ CETTE LISTE EST TENUE A LA MAIN, ET C'EST SUBI, PAS CHOISI
// ======================================================================================
//
// `ScrServerCommand` (classe generee du jeu de base) n'expose AUCUN registre : pas de
// "donne-moi toutes les commandes enregistrees". Verifie dans
// scripts/Game/generated/ScrServerCommand.c -- il n'y a que des `event` a surcharger.
// Impossible, donc, d'enumerer a l'execution.
//
// CONSEQUENCE : en ajoutant une commande, il faut penser a DEUX endroits.
//   1. ici, dans CMDS
//   2. la page Commandes du site : static/commands.html (tableau `cmds`)
//
// C'est la meilleure garantie qu'on ait : une commande absente d'ici est une commande que
// personne ne trouvera. L'inventaire du 2026-09-19 avait justement 7 commandes vivantes
// dans le code et absentes du site (camp, intro, menace, nvg, radiointel, tracante, vbied).
//
// Pour re-verifier plus tard, comparer :
//   grep -rh "override string GetKeyword" -A2 Scripts/ | grep -oE 'return "[a-z0-9]+"'
//   grep -oE 'k:"[a-z0-9]+"' static/commands.html
//
// NOTE : les commandes de GRADE (#promote, #demote, #setrank, #ranks, #xp) appartiennent a
// FLEET, pas a REMIXED. Elles sont listees ici pour l'admin -- qui se fiche de savoir quel
// mod les porte -- mais on ne les maintient pas depuis ce fichier.
//
// NOTE : ASCII uniquement dans les chaines (le dedie compile en strict).

class FFRX_HelpEntry
{
	string m_sKey;
	string m_sArg;
	string m_sDesc;

	void FFRX_HelpEntry(string key, string arg, string desc)
	{
		m_sKey  = key;
		m_sArg  = arg;
		m_sDesc = desc;
	}
}

class FFRX_Help
{
	//! Rubriques = celles de la page Commandes du site, pour que les deux se lisent pareil.
	//
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents.
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<string> s_aSections;

	static array<string> Sections()
	{
		if (!s_aSections)
		{
			s_aSections = new array<string>();
			s_aSections.Insert("CAMPAGNE & EXPLOITATION");
			s_aSections.Insert("MENACE & IA");
			s_aSections.Insert("CIVILS & RENSEIGNEMENT");
			s_aSections.Insert("CONSTRUCTION");
			s_aSections.Insert("GRADES (Fleet)");
		}

		return s_aSections;
	}

	//------------------------------------------------------------------------------------------------
	static void Fill(int section, notnull array<ref FFRX_HelpEntry> outList)
	{
		switch (section)
		{
			case 0:
				outList.Insert(new FFRX_HelpEntry("placefob", "", "Replace la FOB de depart a ta position."));
				outList.Insert(new FFRX_HelpEntry("resupply", "", "Declenche un ravitaillement maritime."));
				outList.Insert(new FFRX_HelpEntry("airresupply", "", "Declenche un ravitaillement aerien ennemi."));
				outList.Insert(new FFRX_HelpEntry("setsupply", "<n>", "Fixe le ravitaillement de la zone."));
				outList.Insert(new FFRX_HelpEntry("census", "[groupes|soldats|vehicules]", "Recensement des spawns ennemis (top 5 au chat, tableau complet au log)."));
				outList.Insert(new FFRX_HelpEntry("intro", "", "Rejoue la cinematique d'introduction."));
				break;

			case 1:
				outList.Insert(new FFRX_HelpEntry("menace", "", "Etat de la menace adaptative (jauges blinde / aerien par zone)."));
				outList.Insert(new FFRX_HelpEntry("camp", "[go]", "Anti-camping : etat de ta cellule. 'go' declenche la riposte ici en sautant les seuils."));
				outList.Insert(new FFRX_HelpEntry("nvg", "", "Vision nocturne ennemie : porteurs tires, reglages en vigueur."));
				outList.Insert(new FFRX_HelpEntry("tracante", "", "Tracantes : combien de tirs ont trahi une position."));
				outList.Insert(new FFRX_HelpEntry("spawnat", "", "Fait apparaitre un servant antichar ennemi."));
				outList.Insert(new FFRX_HelpEntry("spawnaa", "", "Fait apparaitre un servant sol-air ennemi."));
				outList.Insert(new FFRX_HelpEntry("spawncache", "", "Fait apparaitre une cache ennemie gardee."));
				break;

			case 2:
				outList.Insert(new FFRX_HelpEntry("civname", "", "Identite du civil vise (nom, confiance, interactions)."));
				outList.Insert(new FFRX_HelpEntry("civtrust", "<n>", "Modifie la confiance du civil vise."));
				outList.Insert(new FFRX_HelpEntry("frisk", "", "Fouille le civil vise."));
				outList.Insert(new FFRX_HelpEntry("revealspy", "", "Demasque le civil vise s'il est un espion."));
				outList.Insert(new FFRX_HelpEntry("radiointel", "", "Etat du renseignement radio en attente de collecte."));
				outList.Insert(new FFRX_HelpEntry("balise", "", "Balises GPS : combien sont allumees sur la carte."));
				outList.Insert(new FFRX_HelpEntry("radiosites", "", "Sites radio : total, operables, et tenus par la resistance."));
				outList.Insert(new FFRX_HelpEntry("testintel", "", "Envoie un renseignement civil de test."));
				outList.Insert(new FFRX_HelpEntry("tipmines", "", "Signale un champ de mines proche."));
				outList.Insert(new FFRX_HelpEntry("regeneratemines", "", "Repose les champs de mines automatiques."));
				outList.Insert(new FFRX_HelpEntry("trapcar", "", "Piege la voiture civile la plus proche."));
				outList.Insert(new FFRX_HelpEntry("ied", "[go]", "IED disperses sur les lieux : etat. 'go' force un semis immediat."));
				outList.Insert(new FFRX_HelpEntry("epaves", "", "Carcasses piegees : etat du balayage et nombre de charges armees."));
				outList.Insert(new FFRX_HelpEntry("vbied", "", "Lance une voiture piegee ROULANTE sur toi."));
				outList.Insert(new FFRX_HelpEntry("spawnbomber", "", "Fait apparaitre un kamikaze a pied."));
				outList.Insert(new FFRX_HelpEntry("demandes", "", "Liste les demandes civiles en cours."));
				break;

			case 3:
				outList.Insert(new FFRX_HelpEntry("pelle", "", "Pourquoi je ne peux pas construire : outil, zone, objets disponibles."));
				outList.Insert(new FFRX_HelpEntry("build", "", "Ouvre le menu de construction (si tu tiens une pelle valide)."));
				outList.Insert(new FFRX_HelpEntry("testproc", "", "Test de l'approvisionnement / boutique."));
				break;

			case 4:
				outList.Insert(new FFRX_HelpEntry("ranks", "", "Echelle des grades et paliers d'XP."));
				outList.Insert(new FFRX_HelpEntry("promote", "<joueur>", "Monte un joueur d'un grade."));
				outList.Insert(new FFRX_HelpEntry("demote", "<joueur>", "Descend un joueur d'un grade."));
				outList.Insert(new FFRX_HelpEntry("setrank", "<joueur> <n>", "Fixe le grade d'un joueur."));
				outList.Insert(new FFRX_HelpEntry("xp", "", "Ton XP et ton palier."));
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rendu d'une rubrique. On rend UNE rubrique a la fois : le chat du jeu tronque les
	//! messages longs, et 30 commandes d'un coup seraient illisibles de toute facon.
	static string Render(int section)
	{
		if (section < 0 || section >= Sections().Count())
			return RenderIndex();

		array<ref FFRX_HelpEntry> list = {};
		Fill(section, list);

		string txt = "== " + Sections()[section] + " ==";
		foreach (FFRX_HelpEntry e : list)
		{
			txt = txt + "\n#" + e.m_sKey;
			if (e.m_sArg != "")
				txt = txt + " " + e.m_sArg;
			txt = txt + "  --  " + e.m_sDesc;
		}
		return txt;
	}

	//------------------------------------------------------------------------------------------------
	static string RenderIndex()
	{
		string txt = "Commandes admin -- '#help <n>' pour une rubrique :";
		for (int i = 0; i < Sections().Count(); i++)
		{
			array<ref FFRX_HelpEntry> list = {};
			Fill(i, list);
			txt = txt + "\n  " + i.ToString() + ". " + Sections()[i]
				+ " (" + list.Count().ToString() + ")";
		}
		return txt + "\nDetail complet : arma.collectifxxl.fr/commands";
	}
}

// ---------------------------------------------------------------------------
//  #help
// ---------------------------------------------------------------------------
class FFRX_HelpCommand : ScrServerCommand
{
	override string GetKeyword() { return "help"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		int section = -1;
		if (argv && argv.Count() > 1)
			section = argv[1].ToInt() ;

		// "#help" sans argument -> l'index. `ToInt()` rend 0 sur une saisie non numerique,
		// ce qui afficherait la rubrique 0 par surprise ; on n'accepte donc un numero que
		// s'il a bien ete tape.
		if (argv && argv.Count() > 1 && argv[1] != "0" && section == 0)
			section = -1;

		return ScrServerCmdResult(FFRX_Help.Render(section), EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)
	{
		return ScrServerCmdResult(FFRX_Help.RenderIndex(), EServerCmdResultType.OK);
	}

	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate() { return ScrServerCmdResult("", EServerCmdResultType.OK); }
}
