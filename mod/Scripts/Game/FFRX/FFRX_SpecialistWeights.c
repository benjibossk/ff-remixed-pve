// FF - REMIXED - PVE
// FREQUENCE DES EQUIPES SPECIALISTES : un pourcentage par profil, reglable en jeu.
//
// Sans ca, FF tire ses groupes d'infanterie a la plate :
//
//     int groupIndex = possibleGroups.GetRandomIndex();   // JWK_FactionForceCompositionGenerator
//
// Tirage UNIFORME sur les entrees DISTINCTES -- et les doublons ne comptent pas, donc
// repeter un prefab dans le .conf pour le rendre plus frequent ne sert a rien (mesure du
// 2026-09-11 : 4 patrouilles repetees 5 fois + 4 specialistes = 8 entrees effectives, et
// 59 % de specialistes au lieu des 17 % voulus). La frequence d'une equipe n'etait donc
// pas reglable du tout : elle valait 1/N, point.
//
// ------------------------------------------------------------------------------------
// COMMENT
//
// On surcharge le generateur de composition -- il n'est cite dans AUCUN [Friend] de FF,
// contrairement a JWK_AIForce et consorts, donc le modder ne casse rien (verifie).
// C'est lui qui alimente les patrouilles de ville, les garnisons, les checkpoints, les
// camps, les QRF et les vagues de bataille : tout l'infanterie ennemie passe par la.
//
// A chaque groupe a produire, on tire d'abord nos pourcentages. Si l'un sort, on impose
// l'equipe correspondante ; sinon on laisse FF faire son tirage habituel, intact.
//
// ------------------------------------------------------------------------------------
// POURQUOI LES EQUIPES NE SONT PLUS DANS LES LISTES DE FF_MEI.conf
//
// Elles en ont ete RETIREES volontairement. Si elles y restaient, elles auraient DEUX
// chances de sortir : la notre, puis celle du tirage uniforme de FF -- et le pourcentage
// affiche sur le curseur ne voudrait plus rien dire. Elles n'apparaissent donc plus que
// par ce fichier, ce qui rend le reglage exact et, accessoirement, remet le comportement
// strictement vanilla quand tous les curseurs sont a 0.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict, cf. memoire).

class FFRX_SpecialistWeights
{
	static const ResourceName GROUP_DRONE = "{6FFEC0DEDA000215}Prefabs/Groups/MEI/Group_MEI_DroneTeam.et";
	static const ResourceName GROUP_EW    = "{6FFEC0DEDA000216}Prefabs/Groups/MEI/Group_MEI_EWTeam.et";
	static const ResourceName GROUP_AA    = "{6FFEC0DEDA000217}Prefabs/Groups/MEI/Group_MEI_AATeam.et";
	static const ResourceName GROUP_AT    = "{6FFEC0DEDA000218}Prefabs/Groups/MEI/Group_MEI_ATTeam.et";

	// Equipes dont le spawn a echoue (prefab illisible, budget jamais suffisant...). On
	// cesse de les imposer : sans ce garde-fou, l'appelant de GenerateInfantryGroup
	// boucle sur `while (!consumed && !possibleGroups.IsEmpty())` et on le ferait tourner
	// indefiniment en lui reproposant toujours la meme equipe defaillante.
	protected static ref map<string, bool> s_mFailed;

