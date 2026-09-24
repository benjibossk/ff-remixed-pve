// FF - REMIXED - PVE
// Pillar 3 (war economy, D5): the marine RESUPPLY you go fetch. Periodically a supply
// vessel arrives in the SEA near the default FOB, carrying a stock of SUPPLIES. Players
// load them into a vehicle and physically haul them back to a base's logistics storage
// (native FF logistics transfer) -- so the supply line is a real, attackable convoy
// (D5 "vulnerable logistics"). The vessel LEAVES after a window if left untouched.
//
// The functional part is a FF logistics crate (LogisticsStorageLarge) seeded with
// supplies -- players transfer from it with FF's own logistics actions. The visual
// SHIP prefab is optional until Benji imports the naval models (set SHIP_PREFAB then).
//
// Server only. ASCII in strings.
class FFRX_MarineResupply
{
	// TODO Benji: set to the imported naval supply-ship prefab GUID. Empty = spawn only
	// the floating supply crate (loop is still testable) until the ship model exists.
	static const ResourceName SHIP_PREFAB  = "";
	static const ResourceName SUPPLY_CRATE = "{0E13E695845CFB76}Prefabs/Compositions/BuildItems/LogisticsStorageLarge.et";

	static const int   SHIP_SUPPLIES   = 8000;    // stock the vessel brings
	static const int   FIRST_DELAY_MS  = 300000;  // first run 5 min after start
	static const int   INTERVAL_MS     = 2400000; // a resupply run every 40 min
	static const int   LIFETIME_MS     = 1800000; // vessel leaves after 30 min if untouched
	static const float SEA_SEARCH_MAX  = 2500;    // look this far from the FOB for open water
	static const float SEA_MIN_DEPTH   = 2.5;     // water must be at least this deep

	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref array<EntityID> s_aActive;

	protected static array<EntityID> Active()
	{
		if (!s_aActive)
			s_aActive = new array<EntityID>();

		return s_aActive;
	}
	protected static bool s_bStarted;

	// Called from the game-mode start (server), next to FFRX_DefaultFob.Boot().
	// ⚠️ LIVRAISON AUTOMATIQUE DESACTIVEE (decision Benji, 2026-09-10).
	//
	// Le ravitaillement par la mer ne se declenche plus tout seul : il ne part QUE sur
	// la commande admin #resupply, qui appelle directement TryResupply() ci-dessous.
	// Le systeme reste donc entierement fonctionnel, seule la minuterie est coupee.
	//
	// POURQUOI on ne supprime pas le code : le declenchement periodique est la seule
	// partie desactivee, et la remettre tient en une ligne. Supprimer TryResupply
	// casserait aussi la commande manuelle.
	//
	// POUR REACTIVER : reintroduire le CallLater commente ci-dessous.
	static void Boot()
	{
		if (!Replication.IsServer()) return;
		if (s_bStarted) return;               // statics survive a mission restart
		s_bStarted = true;

		// GetGame().GetCallqueue().CallLater(TryResupply, FIRST_DELAY_MS, true);
		Print("[FFRX][Marine] Livraison automatique DESACTIVEE -- utiliser #resupply.", LogLevel.NORMAL);
	}

