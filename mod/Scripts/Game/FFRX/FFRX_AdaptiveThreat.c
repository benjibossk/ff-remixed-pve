// FF - REMIXED - PVE
// MENACE ADAPTATIVE : l'ennemi arme ce qui le fait souffrir, LA OU ca le fait souffrir.
//
// Si les joueurs ecrasent une zone au blinde, les equipes antichar y deviennent plus
// frequentes. S'ils la survolent en helicoptere, ce sont les equipes anti-aeriennes.
// C'est une INFLEXION, pas un interrupteur : on ne fait que biaiser un tirage qui reste
// aleatoire, et seulement dans la limite du reglage "Menace adaptative - intensite".
//
// ------------------------------------------------------------------------------------
// POURQUOI CE SYSTEME N'EXISTE PAS DEJA DANS FF
//
// FF a bien une notion de menace (JWK_WorldThreatManagerComponent, le threat Hearts &
// Minds par ville), mais elle ne pilote que la TAILLE des forces : dans
// CalculateRequestManpower, `threat` devient un sizeFactor -- plus de monde, jamais
// d'autres profils. La preuve la plus claire est dans le generateur lui-meme :
//
//     ResourceName GetPatrolGroup(float threat) {
//         return m_FactionTrait.GetMergedForce().m_aPatrolGroups.GetRandomElement();
//     }
//
// le parametre `threat` est recu et JAMAIS utilise. Rien, dans FF ni dans Reoccupation,
// ne regarde COMMENT le joueur tue.
//
// ------------------------------------------------------------------------------------
// OU ON S'ACCROCHE, ET POURQUOI PAS AILLEURS
//
// On RENFORCE un groupe existant, on ne le remplace pas : a chaque groupe ennemi qui
// apparait dans une zone chaude, un servant Javelin ou sol-air peut s'ajouter. Le hook
// est SCR_AIGroup.EOnInit (porte par FFRX_AIAssault.c, le seul modded SCR_AIGroup
// autorise pour cet addon).
//
// Pourquoi pas remplacer le groupe tire, ce qui serait plus direct : TOUS les points ou
// m_rGroupPrefab est assigne appartiennent a des classes citees dans un [Friend] de FF
// (JWK_AIForce, JWK_AIForceSystem, JWK_TownMilitaryActivityComponent). Les modder fait
// perdre l'amitie et casse la compilation a froid -- 48 erreurs, invisibles en
// hot-reload. Verifie, pas suppose (cf. l'encadre en bas de FFRX_SpawnCensus.c).
//
// Le renfort se revele d'ailleurs meilleur : la patrouille garde ses fusiliers (on ne
// refait pas le bug des groupes vides), l'effet s'applique a une campagne DEJA EN COURS
// -- contrairement au generateur de composition, qui ne tourne qu'a la creation -- et il
// couvre tous les producteurs, DARC compris.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict, cf. memoire).

// ---------------------------------------------------------------------------
//! Les deux jauges d'une zone, dans [0..1]. 1 = saturation (effet maximal).
class FFRX_ThreatZone
{
	float m_fArmor;   // pression blindee : on nous tue depuis des vehicules
	float m_fAir;     // pression aerienne : on nous survole

	// Un point situe DANS la zone. Sert a la retrouver pour l'affichage sur la carte
	// admin : la cle n'est qu'un texte, elle ne permet pas de redemander son polygone
	// au systeme de territoire.
	vector m_vAnchor;
}

// ---------------------------------------------------------------------------
class FFRX_AdaptiveThreat
{
	protected static ref FFRX_AdaptiveThreat s_Instance;

	// Cadence d'echantillonnage. 15 s est assez fin pour du temps de vol et assez large
	// pour etre gratuit en CPU (on ne parcourt que les joueurs connectes).
	protected static const int TICK_MS = 15000;

	// Temps de vol cumule, en secondes, au bout duquel la jauge aerienne d'une zone est
	// pleine. 5 min de survol effectif d'un meme secteur = saturation.
	protected static const float AIR_SATURATION_SEC = 300;

	// Ce qu'un kill depuis un vehicule ajoute a la jauge blindee. ~8 kills = saturation.
	protected static const float ARMOR_PER_KILL = 0.12;