	// ---------------------------------------------------------------------------------
	//  COMPTEURS DE DIAGNOSTIC (2026-09-19)
	// ---------------------------------------------------------------------------------
	// Pourquoi : le recensement du 19/09 a montre 6 equipes specialistes sur 85 groupes
	// (7 %) alors que les curseurs somment 20 %. Deux causes possibles, indiscernables
	// depuis le seul recensement :
	//   - le garde-fou `budgetLeft < SPECIALIST_TEAM_SIZE` saute beaucoup de tirages
	//     (Reoccupation peuple ses postes-frontieres avec 2 hommes) ;
	//   - une equipe en particulier ne sort jamais.
	// Le recensement compte des spawns REUSSIS ; il ne voit donc ni les tirages sautes,
	// ni les tirages qui n'ont pas abouti. Ces compteurs-la comptent les DECISIONS.
	//
	// A lire dans le rapport `[FFRX][Census] SPECIALISTES`. Interpretation :
	//   - `sautes` eleve  -> c'est le garde-fou, le curseur ne peut pas tenir sa promesse ;
	//   - `tirages` proche du total attendu mais une equipe a 0 -> la elle est vraiment en cause ;
	//   - `tirages` a 0   -> la ponderation ne s'execute pas du tout (override inactif).
	protected static int s_iCalls;        // appels a Pick()
	protected static int s_iSkippedBudget; // tirages jamais tentes faute de budget
	protected static int s_iDrone;
	protected static int s_iAA;
	protected static int s_iEW;
	protected static int s_iAT;
	protected static int s_iOrdinary;     // le tirage est retombe sur une patrouille ordinaire

