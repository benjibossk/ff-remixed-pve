// FF - REMIXED - PVE
// Fumigenes defensifs des blindes ennemis : un vehicule vise par un missile antichar
// joueur se masque au lieu d'encaisser sans reagir.
//
// POURQUOI : un blinde ennemi etait une cible immobile pour un tireur AT. Un seul tir
// suffisait, sans risque et sans decision. Ici l'ennemi repond : l'ecran de fumee coupe
// la vue, le tireur doit reengager, se deplacer ou flanquer. Le jeu AT garde sa
// puissance mais demande de la methode.
//
// ------------------------------------------------------------------------------------
// POURQUOI CETTE IMPLEMENTATION ET PAS "UN VRAI LANCE-FUMIGENES"
//
// L'idee d'origine (cf. GAME_DESIGN_ARCHIVE) etait de monter un `Dispenser_Smoke` sur
// les prefabs ennemis et de faire tirer l'IA. Deux blocages, verifies dans les sources :
//   1. Le lance-fumigenes vanilla est une ARME DE TOURELLE declenchee a la main par
//      l'equipage. Aucun comportement IA ne sait s'en servir -- ni en vanilla, ni dans
//      WCS (qui n'est qu'un mod d'arsenal, sans logique de contre-mesure).
//   2. `BaseMissileGuidanceComponent` est une classe VIDE cote script : il n'existe
//      aucune API "je suis verrouille par un missile", donc rien a brancher sur l'IA.
//
// On produit donc l'effet directement : on detecte le tir, et on fait apparaitre des
// fumigenes autour du vehicule. Meme resultat en jeu, sans dependre d'un comportement
// IA qui n'existe pas, et sans toucher aux prefabs ennemis (donc rien a re-baker, et
// aucune casse si le mod du vehicule est mis a jour).
//
// ------------------------------------------------------------------------------------
// COMMENT ON DETECTE LE TIR
//
// L'evenement moteur `OnProjectileShot` est emis sur l'entite du TIREUR. On s'y abonne
// pour chaque joueur qui prend le controle d'un personnage. On ne filtre pas sur l'arme
// (chaque mod nomme les siennes a sa facon) mais sur le PROJECTILE : s'il porte un
// `MissileMoveComponent`, c'est un missile guide. Ce test ne depend d'aucun nom de
// prefab, donc il marche avec les lanceurs vanilla comme avec ceux des mods.
//
// NOTE : ASCII uniquement dans les chaines/commentaires (le build du dedie desynchronise
// sur l'UTF-8).

class FFRX_SmokeScreen
{
	// Fumigene sovietique blanc : coherent avec du materiel ennemi, et sa fumee est
	// dense. TimerTriggerComponent a TIMER 0 -> il fume des qu'on l'arme.
	static const ResourceName SMOKE_PREFAB =
		"{77EAE5E07DC4678A}Prefabs/Weapons/Grenades/Smoke_RDG2.et";

	// Portee de recherche du vehicule vise, le long de la trajectoire du missile.
	static const float SEARCH_RANGE = 2000;
	// Demi-angle du cone de visee. 12 degres : assez large pour un tir a 1,5 km, assez
	// etroit pour ne pas declencher un vehicule qui passait juste a cote.
	static const float CONE_DEG = 12;

	// Delai de reaction de l'equipage. Sans ce delai la fumee apparait AVANT que le
	// joueur ait vu son missile partir : ca semble triche. Avec ~1 s, on lit la reaction.
	static const int REACTION_MS = 900;

	// Un vehicule ne peut pas se masquer en boucle : sinon deux tireurs le rendent
	// invulnerable a la vue en permanence. Une salve, puis rechargement.
	static const int COOLDOWN_MS = 45000;

	// Nombre de pots et rayon de l'arc devant le vehicule.
	static const int SMOKE_COUNT = 5;
	static const float SMOKE_RADIUS = 7.0;
	static const float SMOKE_HEIGHT = 1.2;

	// Dernier tir de fumigenes par vehicule (EntityID -> horodatage monde en ms).
	// On indexe par ID et NON par IEntity : garder une reference sur une entite
	// detruite empecherait sa liberation.
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref map<EntityID, int> s_mLastSmoke;

	protected static map<EntityID, int> LastSmoke()
	{
		if (!s_mLastSmoke)
			s_mLastSmoke = new map<EntityID, int>();

		return s_mLastSmoke;
	}

	// Accumulateur de la requete sphere (appels serveur sequentiels -> statique sur).
	protected static ref array<IEntity> s_aHits;

