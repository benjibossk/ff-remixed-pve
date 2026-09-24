// FF - REMIXED - PVE
// Production REELLE de supplies et de carburant, mesuree et non estimee.
//
// POURQUOI : on affichait un maximum theorique (m_fBaseSuppliesProduction x facteur
// de reglage). C'est faux dans la plupart des cas, car FF applique ensuite des
// modificateurs qu'on ne peut pas recalculer de l'exterieur :
//   - usines  : GetResourceProduction() est *protected*, et multiplie par le facteur
//               horaire jour/nuit, plus une efficacite ALEATOIRE de 0 a 50 % si le
//               site est tenu par l'ennemi -- lequel cesse meme totalement de produire
//               des que son stock depasse la moitie de la capacite ;
//   - stations: la recharge est PROBABILISTE (chance = 1 - remplissage) et la quantite
//               tiree au hasard entre 3 et 6 % de la capacite. Une "vitesse" theorique
//               n'existe donc pas, seulement une esperance.
//
// SOLUTION : on n'essaie plus de reproduire le calcul, on OBSERVE le resultat. Les deux
// methodes de production sont overridables ; on lit le stock avant/apres super() et on
// enregistre la difference. C'est exact par construction, et ca resiste a un changement
// de formule cote FF.
//
// Les totaux sont cumules par heure de jeu ecoulee, puis exposes a FFRX_FFStateSender.
// On ne garde AUCUNE reference vers une entite ou un composant (cf. la regle Enforce
// "Strong ref to '<Comp>' class is not allowed") : uniquement des entiers.

class FFRX_ProductionTracker
{
	// DEUX SERIES, et pas une seule.
	//
	// POURQUOI : dans FF une usine produit QUEL QUE SOIT son proprietaire -- une usine
	// ennemie continue de tourner (avec une efficacite aleatoire, et elle s'arrete quand
	// son stock depasse la moitie de sa capacite). N'afficher que ce qu'on produit revient
	// a masquer l'essentiel : combien la carte produit AU TOTAL, et donc ce qu'on peut
	// encore aller prendre. Un debit joueur de 40/h ne veut pas dire la meme chose selon
	// que la carte en produit 60 ou 600.
	//
	// "Player" = sites tenus par nous. "All" = tous les sites de la carte, nous compris.
	protected static int s_iLastSupplies;      // joueur, derniere heure complete
	protected static int s_iLastFuel;
	protected static int s_iLastSuppliesAll;   // carte entiere, derniere heure complete
	protected static int s_iLastFuelAll;

	// Heure en cours, encore en accumulation.
	protected static int s_iCurSupplies;
	protected static int s_iCurFuel;
	protected static int s_iCurSuppliesAll;
	protected static int s_iCurFuelAll;

	protected static int s_iCurHour = -1;

	//! Appele par les entites de production a chaque tick horaire, avec le delta reel.
	//! `mine` = le site est tenu par la faction joueur.
	static void RecordSupplies(int hour, int amount, bool mine)
	{
		RollIfNewHour(hour);
		if (amount <= 0) return;
		s_iCurSuppliesAll = s_iCurSuppliesAll + amount;
		if (mine) s_iCurSupplies = s_iCurSupplies + amount;
	}

	static void RecordFuel(int hour, int amount, bool mine)
	{
		RollIfNewHour(hour);
		if (amount <= 0) return;
		s_iCurFuelAll = s_iCurFuelAll + amount;
		if (mine) s_iCurFuel = s_iCurFuel + amount;
	}

	// Les entites sont notifiees une par une : on ne ferme le total que lorsque l'heure
	// change, sinon on publierait une somme partielle (seules les premieres usines
	// comptees).
	protected static void RollIfNewHour(int hour)
	{
		if (hour == s_iCurHour) return;

		if (s_iCurHour != -1)
		{
			s_iLastSupplies    = s_iCurSupplies;
			s_iLastFuel        = s_iCurFuel;
			s_iLastSuppliesAll = s_iCurSuppliesAll;
			s_iLastFuelAll     = s_iCurFuelAll;
		}
		s_iCurHour = hour;
		s_iCurSupplies = 0;
		s_iCurFuel = 0;
		s_iCurSuppliesAll = 0;
		s_iCurFuelAll = 0;
	}

	//! Production reelle de la derniere heure de jeu COMPLETE (0 avant la premiere).
	//! Sites TENUS par le joueur.
	static int GetSuppliesPerHour() { return s_iLastSupplies; }
	static int GetFuelPerHour()     { return s_iLastFuel; }

	//! Idem, mais sur TOUTE la carte (nos sites + ceux de l'ennemi). C'est le potentiel
	//! total du theatre, donc ce qui reste a conquerir.
	static int GetSuppliesPerHourAll() { return s_iLastSuppliesAll; }
	static int GetFuelPerHourAll()     { return s_iLastFuelAll; }

	//! Heure de jeu en cours d'accumulation. Sert aux stations-service, dont la methode
	//! de recharge ne recoit pas l'heure en parametre.
	static int GetCurrentHour() { return s_iCurHour; }
}

modded class JWK_FactoryEntity
{
	override void NotifyIngameHourPassed(int passedHour)
	{
		// m_Logistics est protected : lisible ici, pas depuis le sender.
		int before = 0;
		if (m_Logistics) before = m_Logistics.GetResources(JWK_ELogisticsResourceType.SUPPLIES);

		super.NotifyIngameHourPassed(passedHour);

		if (!m_Logistics) return;
		int after = m_Logistics.GetResources(JWK_ELogisticsResourceType.SUPPLIES);

		// On enregistre TOUJOURS, en marquant si le site est a nous : le tracker tient
		// deux series (tenu / carte entiere), car une usine ennemie produit elle aussi.
		bool mine = (m_FactionControl != null && m_FactionControl.IsPlayerFaction());
		FFRX_ProductionTracker.RecordSupplies(passedHour, after - before, mine);
	}
}

modded class JWK_FuelStationEntity
{
	override void UpdateFuelLevel_S()
	{
		int before = 0;
		if (m_Logistics) before = m_Logistics.GetResources(JWK_ELogisticsResourceType.FUEL);

		super.UpdateFuelLevel_S();

		if (!m_Logistics) return;
		int after = m_Logistics.GetResources(JWK_ELogisticsResourceType.FUEL);

		// Contrairement a JWK_FactoryEntity, une station-service n'a PAS de
		// m_FactionControl : JWK_FuelStationEntity ne declare que m_Logistics. On passe
		// donc par le manager de factions, comme le fait le sender.
		//
		// UpdateFuelLevel_S ne recoit pas l'heure : on reutilise celle du tracker, qui
		// est avancee par les usines. Les deux sont appeles sur le meme tick horaire.
		JWK_FactionManager fm = JWK.GetFactions();
		bool mine = (fm != null && fm.GetEntityRole(this) == JWK_EFactionRole.PLAYER);
		FFRX_ProductionTracker.RecordFuel(FFRX_ProductionTracker.GetCurrentHour(), after - before, mine);
	}
}
