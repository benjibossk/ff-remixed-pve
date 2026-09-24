// FF - REMIXED - PVE
// Les civils ne conduisent plus de vehicules militaires.
//
// ======================================================================================
//  LE PROBLEME
// ======================================================================================
//
// Observe en jeu : des civils au volant d'UAZ a mitrailleuse et de camions militaires MEI.
//
// Cause, remontee jusqu'a la source. La faction CIVILE en jeu sur Anizay est MEC, et son
// catalogue de vehicules (Configs/Factions/MEC.conf, mod MEI) pointe sur :
//
//     Configs/EntityCatalog/FIA/Vehicles_EntityCatalog_FIA.conf
//
// ... c'est-a-dire le catalogue MILITAIRE de la FIA. Il contient BTR70, BRDM2,
// UAZ469_PKM, UAZ469_UK59, et les Ural en versions munitions / arsenal / commandement /
// genie / reparation.
//
// Et le filtre de FF ne regarde QUE le type de chassis
// (JWK_AmbientFactionTrafficHandler.InitPrefab) :
//
//     BICYCLE / CAR / MOTORBIKE  -> vehicule courant
//     TRUCK + CARGO ou TANKER    -> camion
//
// Aucun test d'armement ni de blindage. Un UAZ469_PKM est une "voiture" : il passe donc
// comme vehicule civil ordinaire, mitrailleuse comprise.
//
// ======================================================================================
//  POURQUOI ON CORRIGE ICI ET PAS AILLEURS
// ======================================================================================
//
// Trois points d'accroche etaient possibles. Deux sont INTERDITS :
//
//     JWK_AmbientFactionTrafficHandler  -> cite dans BaseTrait_Common.conf et deux autres
//     JWK_AmbientTrafficSystem          -> cite dans ChimeraSystemsConfig.conf
//
// Modder une classe citee nommement dans un .conf la rend introuvable pour le moteur, qui
// jette l'entree EN SILENCE (cf. l'avertissement en tete de FFRX_TrimMainMenu.c : c'est ce
// qui avait tue la touche J pendant trois sessions). On ne les touche pas.
//
// JWK_AmbientVehicleEventSpawner, lui, n'apparait dans AUCUN .conf : il est cree en code
// (`m_Spawner = new JWK_AmbientVehicleEventSpawner()`). C'est donc le seul endroit sur, et
// il se trouve etre exactement le bon : sa methode GenerateRandomVehiclePrefab() est le
// point de passage unique par lequel le systeme obtient un vehicule a faire apparaitre.
//
// On ne corrige PAS le catalogue de MEI : c'est son fichier, un override complet nous
// exposerait a perdre son contenu a chaque mise a jour. On filtre a la sortie.
//
// NOTE : ASCII uniquement dans les chaines (le dedie compile en strict).

modded class JWK_AmbientVehicleEventSpawner
{
	//! Nombre de re-tirages avant d'abandonner. Le tirage etant aleatoire dans une liste
	//! majoritairement civile, quelques essais suffisent ; on borne pour ne jamais boucler.
	protected static const int MAX_TRIES = 8;

	// Observabilite : sans compteur, on ne distingue pas "le filtre marche" de "le filtre
	// ne s'execute pas". Rapporte au premier refus puis tous les 25.
	protected static int s_iRejected;
	protected static int s_iPassed;

	//------------------------------------------------------------------------------------------------
	override ResourceName GenerateRandomVehiclePrefab(string factionKey)
	{
		ResourceName prefab = super.GenerateRandomVehiclePrefab(factionKey);

		// Le filtre ne vise QUE le trafic civil : une patrouille ennemie a parfaitement le
		// droit de rouler en UAZ arme, c'est meme souhaitable.
		if (!FFRX_IsCivilianFaction(factionKey))
			return prefab;

		int tries = 0;
		while (prefab != ResourceName.Empty && FFRX_IsMilitary(prefab) && tries < MAX_TRIES)
		{
			s_iRejected++;
			if (s_iRejected == 1 || (s_iRejected % 25) == 0)
			{
				PrintFormat("[FFRX][Traffic] vehicule militaire refuse pour la faction civile %1 : %2 (refus cumules %3, acceptes %4)",
					factionKey, JWK_PrefabUtils.GetShortPrefabName(prefab), s_iRejected, s_iPassed);
			}

			prefab = super.GenerateRandomVehiclePrefab(factionKey);
			tries++;
		}

		// Garde-fou : si apres MAX_TRIES la liste ne rend toujours que du militaire, on
		// LAISSE PASSER plutot que de rendre vide. Un civil en UAZ arme reste moins grave
		// qu'un monde sans aucun trafic -- et la ligne ci-dessous nous dit que le catalogue
		// de la faction est a revoir, ce qu'un silence ne ferait pas.
		if (prefab != ResourceName.Empty && FFRX_IsMilitary(prefab))
		{
			PrintFormat("[FFRX][Traffic] ATTENTION : apres %1 essais, la faction civile %2 ne propose que du militaire. Catalogue a revoir.",
				MAX_TRIES, factionKey);
			return prefab;
		}

		s_iPassed++;
		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Arme ou blinde = pas un vehicule de civil.
	//!
	//! On accepte volontairement UNDEFINED : un vehicule non classe par FF passe. Rejeter
	//! l'inconnu viderait le trafic des qu'un mod ajoute des vehicules sans attributs --
	//! une panne bien pire que le defaut qu'on corrige.
	protected bool FFRX_IsMilitary(ResourceName prefab)
	{
		if (prefab == ResourceName.Empty)
			return false;

		if (!JWK.GetVehicles())
			return false;

		JWK_VehicleAttributes at = JWK.GetVehicles().GetAttributesForPrefab(prefab);
		if (!at)
			return false;

		if (at.m_iArmamentType != JWK_EVehicleArmamentType.UNDEFINED
			&& at.m_iArmamentType != JWK_EVehicleArmamentType.UNARMED)
			return true;

		if (at.m_iArmorType != JWK_EVehicleArmorType.UNDEFINED
			&& at.m_iArmorType != JWK_EVehicleArmorType.UNARMORED)
			return true;

		// Camions de commandement, de reparation, de ravitaillement : non armes, non
		// blindes, mais clairement pas des vehicules de civils.
		if (at.m_iSpecialType == JWK_EVehicleSpecialType.COMMAND
			|| at.m_iSpecialType == JWK_EVehicleSpecialType.REPAIR
			|| at.m_iSpecialType == JWK_EVehicleSpecialType.RESUPPLY)
			return true;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Cette faction est-elle civile ? On passe par le ROLE plutot que par la cle, pour ne
	//! pas coder en dur "MEC" / "CIV" : le nom change d'une carte a l'autre (Anizay utilise
	//! MEC, Everon utilise CIV), le role AMBIENT non.
	protected bool FFRX_IsCivilianFaction(string factionKey)
	{
		if (!JWK.GetFactions())
			return false;

		// On demande la faction qui TIENT le role AMBIENT et on compare les cles. L'API ne
		// propose pas de chemin cle -> role ; ce detour l'evite sans coder "MEC" en dur.
		JWK_Faction ambient = JWK.GetFactions().GetJWKFactionByRole(JWK_EFactionRole.AMBIENT);
		if (!ambient)
			return false;

		return ambient.GetKey() == factionKey;
	}
}