	//------------------------------------------------------------------------------------
	//! Le joueur `shooter` vient de tirer `projectile`. Ne fait rien si ce n'est pas un
	//! missile guide.
	static void OnPlayerShot(IEntity shooter, IEntity projectile)
	{
		if (!Replication.IsServer() || !shooter || !projectile)
			return;

		// Filtre par COMPOSANT et non par nom de prefab : marche avec tous les mods.
		if (!MissileMoveComponent.Cast(projectile.FindComponent(MissileMoveComponent)))
			return;

		vector origin = projectile.GetOrigin();
		vector dir = ForwardOf(projectile);
		if (dir == vector.Zero)
			return;

		IEntity target = FindTargetVehicle(origin, dir, shooter);
		if (!target)
			return;

		int now = GetGame().GetWorld().GetWorldTime();
		int last;
		if (LastSmoke().Find(target.GetID(), last) && now - last < COOLDOWN_MS)
			return;
		LastSmoke().Set(target.GetID(), now);

		// L'equipage "voit" le depart et reagit -- d'ou le delai.
		GetGame().GetCallqueue().CallLater(PopSmoke, REACTION_MS, false, target, origin);
	}

	//------------------------------------------------------------------------------------
	//! Vehicule ennemi le mieux aligne sur la trajectoire du missile, ou null.
	protected static IEntity FindTargetVehicle(vector origin, vector dir, IEntity shooter)
	{
		World world = GetGame().GetWorld();
		if (!world)
			return null;

		// Sphere centree a mi-portee : couvre le cone utile en UNE requete. Une requete
		// de 2 km depuis le tireur ramasserait toute la carte pour rien.
		vector centre = origin + dir * (SEARCH_RANGE * 0.5);

		s_aHits = new array<IEntity>();
		world.QueryEntitiesBySphere(centre, SEARCH_RANGE * 0.5, CollectVehicle, null, EQueryEntitiesFlags.ALL);

		float cosLimit = Math.Cos(CONE_DEG * Math.DEG2RAD);
		IEntity best;
		float bestDist = float.MAX;

		foreach (IEntity e : s_aHits)
		{
			if (!e || e == shooter)
				continue;

			vector to = e.GetOrigin() - origin;
			float dist = to.Length();
			if (dist < 1 || dist > SEARCH_RANGE)
				continue;

			// Alignement : produit scalaire entre l'axe du missile et la direction du
			// vehicule. Au-dela du cone, le missile ne le vise pas.
			if (vector.Dot(dir, to * (1 / dist)) < cosLimit)
				continue;

			if (!IsEnemyVehicle(e))
				continue;

			// Le plus proche dans le cone = celui qui est vise (les autres sont derriere).
			if (dist < bestDist)
			{
				bestDist = dist;
				best = e;
			}
		}
		return best;
	}

	protected static bool CollectVehicle(IEntity e)
	{
		if (Vehicle.Cast(e))
			s_aHits.Insert(e);
		return true; // continuer la requete
	}

	//! Vehicule appartenant a la faction ENNEMIE (on ne masque ni nos blindes ni les civils).
	protected static bool IsEnemyVehicle(IEntity e)
	{
		JWK_FactionManager fm = JWK.GetFactions();
		if (!fm)
			return false;
		return fm.GetEntityRole(e) == JWK_EFactionRole.ENEMY;
	}