	// Taille d'une cellule de repli quand la position n'est sur aucun territoire FF
	// (desert, hors zone). Assez large pour qu'un helicoptere en transit ne seme pas une
	// trainee de micro-zones derriere lui.
	protected static const float FALLBACK_CELL_M = 1000;

	// Cle de zone -> jauges.
	protected ref map<string, ref FFRX_ThreatZone> m_mZones = new map<string, ref FFRX_ThreatZone>();

	// Les specialistes qu'on adjoint au groupe. On ajoute UN homme, pas une equipe :
	// l'effet doit se sentir sans doubler les effectifs ennemis.
	protected static const ResourceName CHAR_AT = "{6FFEC0DEDA000206}Prefabs/Characters/Factions/IND/MEI/Character_MEI_Javelin.et";
	protected static const ResourceName CHAR_AA = "{6FFEC0DEDA000205}Prefabs/Characters/Factions/IND/MEI/Character_MEI_Air.et";

	// A EOnInit le groupe existe mais il est vide : FF spawne ses membres juste apres.
	// On laisse passer la composition d'origine avant d'ajouter la notre.
	protected static const int REINFORCE_DELAY_MS = 4000;

	// Diagnostic : combien de renforts depuis le demarrage.
	protected int m_iSubsAT;
	protected int m_iSubsAA;

