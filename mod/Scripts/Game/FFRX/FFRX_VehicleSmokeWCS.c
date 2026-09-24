// FF - REMIXED - PVE
// Vrai lance-fumigenes WCS pour les blindes ennemis, quand le vehicule en porte un.
//
// Complement de FFRX_VehicleSmokeScreen.c : celui-ci fait APPARAITRE la fumee autour du
// vehicule (marche partout, y compris sur les vehicules de mods qu'on ne modifie pas) ;
// celui-la DECLENCHE le vrai pot s'il est monte, avec le depart visible, le son et la
// dispersion prevus par WCS.
//
// ------------------------------------------------------------------------------------
// POURQUOI CE FICHIER EXISTE (et pourquoi j'avais dit que c'etait impossible)
//
// J'avais conclu trop vite qu'un lance-fumigenes ne pouvait pas etre declenche sans
// equipage : le pot vanilla est une arme de tourelle, tiree a la main. Mais WCS_Armaments
// embarque un vrai systeme de contre-mesures (leurres / chaff / fumigenes) avec un chemin
// SERVEUR autonome -- ni IA, ni servant, ni joueur. Piste trouvee par Benji.
//
// Le detail qui rend la chose possible : `FireDispensers_S` ne verifie QUE l'entite
// racine partagee entre le manager et le pot. Le lanceur peut donc etre accroche
// n'importe ou sur le vehicule ; il n'est PAS necessaire de remplacer la tourelle,
// contrairement a ce que suggere l'exemple BTR70 de WCS.
//
// ------------------------------------------------------------------------------------
// POURQUOI PASSER PAR UN modded class
//
// Les methodes publiques `FireSmoke()` / `FireSmokeFullRipple()` sont le chemin CLIENT :
// elles empilent une requete et l'envoient au serveur par RPC (`Rpc_DoFireDispensers_S`,
// RplRcver.Server). Les appeler depuis le serveur ne declencherait rien.
//
// Le point d'entree serveur est `FireDispensers_S`, mais il est `protected`, tout comme
// `PruneSmokeDispensers()` et `m_aSmokeDispensers`. Un `modded class` fait partie de la
// classe : il y accede legitimement et peut exposer un wrapper serveur public. C'est la
// seule facon propre -- pas de RPC detourne, pas de copie du code de WCS.
//
// ------------------------------------------------------------------------------------
// DEPENDANCE
//
// WCS_Armaments (629B2BA37EFFD577) n'est PAS declare dans notre addon.gproj, mais il
// arrive TRANSITIVEMENT : AIUsingStingers (68B2E735099BA541), lui, en depend et fait
// partie de nos dependances. Ses types sont donc compiles avant les notres.
// ⚠️ FRAGILITE ASSUMEE : si AIUsingStingers cessait un jour de dependre de WCS, ce
// fichier ne compilerait plus. Le correctif serait de declarer WCS explicitement dans
// notre .gproj -- ca ne coute rien puisqu'il est deja charge, et ca rend la dependance
// honnete. A faire via l'interface du Workbench (ne PAS editer le .gproj a la main).
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