	//------------------------------------------------------------------------------------
	//! Fait apparaitre l'ecran de fumee en arc DU COTE du tireur : se masquer a l'oppose
	//! ne servirait a rien.
	protected static void PopSmoke(IEntity vehicle, vector threatFrom)
	{
		if (!vehicle)
			return;

		World world = GetGame().GetWorld();
		if (!world)
			return;

		// --- Aeronef : LEURRES THERMIQUES, pas de fumigenes ---
		// Un helico ne se masque pas derriere un ecran de fumee : il largue des leurres.
		// Et s'il n'en porte pas, on ne fait RIEN -- poser de la fumee au sol sous un
		// appareil en vol n'aurait aucun sens.
		if (IsAircraft(vehicle))
		{
			if (FFRX_SmokeWCS.TryFireFlares(vehicle))
				Print(string.Format("[FFRX][Smoke] leurres largues par %1.", NameOf(vehicle)), LogLevel.NORMAL);
			return;
		}

		// --- Vehicule terrestre : vrai lance-fumigenes d'abord ---
		// Depart visible, son et dispersion d'origine. Sinon seulement, on simule.
		// Cf. FFRX_VehicleSmokeWCS.c.
		if (FFRX_SmokeWCS.TryFireRealLauncher(vehicle))
		{
			Print(string.Format("[FFRX][Smoke] lance-fumigenes WCS tire sur %1.",
				NameOf(vehicle)), LogLevel.NORMAL);
			return;
		}

		vector centre = vehicle.GetOrigin();
		vector toThreat = threatFrom - centre;
		toThreat[1] = 0;
		if (toThreat.LengthSq() < 1)
			return;
		toThreat.Normalize();

		// Perpendiculaire horizontale, pour etaler les pots en arc face a la menace.
		vector side = vector.Up * toThreat;
		side.Normalize();

		Resource res = Resource.Load(SMOKE_PREFAB);
		if (!res || !res.IsValid())
		{
			Print("[FFRX][Smoke] prefab fumigene introuvable.", LogLevel.WARNING);
			return;
		}

		int placed = 0;
		for (int i = 0; i < SMOKE_COUNT; i++)
		{
			// Reparti de -50 a +50 degres autour de l'axe de la menace.
			float t = 0;
			if (SMOKE_COUNT > 1)
				t = (i / (float)(SMOKE_COUNT - 1)) * 2 - 1; // -1 .. +1
			float spread = t * 0.85; // ~ +/- 50 deg en radians

			vector dir = toThreat * Math.Cos(spread) + side * Math.Sin(spread);
			vector pos = centre + dir * SMOKE_RADIUS;
			pos[1] = world.GetSurfaceY(pos[0], pos[2]) + SMOKE_HEIGHT;

			vector mat[4];
			Math3D.MatrixIdentity4(mat);
			mat[3] = pos;

			EntitySpawnParams params = new EntitySpawnParams();
			params.TransformMode = ETransformMode.WORLD;
			params.Transform = mat;

			IEntity smoke = GetGame().SpawnEntityPrefab(res, world, params);
			if (!smoke)
				continue;

			// Armer le pot : avec TIMER 0 la fumee part immediatement.
			BaseTriggerComponent trig = BaseTriggerComponent.Cast(smoke.FindComponent(BaseTriggerComponent));
			if (trig)
			{
				trig.SetLive();
				placed = placed + 1;
			}
		}

		Print(string.Format("[FFRX][Smoke] ecran de fumee (%1 pots) sur %2.",
			placed, NameOf(vehicle)), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------
	//! Aeronef ? Test par le COMPOSANT DE VOL et non par le nom du prefab : marche pour
	//! les helicos vanilla comme pour ceux des mods (DarcChopper, etc.).
	protected static bool IsAircraft(IEntity e)
	{
		if (!e)
			return false;
		return HelicopterControllerComponent.Cast(e.FindComponent(HelicopterControllerComponent)) != null;
	}

	//! Axe avant d'une entite, extrait de sa matrice monde.
	protected static vector ForwardOf(IEntity e)
	{
		vector mat[4];
		e.GetWorldTransform(mat);
		vector fwd = mat[2];
		if (fwd.LengthSq() < 0.001)
			return vector.Zero;
		fwd.Normalize();
		return fwd;
	}

	protected static string NameOf(IEntity e)
	{
		if (!e)
			return "null";
		EntityPrefabData pd = e.GetPrefabData();
		if (pd)
			return pd.GetPrefabName();
		return e.GetName();
	}
}

//----------------------------------------------------------------------------------------
// Abonnement : `OnProjectileShot` est emis sur l'entite du tireur, il faut donc s'abonner
// sur CHAQUE personnage controle par un joueur -- et se reabonner a chaque respawn, vu
// que le joueur change d'entite.
//----------------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (!Replication.IsServer())
			return;

		// Se desabonner de l'ancienne entite : sans ca, chaque respawn empilerait un
		// abonnement de plus sur des personnages morts.
		if (from)
		{
			EventHandlerManagerComponent oldEh =
				EventHandlerManagerComponent.Cast(from.FindComponent(EventHandlerManagerComponent));
			if (oldEh)
				oldEh.RemoveScriptHandler("OnProjectileShot", this, FFRX_OnProjectileShot);
		}

		if (!to)
			return;

		EventHandlerManagerComponent eh =
			EventHandlerManagerComponent.Cast(to.FindComponent(EventHandlerManagerComponent));
		if (eh)
			eh.RegisterScriptHandler("OnProjectileShot", this, FFRX_OnProjectileShot);
	}

	//! Signature imposee par l'evenement moteur : (playerID, arme, projectile).
	//!
	//! Ce handler est le SEUL point d'ecoute des tirs joueur. Les fonctionnalites qui en
	//! dependent s'y branchent a la suite plutot que de poser leur propre abonnement.
	//!
	//! ⚠️ CORRECTION 2026-09-21 : la raison invoquee ici etait fausse ("deux modded class
	//! surchargeant `OnControlledEntityChanged` dans le meme addon, c'est une definition en
	//! double"). C'est exactement ce que fait le mod aujourd'hui -- ce fichier-ci et
	//! FFRX_IntroCinematic.c:351 surchargent TOUS LES DEUX cette methode -- et ca compile.
	//! Les `modded class` s'enchainent tant que chacun appelle `super`.
	//!
	//! La vraie raison de garder UN SEUL point d'ecoute tient quand meme : l'ordre de la
	//! chaine entre deux blocs du meme addon n'est pas garanti, et un abonnement unique evite
	//! d'oublier un desabonnement au respawn.
	protected void FFRX_OnProjectileShot(int playerID, BaseWeaponComponent weapon, IEntity projectile)
	{
		IEntity shooter = GetControlledEntity();

		// Contre-mesures des vehicules vises par un missile guide.
		FFRX_SmokeScreen.OnPlayerShot(shooter, projectile);

		// Anti-camping : accumule une signature de tir sur la POSITION (cf. FFRX_AntiCamping).
		FFRX_CampWatch.OnPlayerShot(playerID, shooter, weapon);

		// Tracantes : une balle lumineuse designe le tireur aux ennemis qui le voient
		// (cf. FFRX_Tracer.c). Sort immediatement si le projectile n'est pas une tracante.
		FFRX_TracerWatch.OnPlayerShot(playerID, shooter, projectile);
	}
}
