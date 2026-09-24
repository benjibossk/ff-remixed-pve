// FF - REMIXED - PVE
// Garde-fou : une faction sans catalogue d'un type donne ne doit pas faire planter FF.
//
// ------------------------------------------------------------------------------------
// LE BUG (dedie, 2026-09-09)
//
//   SCRIPT (E): Virtual Machine Exception -- NULL pointer to instance
//   Class: 'JWK_ShopManagerComponent'  Function: 'LoadEntityCatalogAttributes'
//   ... OnFactionsPreSetup <- NotifyFactionsPreSetup <- SetupFactions <- LoadState
//
// 33 592 fois au chargement de la sauvegarde. FF ecrit ceci :
//
//   SCR_EntityCatalog catalog = faction.GetFactionEntityCatalogOfType(cat);
//   catalog.GetEntityListWithData(...);        // <- aucun test de nullite
//
// Or il boucle sur les types ITEM et VEHICLE pour CHAQUE faction. Les factions du mod
// MiddleEastInsurgents -- MEI (ennemi) et MEC (ambiant), toutes deux utilisees sur
// Anizay -- ne declarent PAS de catalogue d'objets : elles n'ont que Characters,
// Vehicles, Groups et WeaponTripod. Le getter renvoie donc null, et FF le dereference.
//
// A noter : le jeu de base, lui, teste toujours (cf. SCR_AmbientPatrolSpawnPointComponent
// l.235 "if (!entityCatalog) return;"). C'est bien un oubli de FF. A remonter a Johnny.
//
// CONSEQUENCE : l'exception interrompt SetupFactions EN PLEIN MILIEU. Les factions ne
// sont donc que partiellement configurees, et tout ce qui en depend part de travers --
// c'est tres probablement pourquoi le menu de groupes affiche "Unknown / --- MHz" en
// jeu alors que le serveur, lui, connait parfaitement les groupes (le site affiche bien
// "Premiere Compagnie").
//
// ------------------------------------------------------------------------------------
// POURQUOI CE CORRECTIF-LA
//
// Trois voies etaient possibles :
//   1. Ajouter un catalogue d'objets aux configs MEI/MEC -> il faudrait shadow-overrider
//      les .conf d'un MOD TIERS, avec le risque de collision qu'on vient de decouvrir
//      sur le Mi-8 (deux mods sur un meme GUID = l'un ecrase l'autre), et ca casserait
//      a chaque mise a jour du mod.
//   2. Surcharger LoadEntityCatalogAttributes de FF -> la methode est `protected` et
//      longue ; la recopier nous exposerait a diverger a chaque version de FF.
//   3. Rendre le getter INOFFENSIF quand il n'a rien a rendre. <- retenu.
//
// On renvoie un catalogue VIDE au lieu de null. Semantiquement c'est exact -- "cette
// faction n'a aucun objet" -- et ca vaut pour TOUT appelant, present ou futur, sans
// dependre d'un mod ni d'une version de FF.
//
// Le catalogue vide est mis en cache et partage : il est en lecture seule pour les
// appelants (ils l'iterent), donc une seule instance suffit et on evite d'en allouer
// une a chaque appel -- l'appel se produit des milliers de fois au chargement.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

// ------------------------------------------------------------------------------------
// Un SCR_EntityCatalog cree par `new` a ses listes internes A NULL : elles ne sont
// remplies que par la deserialisation d'un .conf. Fournir un tel objet a la place de
// null deplacait donc simplement l'exception -- de "catalogue null" vers
// "SCR_EntityCatalog.GetEntityListWithData : NULL pointer" (constate : 33 592 exceptions
// devenues 56, mais toujours 56 de trop).
//
// m_aEntityEntryList est `protected` : un modded class fait partie de la classe et peut
// donc l'initialiser. C'est la seule facon d'obtenir un catalogue VIDE mais PARCOURABLE.
// ------------------------------------------------------------------------------------
// ⚠️ ATTRIBUT DE CLASSE REDECLARE -- NE PAS RETIRER.
// Un `modded class` qui omet l'attribut de l'original perd sa deserialisation.
// Le compilateur ne le dit PAS : il rend des dizaines de "Too many instructions
// per function" et "Incompatible parameter" sur des fichiers du JEU DE BASE et de
// FF (JWK_ConvoyAIDeployer, JWK_ShopContext...), aucun ne citant ce fichier, puis
// "Can't compile Game script module!". Panne du dedie le 2026-09-15.
[BaseContainerProps(configRoot: true), SCR_BaseContainerCustomEntityCatalogCatalog(EEntityCatalogType, "m_eEntityCatalogType", "m_aEntityEntryList", "m_aMultiLists")]
modded class SCR_EntityCatalog
{
	//! Rend ce catalogue sur a parcourir meme s'il n'a jamais ete deserialise.
	void FFRX_EnsureUsable()
	{
		if (!m_aEntityEntryList)
			m_aEntityEntryList = {};
	}
}

modded class SCR_Faction
{
	//! Catalogue vide partage, cree a la demande.
	protected static ref SCR_EntityCatalog s_FFRXEmptyCatalog;

	override SCR_EntityCatalog GetFactionEntityCatalogOfType(EEntityCatalogType catalogType, bool printNotFound = true)
	{
		// printNotFound=false : sans ca, remplacer null par un catalogue vide ferait
		// disparaitre l'avertissement d'origine tout en gardant son spam. On veut le
		// silence ici et le diagnostic dans notre propre log, une seule fois par
		// (faction, type).
		SCR_EntityCatalog cat = super.GetFactionEntityCatalogOfType(catalogType, false);
		if (cat)
			return cat;

		FFRX_WarnOnce(catalogType);

		if (!s_FFRXEmptyCatalog)
			s_FFRXEmptyCatalog = new SCR_EntityCatalog();
		// Indispensable : sans ca ses listes internes sont nulles et l'appelant plante
		// en les parcourant (cf. le commentaire sur SCR_EntityCatalog plus haut).
		s_FFRXEmptyCatalog.FFRX_EnsureUsable();
		return s_FFRXEmptyCatalog;
	}

	//! Un avertissement par couple (faction, type) : on veut savoir quelle faction est
	//! incomplete, pas 33 000 lignes identiques.
	protected static ref map<string, bool> s_mFFRXWarned;

	protected void FFRX_WarnOnce(EEntityCatalogType catalogType)
	{
		if (!s_mFFRXWarned)
			s_mFFRXWarned = new map<string, bool>();

		string key = GetFactionKey() + "/" + catalogType;
		if (s_mFFRXWarned.Contains(key))
			return;
		s_mFFRXWarned.Set(key, true);

		Print(string.Format("[FFRX][Catalog] Faction '%1' n'a pas de catalogue '%2' -> catalogue vide fourni (evite le NULL pointer de JWK_ShopManagerComponent).",
			GetFactionKey(), typename.EnumToString(EEntityCatalogType, catalogType)), LogLevel.WARNING);
	}
}
