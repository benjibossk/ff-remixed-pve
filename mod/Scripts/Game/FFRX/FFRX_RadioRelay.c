// FF - REMIXED - PVE
// SAC RADIO LONGUE PORTEE -- le poste qui etire la couverture de son porteur.
//
// ======================================================================================
//  CE QU'IL FAIT, ET POURQUOI IL EXISTE
// ======================================================================================
// Depuis le 21-22/09, la distance aux tours radio qu'on tient decide de deux choses :
//   - la fraicheur de la position d'un soldat sur la livemap (FFRX_LivemapSignal) ;
//   - la cadence et la precision des balises GPS (FFRX_Beacon).
//
// C'etait une contrainte SUBIE : loin des relais, on decroche, point. Le sac radio est la
// reponse que le joueur peut PORTER.
//
// ⚠️ LE FACTEUR N'EST PAS INVENTE -- il vient des portees du JEU DE BASE (releve du
// 2026-09-23, question de Benji "la portee native n'est-elle pas plus grande ?") :
//
//   1 300 m : postes de poitrine (ANPRC-68, R-148, ER328 AMF du spawn)
//   2 000 m : postes de SAC      (ANPRC-77, R-107M, radio deployable)
//
// 2000 / 1300 = 1,54. C'est exactement l'ecart que le jeu met entre "radio de poitrine" et
// "radio de sac", et c'est celui qu'on applique. Une premiere version utilisait 2,5, chiffre
// sorti de nulle part : elle rendait le sac plus fort que tout ce que le moteur propose.
//
// C'est ce qui rend une escouade d'appui utile a autre chose qu'a elle-meme : celui qui
// porte le sac tient la liaison du groupe. Et ca donne une VRAIE cible a l'ennemi.
//
// ======================================================================================
//  CE QU'IL NE FAIT PAS
// ======================================================================================
// Il n'invente pas de relais. Si la resistance ne tient AUCUNE tour radio operable,
// `NearestFriendlySiteDistance()` rend -1 et le niveau reste "hors de portee" quel que soit
// le sac : un poste puissant sans correspondant ne parle a personne. C'est explicite dans
// `FFRX_BeaconSignal.Level()` et c'est voulu -- sinon le sac remplacerait la conquete des
// tours, alors qu'il doit la RECOMPENSER.
//
// ======================================================================================
//  COMMENT ON LE DETECTE
// ======================================================================================
// Par un composant marqueur (`FFRX_RadioRelayComponent`) pose sur le prefab du sac, et non
// par un nom ou un GUID de prefab : ca laisse la porte ouverte a d'autres modeles de sacs
// plus tard (un poste vehicule, un relais portable) sans rien changer ici.
//
// On le cherche dans TOUT l'inventaire du porteur et pas seulement dans le slot du dos :
// un sac pose dans un vehicule ou tenu a la main compte aussi. Ce qui compte est qu'il soit
// AVEC lui.
//
// ⚠️ Cout : ce test tourne a chaque envoi de position (toutes les quelques secondes, par
// joueur). `GetItems()` parcourt l'inventaire complet -- acceptable a cette cadence et pour
// le nombre de joueurs d'un serveur FF, mais a surveiller si on l'appelle un jour par trame.
//
// Serveur uniquement. Chaines ASCII.

class FFRX_RadioRelayComponentClass : ScriptComponentClass
{
}

//! Composant marqueur. A poser sur le prefab du sac radio -- il ne fait rien d'autre
//! qu'exister et porter son multiplicateur.
class FFRX_RadioRelayComponent : ScriptComponent
{
	//! 1.54 = le rapport poste de sac / poste de poitrine du jeu de base (2000 / 1300).
	//! Avec les paliers actuels : signal franc jusqu'a 2 km, tenu jusqu'a 7,7 km.
	[Attribute("1.54", desc: "Multiplicateur de portee radio pour le porteur (1.54 = rapport sac/poitrine du jeu de base)")]
	protected float m_fRangeMultiplier;

	//------------------------------------------------------------------------------------------------
	float FFRX_RangeMultiplier()
	{
		if (m_fRangeMultiplier < 1)
			return 1;

		return m_fRangeMultiplier;
	}
}