	//------------------------------------------------------------------------------------------------
	static FFRX_AdaptiveThreat Get()
	{
		if (!s_Instance)
			s_Instance = new FFRX_AdaptiveThreat();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;

		Get().Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void Start()
	{
		SCR_BaseGameMode gm = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gm)
		{
			// Meme invoker que le killfeed de FFRX_FFStateSender. Un ScriptInvoker accepte
			// plusieurs abonnes : on s'y branche pour notre compte plutot que de se coupler
			// au sender, qui a un tout autre cycle de vie.
			gm.GetOnControllableDestroyed().Insert(OnControllableDestroyed);
		}

		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		Print("[FFRX][Threat] Menace adaptative active.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Cle de la zone qui contient `pos`.
	//!
	//! On prefere le noeud de territoire FF : c'est l'unite que le joueur percoit
	//! (une ville, un secteur), et sa geometrie ne bouge pas. Le centroide arrondi en
	//! fait une cle stable entre deux sessions. Hors territoire, on retombe sur une
	//! grille reguliere pour ne pas perdre l'information.
	protected string ZoneKey(vector pos)
	{
		JWK_TerritoryControlSystem tcs = JWK.GetTerritoryControl();
		if (tcs)
		{
			JWK_TerritoryControlNodeComponent node = tcs.GetNodeAt(pos);
			if (node)
			{
				vector c = node.GetCentroid();
				return "t:" + Math.Round(c[0]).ToString() + ":" + Math.Round(c[2]).ToString();
			}
		}

		int gx = Math.Floor(pos[0] / FALLBACK_CELL_M);
		int gz = Math.Floor(pos[2] / FALLBACK_CELL_M);
		return "g:" + gx.ToString() + ":" + gz.ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected FFRX_ThreatZone ZoneAt(vector pos, bool createIfMissing)
	{
		string key = ZoneKey(pos);

		FFRX_ThreatZone z;
		if (m_mZones.Find(key, z) && z)
			return z;

		if (!createIfMissing)
			return null;

		z = new FFRX_ThreatZone();
		z.m_vAnchor = pos;
		m_mZones.Set(key, z);
		return z;
	}

	//------------------------------------------------------------------------------------------------
	//! Demi-vie des jauges, en secondes, depuis le reglage in-game.
	protected float HalfLifeSec()
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return 20 * 60;

		float minutes = cache.m_fFFRX_AdaptHalfLife;
		if (minutes < 1)
			minutes = 20;
		return minutes * 60;
	}

	//------------------------------------------------------------------------------------------------
	//! Part maximale des groupes specialistes qu'on s'autorise a substituer, dans [0..1].
	//! 0 = systeme desactive.
	protected float MaxShare()
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return 0.35;

		return Math.Clamp(cache.m_fFFRX_AdaptStrength / 100, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Niveau PLANCHER des jauges, dans [0..1]. 0 = comportement purement reactif d'origine.
	//! Reglage in-game "Menace adaptative - socle".
	protected float FloorLevel()
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return 0.15;

		return Math.Clamp(cache.m_fFFRX_AdaptFloor / 100, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Echantillonnage du temps de vol + decroissance des jauges.
	protected void Tick()
	{
		Decay();

		if (MaxShare() <= 0)
			return; // systeme coupe : on ne mesure meme pas

		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return;

		array<int> ids = {};
		pm.GetPlayers(ids);

		float gain = (TICK_MS / 1000.0) / AIR_SATURATION_SEC;

		foreach (int pid : ids)
		{
			IEntity ch = pm.GetPlayerControlledEntity(pid);
			if (!ch)
				continue;

			IEntity vehicle = VehicleOf(ch);
			if (!vehicle)
				continue;

			if (VehicleType(vehicle) != JWK_EVehicleType.HELICOPTER)
				continue;

			// Le temps de vol compte pour la zone SURVOLEE a cet instant : un vol qui
			// traverse trois secteurs les arme tous les trois, chacun a hauteur du temps
			// passe au-dessus. C'est exactement l'effet voulu -- l'AA apparait sur les
			// couloirs reellement empruntes, pas sur la carte entiere.
			FFRX_ThreatZone z = ZoneAt(vehicle.GetOrigin(), true);
			if (z)
				z.m_fAir = Math.Clamp(z.m_fAir + gain, 0, 1);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Decroissance exponentielle : sans nouvelle pression, une zone retombe de moitie a
	//! chaque demi-vie. Une zone retombee a zero est OUBLIEE (la map ne grossit pas
	//! indefiniment sur une longue campagne).
	protected void Decay()
	{
		float halfLife = HalfLifeSec();
		if (halfLife <= 0)
			return;

		float factor = Math.Pow(0.5, (TICK_MS / 1000.0) / halfLife);

		array<string> dead = {};
		foreach (string key, FFRX_ThreatZone z : m_mZones)
		{
			if (!z)
				continue;

			z.m_fArmor = z.m_fArmor * factor;
			z.m_fAir   = z.m_fAir * factor;

			if (z.m_fArmor < 0.01 && z.m_fAir < 0.01)
				dead.Insert(key);
		}

		foreach (string key : dead)
			m_mZones.Remove(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Un ennemi vient de mourir : si son tueur etait embarque dans un vehicule serieux,
	//! la zone retient la lecon.
	protected void OnControllableDestroyed(SCR_InstigatorContextData ctx)
	{
		if (!ctx)
			return;

		if (MaxShare() <= 0)
			return;

		IEntity victim = ctx.GetVictimEntity();
		if (!victim)
			return;

		// Seules les pertes ENNEMIES nous interessent : c'est l'ennemi qui s'adapte.
		if (!JWK.GetFactions())
			return;
		if (JWK.GetFactions().GetEntityRole(victim) != JWK_EFactionRole.ENEMY)
			return;

		Instigator inst = ctx.GetInstigator();
		if (!inst)
			return;

		IEntity killer = inst.GetInstigatorEntity();
		if (!killer)
			return;

		IEntity vehicle = VehicleOf(killer);
		if (!vehicle)
			return;

		// Un pick-up civil ne doit pas declencher une reponse antichar. On exige un
		// vehicule blinde OU arme au-dela de la simple mitrailleuse anti-personnel.
		JWK_VehicleAttributes attrs = AttributesOf(vehicle);
		if (!attrs)
			return;

		bool armored = attrs.m_iArmorType == JWK_EVehicleArmorType.LIGHT_ARMOR
			|| attrs.m_iArmorType == JWK_EVehicleArmorType.HEAVY_ARMOR;
		bool gunned = attrs.m_iArmamentType == JWK_EVehicleArmamentType.MEDIUM_ARMED
			|| attrs.m_iArmamentType == JWK_EVehicleArmamentType.HEAVY_ARMED;

		if (!armored && !gunned)
			return;

		// Un helicoptere de combat nourrit la jauge AERIENNE, pas la blindee : la reponse
		// correcte a un gunship est un missile sol-air, pas un Javelin.
		FFRX_ThreatZone z = ZoneAt(victim.GetOrigin(), true);
		if (!z)
			return;

		if (attrs.m_iVehicleType == JWK_EVehicleType.HELICOPTER)
			z.m_fAir = Math.Clamp(z.m_fAir + ARMOR_PER_KILL, 0, 1);
		else
			z.m_fArmor = Math.Clamp(z.m_fArmor + ARMOR_PER_KILL, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Vehicule qu'occupe ce personnage, ou null. Meme chemin que le recensement :
	//! compartiment -> proprietaire du slot -> RACINE de la hierarchie (le proprietaire
	//! direct n'est qu'une tourelle ou une banquette).
	protected IEntity VehicleOf(IEntity character)
	{
		if (!character)
			return null;

		CompartmentAccessComponent cac = JWK_CompTU<CompartmentAccessComponent>.FindIn(character);
		if (!cac)
			return null;

		BaseCompartmentSlot slot = cac.GetCompartment();
		if (!slot)
			return null;

		IEntity owner = slot.GetOwner();
		if (!owner)
			return null;

		return Vehicle.Cast(SCR_EntityHelper.GetMainParent(owner, true));
	}

	//------------------------------------------------------------------------------------------------
	protected JWK_VehicleAttributes AttributesOf(IEntity vehicle)
	{
		if (!vehicle)
			return null;

		JWK_VehicleAttributesComponent comp = JWK_CompTU<JWK_VehicleAttributesComponent>.FindIn(vehicle);
		if (!comp)
			return null;

		return comp.GetAttributes();
	}

	//------------------------------------------------------------------------------------------------
	protected JWK_EVehicleType VehicleType(IEntity vehicle)
	{
		JWK_VehicleAttributes attrs = AttributesOf(vehicle);
		if (!attrs)
			return JWK_EVehicleType.UNDEFINED;

		return attrs.m_iVehicleType;
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe ennemi vient d'apparaitre : la zone chaude lui adjoint peut-etre un
	//! servant antichar ou sol-air. Appele depuis le `modded class SCR_AIGroup.EOnInit`
	//! de FFRX_AIAssault.c (le seul autorise pour cet addon).
	//!
	//! POURQUOI ON RENFORCE AU LIEU DE SUBSTITUER. Le plan initial etait de remplacer le
	//! groupe tire dans JWK_AIForce.DoSpawn. Impossible : modder JWK_AIForce fait perdre
	//! les [Friend(JWK_AIForce)] de FF et casse la compilation a froid (cf. l'encadre en
	//! bas de FFRX_SpawnCensus.c). Et TOUS les autres points d'assignation de
	//! m_rGroupPrefab appartiennent a des classes elles aussi citees dans un [Friend]
	//! (JWK_AIForceSystem, JWK_TownMilitaryActivityComponent) -- verifie, pas suppose.
	//!
	//! Renforcer via SCR_AIGroup.EOnInit n'a aucun de ces inconvenients, et se trouve
	//! etre un meilleur design : la patrouille reste une patrouille avec ses fusiliers
	//! (on ne refait pas le bug des groupes vides), l'effet marche sur une campagne DEJA
	//! EN COURS, et il couvre tous les producteurs -- FF, Reoccupation et DARC compris.
	void MaybeReinforce(SCR_AIGroup group, IEntity owner)
	{
		if (!group || !owner)
			return;

		float maxShare = MaxShare();
		if (maxShare <= 0)
			return;

		// On ne renforce que l'ennemi.
		if (!JWK.GetFactions())
			return;
		if (JWK.GetFactions().GetEntityRole(owner) != JWK_EFactionRole.ENEMY)
			return;

		FFRX_ThreatZone z = ZoneAt(owner.GetOrigin(), false);
		if (!z)
			return;

		// La jauge la plus haute decide de la REPONSE, son niveau decide de la
		// PROBABILITE. Une zone a moitie chaude avec une intensite reglee a 35 % ne
		// renforce donc que 17 % des groupes qui y apparaissent.
		float level = z.m_fArmor;
		bool wantAA = false;
		if (z.m_fAir > z.m_fArmor)
		{
			level = z.m_fAir;
			wantAA = true;
		}

		// SOCLE : une part de specialistes meme en zone froide.
		//
		// Sans lui le systeme est purement reactif, et le recensement l'a montre : 200 min
		// de jeu, 343 soldats ennemis, et AUCUN sol-air (Character_MEI_Air) -- parce que
		// personne n'avait encore vole ni detruit de blinde. Consequence de design : le
		// premier helicoptere du serveur ne rencontre jamais de defense, et l'ennemi n'a
		// d'antichar qu'apres avoir deja perdu un blinde.
		//
		// Le socle plancherise le NIVEAU, pas la probabilite finale : il reste multiplie par
		// l'intensite, donc couper l'intensite a 0 coupe toujours tout le systeme.
		float floorLevel = FloorLevel();
		if (level < floorLevel)
		{
			level = floorLevel;

			// Zone totalement froide : les deux jauges sont a zero, donc le test
			// "m_fAir > m_fArmor" ci-dessus a tranche pour l'antichar par defaut. Sur le
			// socle il n'y a rien a deduire, on tire a pile ou face -- sinon le socle ne
			// produirait QUE de l'antichar et l'anti-aerien resterait introuvable, ce qui
			// est exactement le trou qu'on vient boucher.
			if (z.m_fAir <= 0 && z.m_fArmor <= 0)
				wantAA = (JWK.Random.RandFloat01() < 0.5);
		}

		if (level <= 0)
			return;

		if (JWK.Random.RandFloat01() > level * maxShare)
			return;

		// Differe : a EOnInit le groupe est cree mais ses membres ne sont pas encore la.
		// On laisse FF poser sa composition, puis on ajoute la notre par-dessus.
		GetGame().GetCallqueue().CallLater(DoReinforce, REINFORCE_DELAY_MS, false, group, wantAA);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawne le specialiste et l'agrege au groupe. Meme forme que JWK_SpawnUtils :
	//! SpawnEntityPrefab puis AddAIEntityToGroup.
	protected void DoReinforce(SCR_AIGroup group, bool wantAA)
	{
		// Le groupe a pu etre detruit ou despawne pendant le delai.
		if (!group)
			return;

		// ⚠️ PAS de `group.GetOwner()` : `SCR_AIGroup` EST une entite (AIGroup -> AIAgent ->
		// GenericEntity), pas un composant -- `GetOwner()` n'existe pas dessus et le
		// compilateur casse tout le module ("Undefined function 'SCR_AIGroup.GetOwner'").
		//
		// On prend le CENTRE DE MASSE plutot que l'origine du groupe : l'origine d'un
		// AIGroup n'est pas garantie de suivre ses membres, alors que le centre de masse
		// est calcule depuis eux -- c'est la ou le renfort doit apparaitre. Meme API que
		// FFRX_AIAssault.
		vector spawnPos = group.GetCenterOfMass();
		if (spawnPos == vector.Zero)
			return;   // groupe sans membre positionne : rien a renforcer

		ResourceName prefab = CHAR_AT;
		if (wantAA)
			prefab = CHAR_AA;

		IEntity npc = JWK_SpawnUtils.SpawnEntityPrefab(prefab, spawnPos);
		if (!npc)
		{
			Print("[FFRX][Threat] echec du spawn de renfort : " + prefab, LogLevel.WARNING);
			return;
		}

		group.AddAIEntityToGroup(npc);

		if (wantAA)
			m_iSubsAA++;
		else
			m_iSubsAT++;
	}

	//------------------------------------------------------------------------------------------------
	//! Zones de menace au format du tableau "zones" de /ffstate, pour la carte admin.
	//!
	//! On reutilise le tableau existant plutot que d'en creer un : la carte sait deja
	//! dessiner un polygone portant un `tag`, il suffit d'un tag de plus. Les deux
	//! pourcentages voyagent avec, pour l'infobulle.
	//!
	//! Le polygone est celui du NOEUD DE TERRITOIRE (GetAreaPoly) : la zone dessinee est
	//! donc exactement le secteur que le joueur connait, pas un cercle approximatif. Hors
	//! territoire (cle "g:", grille de repli) il n'y a pas de polygone a montrer -- on
	//! sort un carre de la taille de la cellule pour que l'info reste visible.
	//!
	//! Concatenation avec '+' : string.Format tronque a ~8 Ko (cf. memoire).
	string ZonesJson()
	{
		string txt = "";

		foreach (string key, FFRX_ThreatZone z : m_mZones)
		{
			if (!z)
				continue;

			// Sous 5 %, la zone est residuelle : l'afficher ne ferait que salir la carte.
			if (z.m_fArmor < 0.05 && z.m_fAir < 0.05)
				continue;

			string pts = PolyJson(z.m_vAnchor);
			if (pts == "")
				continue;

			if (txt != "")
				txt = txt + ",";

			txt = txt + "{\"tag\":\"threat\",\"pts\":" + pts
				+ ",\"armor\":" + Math.Round(z.m_fArmor * 100).ToString()
				+ ",\"air\":" + Math.Round(z.m_fAir * 100).ToString() + "}";
		}

		return txt;
	}

	//------------------------------------------------------------------------------------------------
	//! Polygone de la zone contenant `pos`, en JSON "[[x,z],...]".
	protected string PolyJson(vector pos)
	{
		array<vector> poly = {};

		JWK_TerritoryControlSystem tcs = JWK.GetTerritoryControl();
		if (tcs)
		{
			JWK_TerritoryControlNodeComponent node = tcs.GetNodeAt(pos);
			if (node)
				node.GetAreaPoly(poly);
		}

		if (poly.Count() < 3)
		{
			// Repli : le carre de la cellule de grille qui contient le point.
			float cx = Math.Floor(pos[0] / FALLBACK_CELL_M) * FALLBACK_CELL_M;
			float cz = Math.Floor(pos[2] / FALLBACK_CELL_M) * FALLBACK_CELL_M;
			float s = FALLBACK_CELL_M;

			poly = {};
			poly.Insert(Vector(cx, 0, cz));
			poly.Insert(Vector(cx + s, 0, cz));
			poly.Insert(Vector(cx + s, 0, cz + s));
			poly.Insert(Vector(cx, 0, cz + s));
		}

		string txt = "[";
		for (int i = 0; i < poly.Count(); i++)
		{
			if (i > 0)
				txt = txt + ",";
			txt = txt + "[" + ((int)poly[i][0]).ToString() + "," + ((int)poly[i][2]).ToString() + "]";
		}
		return txt + "]";
	}

	//------------------------------------------------------------------------------------------------
	//! Rendu texte pour la commande admin #menace.
	string Render()
	{
		string txt = "[FFRX][Threat] zones suivies " + m_mZones.Count().ToString()
			+ " | renforts AT " + m_iSubsAT.ToString()
			+ " / AA " + m_iSubsAA.ToString()
			+ " | intensite max " + Math.Round(MaxShare() * 100).ToString() + "%"
			+ " | demi-vie " + Math.Round(HalfLifeSec() / 60).ToString() + " min\n";

		foreach (string key, FFRX_ThreatZone z : m_mZones)
		{
			if (!z)
				continue;

			txt = txt + "  " + key
				+ "  blinde " + Math.Round(z.m_fArmor * 100).ToString() + "%"
				+ "  aerien " + Math.Round(z.m_fAir * 100).ToString() + "%\n";
		}

		return txt;
	}
}

// ---------------------------------------------------------------------------
//  #menace -- etat des jauges, pour verifier que la boucle tourne.
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_ThreatCommand : ScrServerCommand
{
	override string GetKeyword() { return "menace"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Handle(); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return Handle(); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }

	protected ScrServerCmdResult Handle()
	{
		string txt = FFRX_AdaptiveThreat.Get().Render();
		Print(txt, LogLevel.NORMAL);

		// Le detail par zone part dans le log ; le chat recoit la premiere ligne.
		array<string> lines = {};
		txt.Split("\n", lines, true);
		string head = "";
		if (!lines.IsEmpty())
			head = lines[0];

		return ScrServerCmdResult(head, EServerCmdResultType.OK);
	}
}
