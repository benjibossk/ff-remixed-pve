// FF - REMIXED - PVE
// BALISE GPS -- poser une position sur la carte de l'escouade, tant qu'il y a du jus.
//
// ======================================================================================
//  L'INTENTION
// ======================================================================================
// Toute la conception du mod tient sur un principe : l'information se GAGNE. Carte vide,
// depeches a recuperer sur un site radio tenu, papiers sur le corps d'un officier, convois
// reveles en coordonnees imprecises. La balise est la brique ACTIVE de cette famille : au
// lieu d'attendre un renseignement, on va le poser soi-meme.
//
// Et surtout, elle rend l'intel PERISSABLE -- la batterie qui descend met un compte a
// rebours sur ce qu'on a marque. Meme logique que les depeches radio qui se perimentt.
//
// ======================================================================================
//  LA BATTERIE : LA MEME CELLULE QUE LES DRONES ET LES JAMMERS
// ======================================================================================
// La charge vit dans la BALISE, pas dans la cellule. C'est deja le schema du mod drones :
// `SAL_BatteryComponent` ne porte qu'une CAPACITE (`m_fBatteryStorage`, 1800 par defaut),
// pas une charge courante -- une cellule est un jeton "plein" qu'on consomme entier. Une
// cellule a moitie vide n'existe pas.
//
// Consequence voulue : une seule ressource logistique pour drone + jammer + balise. Et une
// balise rechargeable est du materiel qu'on RAMENE, donc un enjeu de plus a l'extraction.
//
// Deux gestes pour recharger, comme le jammer :
//   - glisser une cellule sur la balise (branche dans FFRX_JammerBatteryDrop.c, qui porte
//     l'unique `modded SCR_InventoryStorageManagerComponent.OnItemAdded`) ;
//   - l'action d'inventaire (FFRX_BeaconRecharge, plus bas).
//
// ======================================================================================
//  POURQUOI UN SYSTEME CENTRAL ET PAS UN TIC PAR BALISE
// ======================================================================================
// Un `ScriptComponent` pose sur un objet RANGE DANS UN SAC ne recoit pas d'evenement de
// trame : compter dessus donnerait une balise qui ne se decharge pas une fois dans
// l'inventaire -- exactement le cas d'usage principal. On tient donc un registre des balises
// ALLUMEES et un seul timer serveur les parcourt. Meme raison que le timer unique de
// FFRX_JammerSituational ("one timer to rule them all").
//
// ======================================================================================
//  RESEAU
// ======================================================================================
// L'etat (allume / batterie) est AUTORITAIRE SERVEUR. L'action d'inventaire, elle, s'execute
// cote CLIENT : basculer l'etat directement dedans ne ferait rien sur le dedie -- c'est le
// piege deja paye avec l'action "Prendre dotation" (cf. memoire
// `dedie-useraction-canperform-client-gate`). On passe donc par un RPC vers le serveur,
// comme le fait le mod drones avec `SCR_PlayerController.ToggleJammer`.
//
// Le marqueur est un marqueur DYNAMIQUE accroche a l'entite balise : il suit l'objet tout
// seul (utile si la balise est posee sur un vehicule), et il s'enleve d'un appel.
//
// Serveur pour l'etat, diffusion a tous les clients pour le marqueur. Chaines ASCII.

//------------------------------------------------------------------------------------------------
class FFRX_BeaconComponentClass : ScriptComponentClass
{
}

//! Pose ce composant sur le prefab de la balise (derive de FPVBattery.et).
class FFRX_BeaconComponent : ScriptComponent
{
	//! Autonomie a pleine charge, en secondes. 2 h, comme demande.
	static const float BATTERY_MAX = 7200.0;

	protected float m_fBattery = -1.0;   // -1 = pas encore initialise
	protected bool  m_bOn;