// ---------------------------------------------------------------------------
class FFRX_RadioRelay
{
	//------------------------------------------------------------------------------------------------
	//! Multiplicateur de portee d'un porteur : 1 s'il n'a pas de sac radio.
	//! Si plusieurs sacs sont presents, on garde le MEILLEUR (on ne les cumule pas -- deux
	//! postes ne portent pas deux fois plus loin).
	static float MultiplierFor(IEntity carrier)
	{
		if (!carrier)
			return 1;

		InventoryStorageManagerComponent inv = InventoryStorageManagerComponent.Cast(
			carrier.FindComponent(InventoryStorageManagerComponent));
		if (!inv)
			return 1;

		array<IEntity> items = {};
		inv.GetItems(items);

		float best = 1;

		foreach (IEntity it : items)
		{
			if (!it)
				continue;

			FFRX_RadioRelayComponent relay = FFRX_RadioRelayComponent.Cast(
				it.FindComponent(FFRX_RadioRelayComponent));
			if (!relay)
				continue;

			float m = relay.FFRX_RangeMultiplier();
			if (m > best)
				best = m;
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Niveau de signal d'un porteur, sac compris. Point d'entree unique pour tout ce qui
	//! veut juger la liaison d'un JOUEUR (par opposition a un objet pose).
	static int LevelFor(IEntity carrier)
	{
		if (!carrier)
			return 3;

		return FFRX_BeaconSignal.Level(carrier.GetOrigin(), MultiplierFor(carrier));
	}
}

// ---------------------------------------------------------------------------
//  TOURS EMETTRICES CONSTRUITES PAR LES JOUEURS
// ---------------------------------------------------------------------------
// ⚠️ LE TROU QUE CA BOUCHE (constate le 2026-09-23, question de Benji). Les trois tours
// emettrices etaient DEJA constructibles (Configs/Construction/BuildItems.conf), mais
// `FFRX_RadioIntel.NearestFriendlySiteDistance()` ne regarde que les `JWK_RadioSiteEntity`
// de FF. Une tour posee par un joueur etait donc INVISIBLE pour le signal : on payait
// jusqu'a 400 de ravitaillement pour un decor. Personne ne s'en serait apercu, puisque rien
// n'echouait -- ca ne faisait simplement rien.
//
// COMMENT ON LES RECONNAIT. Par leur `ScriptedRadioComponent` et sa portee declaree, pas par
// un GUID de prefab : le jeu de base decline trois tailles (4000 / 4500 / 5000 m) et un mod
// pourrait en ajouter. On lit la portee REELLE de l'emetteur plutot que de la supposer.
//
// CE QU'ELLES APPORTENT. Une tour couvre autour d'elle a hauteur de sa portee. On la traite
// donc comme un relais ami : si le joueur est dans son rayon, le signal est FRANC, quelle
// que soit la distance a la tour radio de FF la plus proche. C'est ce qui donne enfin un
// interet a construire -- et un interet a defendre ce qu'on a construit.
//
// ⚠️ PAS DE CONTROLE DE FACTION sur ces tours. Un `StaticModelEntity` n'a pas de
// `JWK_FactionControlComponent` : on ne peut pas savoir a qui elle appartient. On part donc
// du principe qu'une tour DEBOUT sert tout le monde -- ce qui est vrai d'une antenne. Si
// l'ennemi en pose une, elle profite aussi aux joueurs. C'est assume : dans les faits ce
// sont les joueurs qui construisent, et une antenne ennemie a detruire reste un objectif
// lisible plutot qu'une incoherence.
class FFRX_RadioTowers
{
	//! Re-scan periodique : les tours ne bougent pas, mais on en construit et on en detruit.
	protected static const int RESCAN_MS = 120000;

	// ⚠️ Pas de `= {}` sur un champ statique : ces initialiseurs sont hisses dans l'unique
	// fonction d'init du module de script, dont le buffer de 64 Ko est partage par vanilla et
	// TOUS les mods (~1000 unites au total, ~6 par static). Son debordement produit les
	// "Too many instructions per function" sur des fichiers du jeu de base. Cf. la memoire
	// `enfusion-script-compile-ceiling`. On construit donc a la premiere utilisation.
	protected static ref array<IEntity> s_aTowers;
	protected static ref array<float>   s_aRanges;

	protected static void EnsureLists()
	{
		if (!s_aTowers)
			s_aTowers = new array<IEntity>();
		if (!s_aRanges)
			s_aRanges = new array<float>();
	}
	protected static bool s_bScanned;

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;

		GetGame().GetCallqueue().CallLater(Rescan, RESCAN_MS, true);
		GetGame().GetCallqueue().CallLater(Rescan, 30000, false);
	}

	//------------------------------------------------------------------------------------------------
	//! true si `pos` est dans le rayon d'une tour emettrice debout.
	static bool InTowerCoverage(vector pos)
	{
		if (!s_bScanned)
			return false;

		EnsureLists();

		for (int i = 0; i < s_aTowers.Count(); i++)
		{
			IEntity t = s_aTowers[i];
			if (!t || t.IsDeleted())
				continue;

			// ⚠️ Une tour DETRUITE reste une entite presente : tester `IsDeleted()` seul
			// laissait une antenne en ruines fournir de la couverture. FF, lui, coupe son
			// site des que la tour tombe (`JWK_RadioSiteEntity.OnTowerDestroyed_S`). On
			// s'aligne : une antenne abattue ne relaie plus rien, ce qui en fait une cible.
			if (IsDestroyed(t))
				continue;

			if (vector.Distance(pos, t.GetOrigin()) <= s_aRanges[i])
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! true si l'entite est une structure de la carte qu'on a decide de traiter en relais.
	//! On compare sur le NOM de prefab et non sur un GUID : la carte decline parfois des
	//! variantes (materiaux locaux, versions "_anizay"), qui partagent le nom mais pas le GUID.
	protected static bool IsPromotedStructure(IEntity ent)
	{
		EntityPrefabData pd = ent.GetPrefabData();
		if (!pd)
			return false;

		string name = pd.GetPrefabName();
		if (name == "")
			return false;

		foreach (string token : Promoted())
		{
			if (name.Contains(token))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! true si la structure est detruite. Sans gestionnaire de degats, on la considere intacte
	//! (certaines tours de decor ne sont tout simplement pas destructibles).
	protected static bool IsDestroyed(IEntity ent)
	{
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.Cast(
			ent.FindComponent(SCR_DamageManagerComponent));
		if (!dmg)
			return false;

		return dmg.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Rescan()
	{
		EnsureLists();
		s_aTowers.Clear();
		s_aRanges.Clear();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		// Requete large : les tours sont des entites STATIQUES posees n'importe ou sur la
		// carte. Cadence de 2 min, donc le cout reste negligeable -- contrairement au
		// balayage des epaves, qui lui devait etre decoupe en tuiles (cf. FFRX_TrappedWrecks).
		vector mins = Vector(-20000, -1000, -20000);
		vector maxs = Vector(20000, 3000, 20000);

		world.QueryEntitiesByAABB(mins, maxs, CollectTower, null, EQueryEntitiesFlags.STATIC);

		s_bScanned = true;

		if (!s_aTowers.IsEmpty())
			Print(string.Format("[FFRX][Radio] %1 tour(s) emettrice(s) prise(s) en compte.", s_aTowers.Count()), LogLevel.NORMAL);
	}

	// ------------------------------------------------------------------------------------
	//  STRUCTURES DE LA CARTE PROMUES EN RELAIS (decision Benji, 2026-09-23)
	// ------------------------------------------------------------------------------------
	// Anizay ne contient AUCUN emetteur : ni TransmitterTower, ni JWK_RadioSiteEntity
	// (verifie dans les layers de la carte et dans le log du dedie). Tout notre systeme de
	// signal etait donc en "hors de portee" permanent, et les depeches radio de Reoccupation
	// inertes depuis le debut sur cette carte.
	//
	// Il y a en revanche des TOURS DE CONTROLE d'aerodrome, deja posees, aux deux extremites
	// de la carte (3483/1614 a l'ouest, 8364/6995 a l'est). Emplacements credibles : c'est la
	// qu'un joueur s'attend a trouver une station radio.
	//
	// ⚠️ POURQUOI ON NE MODIFIE PAS LE PREFAB. L'idee de depart etait d'ajouter un composant
	// radio a `ControlTower_01.et`. Ecarte : le monde reference ce prefab par son GUID, donc
	// il faudrait un override AU MEME GUID -- qui REMPLACE le fichier. Or il fait 693 lignes
	// et 27 composants : le moindre oubli les perd EN SILENCE (piege deja paye sur le jammer
	// et les lunettes FPV), et il faudrait resynchroniser a chaque mise a jour du jeu de base.
	// Beaucoup de risque pour ajouter un composant.
	//
	// On les reconnait donc ICI, par leur prefab. Un seul fichier a maintenir, reversible,
	// et la portee est un reglage plutot qu'une donnee figee dans un .et.
	//
	// ⚠️ CE QUE CA NE DONNE PAS : ces tours alimentent NOTRE signal (livemap, balises), mais
	// elles ne deviennent pas des `JWK_RadioSiteEntity`. Donc pas de capture, pas d'affichage
	// de portee sur la carte de FF, et les depeches de Reoccupation restent inertes -- elles
	// exigent un vrai site FF. Pour ca il faudra poser des controleurs dans l'editeur.
	static const float MAP_TOWER_RANGE = 4000.0;

	//! Meme raison que ci-dessus : pas d'initialiseur statique immediat.
	protected static ref array<string> s_aPromoted;

	protected static array<string> Promoted()
	{
		if (!s_aPromoted)
		{
			s_aPromoted = new array<string>();
			s_aPromoted.Insert("ControlTower_01");
		}

		return s_aPromoted;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectTower(IEntity ent)
	{
		if (!ent)
			return true;

		BaseRadioComponent radio = BaseRadioComponent.Cast(ent.FindComponent(BaseRadioComponent));
		if (!radio)
		{
			// Pas d'emetteur, mais peut-etre une structure qu'on a decide de promouvoir.
			if (IsPromotedStructure(ent))
			{
				s_aTowers.Insert(ent);
				s_aRanges.Insert(MAP_TOWER_RANGE);
			}

			return true;
		}

		// Portee reelle du meilleur emetteur de la tour.
		float best = 0;
		int count = radio.TransceiversCount();
		for (int i = 0; i < count; i++)
		{
			BaseTransceiver tr = radio.GetTransceiver(i);
			if (!tr)
				continue;

			float r = tr.GetRange();
			if (r > best)
				best = r;
		}

		// On ne retient que les VRAIS relais. Une radio de poitrine tombee par terre porte
		// 1300 m : la compter ferait d'un objet perdu une antenne.
		if (best < 3000)
			return true;

		s_aTowers.Insert(ent);
		s_aRanges.Insert(best);
		return true;
	}
}