	//------------------------------------------------------------------------------------------------
	//! Appele par le garde-fou de budget, AVANT tout tirage.
	static void NoteSkippedBudget()
	{
		s_iSkippedBudget = s_iSkippedBudget + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne pour le rapport de recensement.
	static string Render()
	{
		string txt = "[FFRX][Census] SPECIALISTES -- tirages " + s_iCalls.ToString()
			+ "  sautes (budget) " + s_iSkippedBudget.ToString();

		txt = txt + "\n  drone   " + s_iDrone.ToString();
		txt = txt + "\n  sol-air " + s_iAA.ToString();
		txt = txt + "\n  elec    " + s_iEW.ToString();
		txt = txt + "\n  antichar " + s_iAT.ToString();
		txt = txt + "\n  ordinaire " + s_iOrdinary.ToString();

		return txt;
	}

	//------------------------------------------------------------------------------------------------
	//! Tire une equipe specialiste selon les pourcentages regles en jeu, ou "" si le
	//! tirage retombe sur une patrouille ordinaire.
	//!
	//! Les quatre pourcentages sont cumulatifs : 8 + 6 + 4 + 2 = 20 % de chances qu'un
	//! groupe donne soit une equipe specialiste, et 80 % qu'il reste ordinaire.
	static ResourceName Pick()
	{
		s_iCalls = s_iCalls + 1;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return ResourceName.Empty;

		float drone = cache.m_fFFRX_SpecDrone;
		float aa    = cache.m_fFFRX_SpecAA;
		float ew    = cache.m_fFFRX_SpecEW;
		float at    = cache.m_fFFRX_SpecAT;

		float total = drone + aa + ew + at;
		if (total <= 0)
		{
			s_iOrdinary = s_iOrdinary + 1;
			return ResourceName.Empty; // tous les curseurs a 0 = comportement vanilla
		}

		// Un seul tirage sur 100, reparti en tranches. Au-dela de la somme des poids,
		// c'est une patrouille ordinaire.
		float roll = JWK.Random.RandFloat01() * 100;

		// NOTE : on compte l'equipe TIREE, pas l'equipe spawnee. Une equipe comptee ici mais
		// absente du tableau GROUPES a donc bien ete choisie et n'a pas abouti -- c'est
		// exactement la distinction qu'on cherche a faire.
		float edge = drone;
		if (roll < edge)
		{
			s_iDrone = s_iDrone + 1;
			return NotFailed(GROUP_DRONE);
		}

		edge = edge + aa;
		if (roll < edge)
		{
			s_iAA = s_iAA + 1;
			return NotFailed(GROUP_AA);
		}

		edge = edge + ew;
		if (roll < edge)
		{
			s_iEW = s_iEW + 1;
			return NotFailed(GROUP_EW);
		}

		edge = edge + at;
		if (roll < edge)
		{
			s_iAT = s_iAT + 1;
			return NotFailed(GROUP_AT);
		}

		s_iOrdinary = s_iOrdinary + 1;
		return ResourceName.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected static ResourceName NotFailed(ResourceName prefab)
	{
		if (s_mFailed && s_mFailed.Contains(prefab))
			return ResourceName.Empty;
		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Marque une equipe comme non spawnable : on ne la proposera plus.
	static void MarkFailed(ResourceName prefab)
	{
		if (!s_mFailed)
			s_mFailed = new map<string, bool>();

		if (s_mFailed.Contains(prefab))
			return;

		s_mFailed.Set(prefab, true);
		Print("[FFRX][Spec] " + prefab + " ne spawne pas -- retire du tirage pondere.", LogLevel.WARNING);
	}
}

// ---------------------------------------------------------------------------
//  Le generateur de composition de FF. Aucun [Friend] ne le cite : modder cette
//  classe-la est sans danger, contrairement a JWK_AIForce (cf. FFRX_SpawnCensus.c).
// ---------------------------------------------------------------------------
modded class JWK_FactionForceCompositionGenerator
{
	//! Effectif d'une equipe specialiste : le servant + 2 d'escorte. Sert de budget
	//! minimum pour accepter une substitution (cf. GenerateInfantryGroup).
	protected static const int SPECIALIST_TEAM_SIZE = 3;


	//! Patrouilles de ville (JWK_TownMilitaryActivityComponent). Au passage, l'original
	//! recoit un parametre `threat` qu'il n'utilise PAS -- il se contente d'un
	//! GetRandomElement. On ne fait donc perdre aucune logique en le surchargeant.
	override ResourceName GetPatrolGroup(float threat)
	{
		ResourceName forced = FFRX_SpecialistWeights.Pick();
		if (forced != ResourceName.Empty)
			return forced;

		return super.GetPatrolGroup(threat);
	}

	//! Compositions : garnisons, checkpoints, camps, QRF, vagues de bataille.
	override protected int GenerateInfantryGroup(
		JWK_FactionForceInfantryComposition result,
		array<ResourceName> possibleGroups,
		int budgetLeft
	) {
		// PAS DE SUBSTITUTION SUR LES PETITS BUDGETS -- lire avant de retirer ce test.
		//
		// Reoccupation 6.2.0 peuple ses postes-frontieres avec exactement cet appel :
		//     generator.GenerateInfantry(2, 2, intent: MILITARY_CHECKPOINT, ...)
		// (FFML_BorderRoles.GroundSeed et FFML_BorderStation). Sans ce garde-fou, un poste
		// sur cinq verrait son escouade de garde remplacee par une equipe drone ou sol-air,
		// TRONQUEE a 2 hommes faute de budget (super fait members.Resize(budgetLeft)) --
		// donc un poste ampute de son inspecteur, et la mecanique d'inspection du nouvel
		// update cassee en silence.
		//
		// Nos equipes comptent 3 hommes (specialiste + 2 d'escorte). En dessous, on laisse
		// FF composer : une equipe a moitie spawnee n'a de toute facon aucun interet.
		if (budgetLeft < SPECIALIST_TEAM_SIZE)
		{
			FFRX_SpecialistWeights.NoteSkippedBudget();
			return super.GenerateInfantryGroup(result, possibleGroups, budgetLeft);
		}

		ResourceName forced = FFRX_SpecialistWeights.Pick();
		if (forced == ResourceName.Empty)
			return super.GenerateInfantryGroup(result, possibleGroups, budgetLeft);

		// On impose l'equipe en ne presentant qu'elle. super fait le reste comme
		// d'habitude : lecture des membres, budget, troncature si necessaire.
		array<ResourceName> only = {};
		only.Insert(forced);

		int consumed = super.GenerateInfantryGroup(result, only, budgetLeft);
		if (consumed > 0)
			return consumed;

		// Echec : soit le prefab est illisible, soit il ne rentre pas dans ce budget.
		// Si super l'a lui-meme ecarte, c'est qu'il est defectueux -> on ne le proposera
		// plus. Sinon on laisse simplement FF tirer normalement pour ce groupe-ci, ce qui
		// garantit que la boucle appelante progresse.
		if (only.IsEmpty())
			FFRX_SpecialistWeights.MarkFailed(forced);

		return super.GenerateInfantryGroup(result, possibleGroups, budgetLeft);
	}
}