	// Spawn a resupply vessel in the sea near the FOB. Also callable from #resupply.
	static bool TryResupply()
	{
		if (!Replication.IsServer()) return false;

		vector fobPos;
		if (!FindFob(fobPos))
		{
			Print("[FFRX][Marine] No FOB yet -> skipping resupply.", LogLevel.NORMAL);
			return false;
		}

		vector seaPos;
		if (!FindSeaNear(fobPos, seaPos))
		{
			Print("[FFRX][Marine] No open water within range of the FOB -> no resupply.", LogLevel.WARNING);
			return false;
		}

		Print(string.Format("[FFRX][Marine] FOB trouvee a %1 ; eau libre trouvee a %2 (dist FOB %3 m).",
			fobPos.ToString(), seaPos.ToString(), (int)vector.Distance(fobPos, seaPos)), LogLevel.NORMAL);

		// Functional supply crate (players transfer from it via FF logistics).
		IEntity crate = JWK_SpawnUtils.SpawnEntityPrefab(SUPPLY_CRATE, seaPos);
		if (!crate)
		{
			Print("[FFRX][Marine] APPARITION ECHOUEE : la caisse de supplies n'a pas pu spawn.", LogLevel.ERROR);
			return false;
		}
		JWK_LogisticsStorageControllerComponent st =
			JWK_CompTU<JWK_LogisticsStorageControllerComponent>.FindIn(crate);
		if (st)
			st.AddResources(JWK_ELogisticsResourceType.SUPPLIES, SHIP_SUPPLIES);
		else
			Print("[FFRX][Marine] ATTENTION : pas de JWK_LogisticsStorageControllerComponent sur la caisse -> 0 supplies dedans.", LogLevel.WARNING);
		Active().Insert(crate.GetID());
		Print(string.Format("[FFRX][Marine] +++ APPARITION caisse ID=%1 a %2 (%3 supplies).",
			crate.GetID().ToString(), seaPos.ToString(), SHIP_SUPPLIES), LogLevel.NORMAL);

		// Optional visual ship (once the naval model is imported).
		if (SHIP_PREFAB != string.Empty)
		{
			IEntity ship = JWK_SpawnUtils.SpawnEntityPrefab(SHIP_PREFAB, seaPos);
			if (ship)
			{
				Active().Insert(ship.GetID());
				Print(string.Format("[FFRX][Marine] +++ APPARITION navire ID=%1 (%2).",
					ship.GetID().ToString(), SHIP_PREFAB), LogLevel.NORMAL);
			}
			else
			{
				Print(string.Format("[FFRX][Marine] ATTENTION : le prefab navire n'a pas pu spawn (%1).", SHIP_PREFAB), LogLevel.WARNING);
			}
		}
		else
		{
			Print("[FFRX][Marine] (pas de SHIP_PREFAB defini -> caisse seule ; a remplir apres import des modeles navals).", LogLevel.NORMAL);
		}

		int gx, gz;
		SCR_MapEntity.GetGridPos(seaPos, gx, gz);
		Notify(string.Format("Un navire de ravitaillement est arrive en mer pres de la FOB : grille %1 - %2. Chargez les supplies et ramenez-les.", gx, gz));

		// Perishable: the vessel leaves after the window.
		GetGame().GetCallqueue().CallLater(Depart, LIFETIME_MS, false);

		Print(string.Format("[FFRX][Marine] Ravitaillement arrive grille %1-%2 ; %3 entite(s) active(s) ; depart auto dans %4 min.",
			gx, gz, Active().Count(), LIFETIME_MS / 60000), LogLevel.NORMAL);
		return true;
	}

	// Remove whatever is left of the last vessel (crate + ship).
	protected static void Depart()
	{
		int deleted = 0;
		int gone = 0;
		foreach (EntityID id : Active())
		{
			IEntity e = GetGame().GetWorld().FindEntityByID(id);
			if (e)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(e);
				deleted++;
			}
			else
			{
				gone++; // already destroyed (e.g. player emptied/removed it)
			}
		}
		Print(string.Format("[FFRX][Marine] --- DISPARITION ravitaillement : %1 entite(s) supprimee(s), %2 deja disparue(s).",
			deleted, gone), LogLevel.NORMAL);
		Active().Clear();
	}

	protected static bool FindFob(out vector pos)
	{
		World world = GetGame().GetWorld();
		if (!world) return false;
		array<EntityID> fobs = JWK_IndexSystem.Get(world).GetAll(JWK_PlayerFobEntity);
		if (!fobs || fobs.IsEmpty()) return false;
		IEntity fob = world.FindEntityByID(fobs[0]);
		if (!fob) return false;
		pos = fob.GetOrigin();
		return true;
	}

	// Spiral outward from the FOB; return the nearest open-water point (deep enough for a
	// vessel), placed at ocean level. false if no water is found within SEA_SEARCH_MAX.
	protected static bool FindSeaNear(vector from, out vector seaPos)
	{
		World world = GetGame().GetWorld();
		if (!world) return false;
		float oceanY = world.GetOceanBaseHeight();

		float step = 50;
		for (float r = 100; r <= SEA_SEARCH_MAX; r = r + step)
		{
			int samples = Math.Max(8, (int)(r / step) * 6);
			for (int i = 0; i < samples; i++)
			{
				float ang = (i * Math.PI2) / samples;
				float x = from[0] + Math.Cos(ang) * r;
				float z = from[2] + Math.Sin(ang) * r;
				if (world.GetSurfaceY(x, z) < oceanY - SEA_MIN_DEPTH)
				{
					seaPos = Vector(x, oceanY, z);
					return true;
				}
			}
		}
		return false;
	}

	// Broadcast a hint to every player (reuses the intel-hint RPC).
	protected static void Notify(string text)
	{
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int pid : ids)
		{
			JWK_PlayerControllerComponent jpc = JWK.GetPlayerController(pid);
			if (jpc) jpc.FFRX_SendIntelHint(text);
		}
	}
}