modded class WCS_Armament_DispenserManagerComponent
{
	//! Tire TOUS les pots fumigenes du vehicule, cote serveur, sans equipage ni joueur.
	//! Retourne false s'il n'y a rien a tirer -- l'appelant peut alors se rabattre sur
	//! l'ecran de fumee simule.
	bool FFRX_FireSmokeAll_S()
	{
		if (!Replication.IsServer())
			return false;

		// Ecarte les pots detruits/vides avant de construire la salve (methode de WCS).
		PruneSmokeDispensers();

		if (!m_aSmokeDispensers || m_aSmokeDispensers.IsEmpty())
			return false;

		// FireDispensers_S identifie chaque pot par (RplId de son entite, id local du
		// composant) : deux pots du meme vehicule sont des entites distinctes, d'ou les
		// deux tableaux paralleles.
		array<RplId> rplIds = {};
		array<int> ids = {};

		foreach (WCS_Armament_SmokeDispenserComponent disp : m_aSmokeDispensers)
		{
			if (!disp)
				continue;
			IEntity owner = disp.GetOwner();
			if (!owner)
				continue;
			RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
			if (!rpl)
				continue;

			rplIds.Insert(rpl.Id());
			ids.Insert(disp.GetId());
		}

		if (ids.IsEmpty())
			return false;

		// compartmentUniqueName vide + requireCompartmentAuthorization=false : le tir ne
		// vient d'aucun poste d'equipage, c'est une reaction automatique du vehicule.
		FireDispensers_S(rplIds, ids, "", false);
		return true;
	}

	//! Meme chose pour les LEURRES THERMIQUES (helicopteres). Un Mi-8 vise par un missile
	//! guide largue ses leurres au lieu d'encaisser sans reagir.
	bool FFRX_FireFlaresAll_S()
	{
		if (!Replication.IsServer())
			return false;

		PruneFlareDispensers();

		if (!m_aFlareDispensers || m_aFlareDispensers.IsEmpty())
			return false;

		array<RplId> rplIds = {};
		array<int> ids = {};

		foreach (WCS_Armament_FlareDispenserComponent disp : m_aFlareDispensers)
		{
			if (!disp)
				continue;
			IEntity owner = disp.GetOwner();
			if (!owner)
				continue;
			RplComponent rpl = RplComponent.Cast(owner.FindComponent(RplComponent));
			if (!rpl)
				continue;

			rplIds.Insert(rpl.Id());
			ids.Insert(disp.GetId());
		}

		if (ids.IsEmpty())
			return false;

		FireDispensers_S(rplIds, ids, "", false);
		return true;
	}
}

//----------------------------------------------------------------------------------------
// Pont vers FFRX_VehicleSmokeScreen : le vrai pot est prefere, la fumee simulee sert de
// repli. Un blinde equipe WCS tire son lanceur ; tous les autres (vehicules de mods non
// modifies) restent couverts par la version simulee. Aucun vehicule ne se retrouve sans
// reaction, ce qui serait le cas si on ne gardait QUE le vrai lanceur.
//----------------------------------------------------------------------------------------
class FFRX_SmokeWCS
{
	//! true si le vehicule portait un lance-fumigenes WCS et l'a effectivement tire.
	static bool TryFireRealLauncher(IEntity vehicle)
	{
		if (!vehicle)
			return false;

		// Le manager est sur la racine du vehicule ; les pots sont ses enfants.
		WCS_Armament_DispenserManagerComponent mgr =
			WCS_Armament_DispenserManagerComponent.Cast(
				vehicle.FindComponent(WCS_Armament_DispenserManagerComponent));
		if (!mgr)
			return false;

		return mgr.FFRX_FireSmokeAll_S();
	}

	// Chance qu'un helicoptere REAGISSE a un verrouillage. Volontairement < 100 % : un
	// hélico qui largue ses leurres a TOUS les coups rendrait le Stinger inutile et le
	// duel previsible. A 65 %, le joueur ne sait jamais si son tir va passer -- et un
	// helico qui esquive une fois peut se faire avoir au suivant.
	static const int FLARE_CHANCE_PCT = 65;

	//! true si l'aeronef portait des leurres, a passe le tirage, et les a largues.
	static bool TryFireFlares(IEntity aircraft)
	{
		if (!aircraft)
			return false;

		WCS_Armament_DispenserManagerComponent mgr =
			WCS_Armament_DispenserManagerComponent.Cast(
				aircraft.FindComponent(WCS_Armament_DispenserManagerComponent));
		if (!mgr)
			return false;

		// Le tirage se fait APRES avoir verifie la presence du lanceur : sinon on
		// "consommerait" un echec sur un appareil qui n'aurait de toute facon rien tire.
		if (Math.RandomInt(0, 100) >= FLARE_CHANCE_PCT)
		{
			Print("[FFRX][Smoke] helico verrouille : pas de leurres cette fois (tirage).", LogLevel.NORMAL);
			return false;
		}

		return mgr.FFRX_FireFlaresAll_S();
	}
}