	//! Nombre de tics ecoules depuis la derniere position transmise. C'est CE compteur qui
	//! fait "saccader" le signal : plus on est loin d'un site radio ami, plus il faut de
	//! tics avant qu'une nouvelle position parte (cf. FFRX_Beacons.Tick).
	protected int m_iTicksSinceSend;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_fBattery < 0)
			m_fBattery = BATTERY_MAX;   // une balise neuve sort chargee
	}

	//------------------------------------------------------------------------------------------------
	bool FFRX_IsOn()
	{
		return m_bOn;
	}

	//------------------------------------------------------------------------------------------------
	int FFRX_BatteryPct()
	{
		if (m_fBattery < 0)
			return 100;

		return Math.ClampInt((int)(m_fBattery / BATTERY_MAX * 100.0), 0, 100);
	}

	//------------------------------------------------------------------------------------------------
	bool FFRX_IsFlat()
	{
		return m_fBattery <= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Consomme du temps de fonctionnement. \return true si la balise vient de tomber a plat.
	bool FFRX_Drain(float seconds)
	{
		if (m_fBattery < 0)
			m_fBattery = BATTERY_MAX;

		m_fBattery = m_fBattery - seconds;
		if (m_fBattery > 0)
			return false;

		m_fBattery = 0;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Recharge depuis une cellule. La capacite vient de la cellule (1800 s par defaut), donc
	//! quatre cellules pour faire le plein -- c'est voulu : la balise coute cher a tenir allumee.
	void FFRX_Recharge(float seconds)
	{
		if (m_fBattery < 0)
			m_fBattery = 0;

		m_fBattery = Math.Min(m_fBattery + seconds, BATTERY_MAX);
	}

	//------------------------------------------------------------------------------------------------
	void FFRX_SetOn(bool on)
	{
		m_bOn = on;
		m_iTicksSinceSend = 9999;   // forcer un envoi immediat a l'allumage
	}

	//------------------------------------------------------------------------------------------------
	//! \return true si une position doit partir maintenant, et remet le compteur a zero.
	bool FFRX_DueToSend(int everyNTicks)
	{
		m_iTicksSinceSend = m_iTicksSinceSend + 1;

		if (m_iTicksSinceSend < everyNTicks)
			return false;

		m_iTicksSinceSend = 0;
		return true;
	}
}

// ---------------------------------------------------------------------------
//  QUALITE DU SIGNAL
// ---------------------------------------------------------------------------
// Demande de Benji (21/09) : "le signal saccade plus il s'eloigne de la portee radio de la
// base". C'est la premiere brique concrete du chantier radio/signal.
//
// On reutilise `FFRX_RadioIntel.NearestFriendlySiteDistance()`, qui rend la distance au site
// radio OPERABLE le plus proche que l'on controle (-1 s'il n'y en a aucun) -- exactement la
// notion de "portee radio de la base", et deja eprouvee par les depeches radio.
//
// L'effet n'est PAS cosmetique : loin d'un relais, la position transmise est a la fois plus
// RARE et plus FAUSSE. Le marqueur devient une derniere position connue, pas un suivi. Et
// capturer une tour radio ameliore donc aussi la balise -- ce qui relie enfin la couverture
// radio a autre chose qu'aux rapports de Reoccupation.
class FFRX_BeaconSignal
{
	// ------------------------------------------------------------------------------------
	//  LE PALIER "FRANC" N'EST PLUS UNE DISTANCE : C'EST LA COUVERTURE DECLAREE PAR FF
	// ------------------------------------------------------------------------------------
	// Historique de ce reglage, parce qu'il s'est trompe deux fois :
	//   1. 800 / 2000 / 4000 m -- chiffres inventes, sans rapport avec le jeu.
	//   2. 1300 / 3000 / 5000 m -- cales sur les portees d'EMETTEURS relevees dans les
	//      prefabs (poste de poitrine 1300, poste de sac 2000, relais 3000, tours 4000-5000).
	//      Mieux, mais toujours a cote : FF ne raisonne PAS en portee d'emetteur.
	//   3. Aujourd'hui : on demande directement a FF.
	//
	// FF expose `JWK.GetRadioTowers().IsPointInRange(pos, role)`, qui verifie pour CHAQUE site
	// radio qu'il est operable, controle par la bonne faction, et que le point tombe dans le
	// `m_iSignalRange` DECLARE de ce site-la (2500 m par defaut). C'est la meme fonction que
	// FF utilise pour decider quels appels radio un joueur peut passer, et la meme portee que
	// celle affichee sur sa carte. En la reutilisant, notre signal et l'interface de FF
	// disent enfin la meme chose -- avant, un joueur pouvait voir "dans la zone" sur sa carte
	// et decrocher quand meme.
	//
	// Les deux paliers suivants restent des distances, parce qu'ils decrivent la DEGRADATION
	// au-dela de la couverture nominale : quelque chose passe encore, de moins en moins bien.
	// Multiples ronds de la portee de reference de FF (2500).
	static const float MOYEN_M   = 5000.0;   // 2x la portee nominale d'un site
	static const float FAIBLE_M  = 7500.0;   // 3x

	//------------------------------------------------------------------------------------------------
	//! 0 = fort, 1 = moyen, 2 = faible, 3 = hors de portee.
	//!
	//! `rangeMult` etire les paliers : c'est par la que passe le sac radio (cf.
	//! FFRX_RadioRelay). Le facteur n'est pas arbitraire -- c'est le rapport REEL entre un
	//! poste de sac et un poste de poitrine dans le jeu : 2000 / 1300 = 1,54.
	//! On multiplie les SEUILS plutot que de diviser la distance : c'est la meme chose en
	//! calcul, mais ca se lit comme ce que c'est -- un poste qui porte plus loin.
	static int Level(vector pos, float rangeMult = 1.0)
	{
		// --- Palier FRANC : on delegue a FF -----------------------------------------------
		// `IsPointInRange` verifie l'operabilite du site, son controle par la resistance, et
		// la portee propre a CE site. Rien a recalculer, et on ne peut pas diverger de la
		// carte affichee au joueur.
		JWK_RadioTowerManagerComponent towers = JWK.GetRadioTowers();
		if (towers && towers.IsPointInRange(pos, JWK_EFactionRole.PLAYER))
			return 0;

		// Une TOUR EMETTRICE construite par les joueurs couvre aussi son rayon. FF ne les
		// connait pas (ce ne sont pas des JWK_RadioSiteEntity), d'ou notre propre registre.
		if (FFRX_RadioTowers.InTowerCoverage(pos))
			return 0;

		float d = FFRX_RadioIntel.NearestFriendlySiteDistance(pos);

		// Aucun site radio tenu : on ne recoit rien du tout. Et aucun sac n'y change quoi que
		// ce soit -- un poste puissant sans relais en face ne parle a personne.
		if (d < 0)
			return 3;

		if (rangeMult < 1)
			rangeMult = 1;

		if (d <= MOYEN_M * rangeMult)
			return 1;
		if (d <= FAIBLE_M * rangeMult)
			return 2;

		return 3;
	}

	//------------------------------------------------------------------------------------------------
	static int TicksBetweenSends(int level)
	{
		if (level == 0)
			return 1;    // 5 s
		if (level == 1)
			return 3;    // 15 s
		return 6;        // 30 s
	}

	//------------------------------------------------------------------------------------------------
	static float ErrorMeters(int level)
	{
		if (level == 0)
			return 0;
		if (level == 1)
			return 40;
		return 120;
	}

	//------------------------------------------------------------------------------------------------
	static string Label(int level)
	{
		if (level == 0)
			return "";                      // signal franc : rien a signaler
		if (level == 1)
			return " - signal moyen";
		return " - signal faible";
	}
}

// ---------------------------------------------------------------------------
//  LE SYSTEME : registre des balises allumees + tic serveur + marqueur
// ---------------------------------------------------------------------------
class FFRX_Beacons
{
	protected static ref FFRX_Beacons s_Instance;

	//! Periode du tic, en ms. 5 s suffit pour une autonomie qui se compte en heures, et
	//! c'est 12 fois moins de reveils qu'un tic a la seconde.
	protected static const int TICK_MS = 5000;

	//! Balises actuellement ALLUMEES. Une balise eteinte n'est pas ici : elle ne consomme
	//! rien et ne coute aucun parcours.
	protected ref array<IEntity> m_aLit = {};

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_Instance)
			return;

		s_Instance = new FFRX_Beacons();
		GetGame().GetCallqueue().CallLater(s_Instance.Tick, TICK_MS, true);
		Print("[FFRX][Balise] Systeme demarre.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static FFRX_BeaconComponent Find(IEntity ent)
	{
		if (!ent)
			return null;

		return FFRX_BeaconComponent.Cast(ent.FindComponent(FFRX_BeaconComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Bascule une balise. SERVEUR uniquement -- appele depuis le RPC, jamais directement
	//! depuis l'action d'inventaire (qui, elle, tourne chez le client).
	//! \return un message a afficher au joueur, ou "" si rien a dire.
	static string Toggle(IEntity beacon)
	{
		if (!Replication.IsServer())
			return "";

		FFRX_BeaconComponent c = Find(beacon);
		if (!c)
			return "";

		if (!s_Instance)
			Boot();

		if (c.FFRX_IsOn())
		{
			SetLit(beacon, c, false);
			return "Balise eteinte (" + c.FFRX_BatteryPct().ToString() + " pct).";
		}

		if (c.FFRX_IsFlat())
			return "Balise a plat : recharge-la avec une batterie de drone.";

		SetLit(beacon, c, true);
		return "Balise allumee (" + c.FFRX_BatteryPct().ToString() + " pct). Position transmise a l'escouade.";
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetLit(IEntity beacon, FFRX_BeaconComponent c, bool on)
	{
		c.FFRX_SetOn(on);

		if (!s_Instance)
			return;

		int idx = s_Instance.m_aLit.Find(beacon);

		if (on)
		{
			if (idx == -1)
				s_Instance.m_aLit.Insert(beacon);
		}
		else if (idx != -1)
		{
			s_Instance.m_aLit.Remove(idx);
		}

		FFRX_BeaconMarker.Broadcast(beacon, on);
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		float step = TICK_MS / 1000.0;

		for (int i = m_aLit.Count() - 1; i >= 0; i--)
		{
			IEntity b = m_aLit[i];

			// Balise detruite ou ramassee par le ramasse-miettes : on nettoie sans bruit.
			if (!b || b.IsDeleted())
			{
				m_aLit.Remove(i);
				continue;
			}

			FFRX_BeaconComponent c = Find(b);
			if (!c)
			{
				m_aLit.Remove(i);
				continue;
			}

			if (c.FFRX_Drain(step))
			{
				// A plat : extinction automatique et retrait du marqueur.
				c.FFRX_SetOn(false);
				m_aLit.Remove(i);
				FFRX_BeaconMarker.Broadcast(b, false);
				Print("[FFRX][Balise] Batterie epuisee : extinction.", LogLevel.NORMAL);
				continue;
			}

			// --- Transmission de la position, cadencee par la qualite du signal ---------
			//
			// Une balise PORTEE profite du sac radio de son porteur : on juge la liaison
			// depuis l'entite RACINE (le soldat, ou le vehicule), pas depuis la balise. Une
			// balise posee au sol, elle, n'a pas de porteur -- `MultiplierFor` rend 1 et on
			// retombe sur le comportement normal.
			IEntity carrier = RootOf(b);
			vector pos = carrier.GetOrigin();
			int level = FFRX_BeaconSignal.Level(pos, FFRX_RadioRelay.MultiplierFor(carrier));

			// Hors de portee : on n'envoie RIEN. Le marqueur reste ou il etait -- une
			// derniere position connue, ce qui est plus honnete (et plus evocateur) que de
			// le faire disparaitre ou de le laisser suivre par magie.
			if (level >= 3)
				continue;

			if (!c.FFRX_DueToSend(FFRX_BeaconSignal.TicksBetweenSends(level)))
				continue;

			FFRX_BeaconMarker.BroadcastPos(b, Scatter(pos, FFRX_BeaconSignal.ErrorMeters(level)),
				c.FFRX_BatteryPct(), level);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Position REELLE dans le monde d'une balise.
	//!
	//! ⚠️ Indispensable : une balise RANGEE dans un sac est une entite ENFANT du porteur, et
	//! son `GetOrigin()` est alors une position LOCALE (proche de zero), pas une position
	//! monde. Sans ce remontage de hierarchie, un joueur qui garde sa balise sur lui
	//! apparaitrait a l'origine de la carte -- exactement le faux positif qu'on a deja vu
	//! avec la sonde des specialistes (`<0,0,0>` lus avant placement).
	//!
	//! C'est aussi ce qui fait marcher la demande "un soldat qui a son GPS, on le voit" : on
	//! remonte jusqu'au porteur, donc le marqueur suit le SOLDAT.
	protected static vector WorldPosOf(IEntity ent)
	{
		return RootOf(ent).GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! L'entite racine : la balise elle-meme si elle est posee au sol, sinon son PORTEUR
	//! (soldat, vehicule). Sert a la fois a la position monde et a trouver un sac radio.
	protected static IEntity RootOf(IEntity ent)
	{
		IEntity root = ent;
		while (root.GetParent())
		{
			root = root.GetParent();
		}

		return root;
	}

	//------------------------------------------------------------------------------------------------
	//! Decale une position d'au plus `meters`, dans une direction quelconque.
	protected static vector Scatter(vector pos, float meters)
	{
		if (meters <= 0)
			return pos;

		float ang = Math.RandomFloat(0, Math.PI2);
		float d   = Math.RandomFloat(0, meters);

		vector p = pos;
		p[0] = p[0] + Math.Cos(ang) * d;
		p[2] = p[2] + Math.Sin(ang) * d;
		return p;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de balises allumees, pour la commande #balise.
	static int LitCount()
	{
		if (!s_Instance)
			return 0;

		return s_Instance.m_aLit.Count();
	}
}
