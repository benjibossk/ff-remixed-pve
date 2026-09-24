// FF - REMIXED - PVE
// LIVEMAP -- la position d'un soldat se degrade quand il s'eloigne des relais qu'on tient.
//
// ======================================================================================
//  CE QUI EXISTAIT DEJA (et qu'on ne refait pas)
// ======================================================================================
// Fleet a DEJA un brouillard de guerre radio sur la livemap (`Flt_GTGPositions`, vers la
// ligne 883) :
//
//     int radio = Flt_RadioStatus(ent);   // 0 NONE / 1 RECEIVE / 2 SEND / 3 BOTH_WAYS
//     bool canSend = (radio >= 3);        // seule une liaison BIDIRECTIONNELLE passe
//     ... sinon : position FIGEE a la derniere connue
//
// C'est deja la bonne mecanique, et on la reutilise telle quelle : hors liaison, le site
// garde la derniere position connue au lieu de mentir. Exactement le choix fait pour les
// balises GPS.
//
// ======================================================================================
//  CE QUI MANQUAIT : LA DISTANCE AUX TOURS QU'ON TIENT
// ======================================================================================
// Le test de Fleet ne regarde que le MATERIEL du joueur (a-t-il une radio qui emet et
// recoit ?), via la couverture du moteur. Il ne sait rien des tours radio de FF, donc
// capturer une tour ne changeait rien a la livemap -- alors que c'est precisement la
// recompense qu'on veut donner (demande Benji : "le signal saccade plus il s'eloigne de la
// portee radio de la base").
//
// On surcharge donc `Flt_RadioStatus` pour y ajouter cette seconde condition. Le reste de
// Fleet est INCHANGE : c'est sa propre logique de gel qui fait le travail, on ne fait que
// lui dire plus souvent "pas de liaison".
//
// POURQUOI ICI ET PAS DANS FLEET : Fleet ne connait pas FF (aucune reference JWK, et ses
// dependances se limitent au jeu de base + AnarchyMarkers -- verifie le 2026-09-22). Il ne
// PEUT pas interroger les sites radio de FF. REMIXED, lui, depend des deux : c'est le seul
// endroit d'ou le pont est possible.
//
// ======================================================================================
//  L'EFFET : UNE POSITION QUI SACCADE, PAS QUI MENT
// ======================================================================================
// On reutilise les paliers des balises (`FFRX_BeaconSignal`), pour que les deux systemes
// parlent le meme langage :
//
//   <= 800 m d'un relais tenu : liaison normale, position vivante a chaque envoi
//   <= 2 km                   : 1 envoi sur 3 passe -> la position saute par a-coups
//   <= 4 km                   : 1 envoi sur 6
//   au-dela, ou aucun relais  : jamais -> derniere position connue, figee
//
// Entre deux passages, Fleet gele : le point ne bouge pas, puis fait un bond. C'est
// exactement l'effet voulu, et il est obtenu SANS toucher a la boucle d'envoi.
//
// ⚠️ Ce qu'on ne fait PAS ici : ajouter une ERREUR de position (le decalage de +/- 40 ou
// 120 m applique aux balises). Le crochet `Flt_RadioStatus` ne rend qu'un etat de liaison,
// il n'a pas la main sur le vecteur envoye. Pour brouiller aussi la position il faudrait
// entrer dans la boucle de Fleet, ce qui obligerait a en dupliquer une centaine de lignes --
// mauvais echange. Le saccadement seul porte deja l'essentiel de l'intention.
//
// ⚠️ ADMINS : rien n'est change pour eux cote SITE (la page admin lit ce qu'elle recoit).
// Si le suivi admin doit rester parfait, c'est un filtre a poser cote site, pas ici :
// degrader a la source et re-fabriquer la verite ensuite serait impossible.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict).

modded class Flt_GTGPositions
{
	//! Compteur d'envois par joueur. Sert a ne laisser passer qu'un envoi sur N quand le
	//! signal est faible.
	//!
	//! Indexe par ENTITE et non par `EntityID` : celui-ci n'expose pas de conversion en
	//! entier ("Undefined function 'EntityID.ToInt'"), il ne peut donc pas servir de cle.
	//! La table reste petite -- une entree par personnage joueur vivant -- et les entrees
	//! perimees (respawn) sont sans consequence : au pire un joueur repart avec un compteur
	//! a zero, ce qui ne fait que laisser passer son premier envoi.
	protected ref map<IEntity, int> m_mFFRXSignalTick;

	//------------------------------------------------------------------------------------------------
	override protected int Flt_RadioStatus(IEntity ent)
	{
		int base = super.Flt_RadioStatus(ent);

		// Pas de liaison materielle : Fleet gele deja, rien a ajouter.
		if (base < 3 || !ent)
			return base;

		// `LevelFor` et non `Level` : le porteur d'un sac radio longue portee voit ses paliers
		// etires (cf. FFRX_RadioRelay). C'est ce qui fait qu'une escouade garde sa liaison en
		// s'eloignant, a condition d'avoir emmene le poste.
		int level = FFRX_RadioRelay.LevelFor(ent);

		// Sous un relais tenu : liaison pleine, comportement d'origine.
		if (level <= 0)
			return base;

		// Hors de portee (ou aucune tour radio operable tenue) : on coupe. Fleet figera la
		// derniere position connue -- pas de position fausse, une position perimee.
		if (level >= 3)
			return 2;   // "SEND" : en dessous du seuil de Fleet, donc gele

		// Portee intermediaire : on ne laisse passer qu'un envoi sur N.
		if (!m_mFFRXSignalTick)
			m_mFFRXSignalTick = new map<IEntity, int>();

		int every = FFRX_BeaconSignal.TicksBetweenSends(level);

		int n = 0;
		m_mFFRXSignalTick.Find(ent, n);
		n = n + 1;

		if (n < every)
		{
			m_mFFRXSignalTick.Set(ent, n);
			return 2;   // gele ce tour-ci
		}

		m_mFFRXSignalTick.Set(ent, 0);
		return base;    // ce tour-ci passe : le point fait un bond
	}
}
