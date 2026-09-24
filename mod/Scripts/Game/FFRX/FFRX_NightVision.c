// FF - REMIXED - PVE
// LA NUIT REDEVIENT UN AVANTAGE -- sauf pour les rares ennemis equipes en vision nocturne.
//
// ------------------------------------------------------------------------------------
// LE PROBLEME : LE MOTEUR IGNORE LES OPTIQUES NV POUR L'IA
//
// Un joueur qui sort la nuit ne gagne rien contre l'IA : elle voit exactement aussi bien
// a 3 h du matin qu'a midi. Le moteur ne modelise la lumiere ambiante NI en malus pour
// l'IA sans equipement, NI en bonus pour celle qui porte une lunette de vision nocturne.
// Verifie : ni FF ni le jeu de base ne referencent la moindre notion de nuit dans leurs
// calculs de perception (aucune occurrence de NightVision / IsNight cote perception IA).
//
// Consequence de design : la nuit ne sert a rien. Elle devrait etre le moment ou une
// guerilla mal equipee prend l'avantage sur une armee reguliere.
//
// ------------------------------------------------------------------------------------
// CE QU'ON FAIT
//
// La nuit, l'IA ennemie voit MOINS BIEN -- son facteur de perception est reduit. Sauf si
// elle porte une optique de vision nocturne, auquel cas elle garde sa vue de jour. Les
// porteurs de NV etant rares (c'est une piece d'equipement, pas une dotation standard),
// une patrouille de nuit devient majoritairement aveugle, avec quelques elements
// dangereux dedans -- et le joueur ne sait pas lesquels avant de se faire reperer.
//
// On ne DONNE pas de NV a l'IA ici : la distribution se fait dans les dotations
// (TacticalFlava equipe deja quelques persos USSR avec la Dedal Narodovolec). Ce fichier
// ne fait que traduire en perception un equipement que le moteur ignorait.
//
// ------------------------------------------------------------------------------------
// COMMENT ON S'ACCROCHE
//
// Pas de nouveau composant ni de nouveau tick : FFRX_AIDifficulty tient deja un registre
// de tous les SCR_AICombatComponent ennemis et sait les rafraichir en masse
// (FFRX_ReapplyAll). On se greffe sur son calcul de m_fPerceptionFactor, et un seul
// timer global surveille le passage jour/nuit pour declencher un rafraichissement.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict, cf. memoire).

class FFRX_NightVision
{
	// Optiques de vision nocturne connues. Un porteur garde sa vue de jour.
	// Dedal Narodovolec : lunette NV russe de TacticalFlava, montee sur arme.
	static const ResourceName OPTIC_DEDAL = "{D8147F3F740C29DF}Prefabs/Weapons/Attachments/Optics/Dedal_Narodovolec/Dedal_Narodovolec.et";

	// Bornes de la nuit, en heures locales. Entre les deux = jour.
	// Large a dessein : l'aube et le crepuscule sont deja des periodes ou l'oeil nu
	// fonctionne, inutile d'y ajouter un malus.
	static const float NIGHT_START_H = 21.0;
	static const float NIGHT_END_H   = 5.0;

	// Cadence de surveillance du cycle jour/nuit. La bascule n'a pas besoin d'etre a la
	// minute pres, et un rafraichissement de masse n'est pas gratuit.
	static const int WATCH_MS = 60000;

	protected static bool s_bWasNight;
	protected static bool s_bStarted;

	// ---- Observabilite ----
	// Un systeme probabiliste a 8 % ne se verifie pas a l'oeil en jeu : sans compteur, on
	// ne sait pas distinguer "ca marche et c'est rare" de "ca ne marche pas du tout".
	// On compte donc les tirages, et on sort le bilan avec le rapport periodique.
	protected static int s_iRolled;      // ennemis passes par le tirage
	protected static int s_iCarriers;    // dont porteurs d'une optique NV
	protected static int s_iFromLoadout; // dont ceux qui l'avaient DEJA par leur dotation

	//! Appele par FFRX_AIDifficulty a chaque tirage, pour la statistique.
	static void NoteRoll(bool carrier, bool fromLoadout)
	{
		s_iRolled++;
		if (carrier)
			s_iCarriers++;
		if (fromLoadout)
			s_iFromLoadout++;
	}

	//------------------------------------------------------------------------------------------------
	//! Bilan lisible : ce qu'on a tire, et ce que ca donne par rapport au reglage.
	static string Render()
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();

		float wantPct = 0;
		float eliteMult = 0;
		float nightPct = 0;
		if (cache)
		{
			wantPct   = cache.m_fFFRX_NVGPct;
			eliteMult = cache.m_fFFRX_NVGEliteMult;
			nightPct  = cache.m_fFFRX_NightPerception;
		}

		int gotPct = 0;
		if (s_iRolled > 0)
			gotPct = (s_iCarriers * 100) / s_iRolled;

		string night = "JOUR";
		if (IsNight())
			night = "NUIT";

		// string.Format PLUTOT QUE DES '+' EN CHAINE.
		//
		// Une longue concatenation fait echouer le compilateur sur "Formula too complex",
		// et l'erreur part en cascade sur des dizaines de fichiers du jeu de base qui ne
		// citent meme pas celui-ci (constate le 2026-09-17 : 28 erreurs, dont 27 fausses).
		// Decouper en plusieurs `txt = txt + ...` ne suffisait pas : chaque ligne restait
		// trop chargee. Format prend les valeurs en parametres, il n'y a plus d'expression
		// a evaluer.
		//
		// Limite a connaitre : string.Format s'arrete a %9. On en utilise 8.
		return string.Format(
			"[FFRX][NVG] %1 | porteurs %2/%3 (%4%%, reglage %5%%) | dotation %6, posees %7, sans rail %8 | les autres voient a %9%% la nuit",
			night, s_iCarriers, s_iRolled, gotPct, Math.Round(wantPct),
			s_iFromLoadout, s_iGranted, s_iGrantFailed, Math.Round(nightPct));
	}

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_bStarted)
			return;

		s_bStarted = true;
		s_bWasNight = IsNight();
		GetGame().GetCallqueue().CallLater(Watch, WATCH_MS, true);
		Print("[FFRX][NVG] Perception nocturne active.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Fait-il nuit ? Faux si l'heure du monde n'est pas lisible : on ne veut pas
	//! aveugler toute l'IA sur une lecture ratee.
	static bool IsNight()
	{
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		TimeAndWeatherManagerEntity tawme = world.GetTimeAndWeatherManager();
		if (!tawme)
			return false;

		float hour = tawme.GetTimeOfTheDay();

		// La nuit enjambe minuit : elle est donc l'UNION de deux intervalles, pas une
		// simple comparaison encadree.
		return (hour >= NIGHT_START_H || hour < NIGHT_END_H);
	}

	//------------------------------------------------------------------------------------------------
	//! Bascule jour/nuit -> on rafraichit toute l'IA ennemie vivante.
	protected static void Watch()
	{
		bool night = IsNight();
		if (night == s_bWasNight)
			return;

		s_bWasNight = night;

		if (night)
			Print("[FFRX][NVG] La nuit tombe : l'ennemi sans optique NV voit moins bien.", LogLevel.NORMAL);
		else
			Print("[FFRX][NVG] Le jour se leve : perception ennemie normale.", LogLevel.NORMAL);

		// Le bilan au moment ou ca bascule : c'est la qu'il est le plus parlant.
		Print(Render(), LogLevel.NORMAL);

		FFRX_AIDifficulty_ReapplyAll();
	}

	//------------------------------------------------------------------------------------------------
	//! Indirection : FFRX_ReapplyAll est une static de SCR_AICombatComponent, on la
	//! traverse ici pour garder ce fichier independant de l'ordre de compilation.
	protected static void FFRX_AIDifficulty_ReapplyAll()
	{
		SCR_AICombatComponent.FFRX_ReapplyAll();
	}

	//------------------------------------------------------------------------------------------------
	//! Ce soldat est-il equipe en vision nocturne ? Tirage au sort, une seule fois par
	//! soldat (le resultat est mis en cache par l'appelant).
	//!
	//! DEUX CURSEURS, comme demande : une part de base pour la troupe, multipliee pour les
	//! forces d'elite. Un Spetsnaz est nettement mieux dote qu'un conscrit -- avec les
	//! valeurs par defaut (8 % et x4) : 8 % chez les reguliers et la marine, 32 % chez les
	//! KLMK et les Spetsnaz. Une patrouille de nuit ordinaire est donc quasi aveugle,
	//! tandis qu'un groupe d'elite garde un ou deux yeux valides.
	static bool RollNightOptic(FFRX_EForce force, int tier)
	{
		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return false;

		float pct = cache.m_fFFRX_NVGPct;
		if (pct <= 0)
			return false;

		// Elite = KLMK et Spetsnaz. L'echelle de dangerosite du mod est
		// Regulier < Naval < KLMK < Spetsnaz : le multiplicateur suit cette marche.
		if (force == FFRX_EForce.SPETSNAZ || force == FFRX_EForce.KLMK)
			pct = pct * cache.m_fFFRX_NVGEliteMult;

		// Niveau de l'escouade : c'est ce qui fait vivre le systeme sur un theatre sans
		// sous-forces. Sur Anizay tous les MEI sont "Reguliers", donc SANS ce facteur
		// chaque patrouille aurait exactement le meme taux -- et le multiplicateur elite
		// ci-dessus ne s'appliquerait jamais (constate : 7/123, tous au taux de base).
		pct = pct * FFRX_SquadTier.NvgMultiplier(tier);

		if (pct > 100)
			pct = 100;

		return (JWK.Random.RandFloat01() * 100) < pct;
	}

	//------------------------------------------------------------------------------------------------
	//! Multiplicateur de perception a appliquer a CET ennemi, compte tenu de l'heure et
	//! de son equipement. 1 = inchange.
	//!
	//! `hasOptic` est le resultat du tirage, fait une fois a l'init du soldat. On ne le
	//! recalcule pas ici : PerceptionMultiplier est rappele a chaque bascule jour/nuit et
	//! a chaque changement de reglage, et un soldat ne doit pas gagner puis perdre ses
	//! jumelles au fil de la nuit.
	static float PerceptionMultiplier(bool hasOptic)
	{
		if (!IsNight())
			return 1;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return 1;

		// Reglage a 100 % = pas de malus, le systeme est neutralise.
		float nightPct = cache.m_fFFRX_NightPerception;
		if (nightPct >= 100)
			return 1;

		// Un porteur d'optique NV ne subit pas le malus. On ne lui donne pas de bonus
		// au-dela de sa vue de jour : l'avantage vient de ce que les AUTRES sont diminues.
		if (hasOptic)
			return 1;

		return nightPct / 100;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce personnage porte-t-il une optique de vision nocturne sur son arme en main ?
	//!
	//! On regarde l'arme COURANTE et ses attachements. Une optique rangee dans le sac ne
	//! compte pas -- ce qui est exactement le comportement voulu.
	//! Pose REELLEMENT l'optique sur l'arme du soldat, pour qu'elle se voie et se loote.
	//!
	//! Differe : a l'init du composant de combat, l'arme n'est pas encore en main. On
	//! laisse la dotation se poser avant d'y toucher.
	static void GrantOptic(IEntity character)
	{
		if (!character)
			return;

		GetGame().GetCallqueue().CallLater(DoGrantOptic, GRANT_DELAY_MS, false, character);
	}

	protected static const int GRANT_DELAY_MS = 4000;

	//------------------------------------------------------------------------------------------------
	protected static void DoGrantOptic(IEntity character)
	{
		if (!character)
			return;

		// Deja equipe (par sa dotation, ou par un passage precedent) : on ne double pas.
		if (HasNightOptic(character))
			return;

		BaseWeaponManagerComponent wm = JWK_CompTU<BaseWeaponManagerComponent>.FindIn(character);
		if (!wm)
			return;

		BaseWeaponComponent weapon = wm.GetCurrentWeapon();
		if (!weapon)
			return;

		IEntity weaponEnt = weapon.GetOwner();
		if (!weaponEnt)
			return;

		InventoryStorageManagerComponent mgr = InventoryStorageManagerComponent.Cast(
			character.FindComponent(InventoryStorageManagerComponent));
		if (!mgr)
			return;

		// Le rail de l'arme. Sans stockage d'attachements, l'arme n'accepte tout
		// simplement pas d'optique (beaucoup d'armes MEI sont dans ce cas).
		SCR_WeaponAttachmentsStorageComponent rail = SCR_WeaponAttachmentsStorageComponent.Cast(
			weaponEnt.FindComponent(SCR_WeaponAttachmentsStorageComponent));
		if (!rail)
			return;

		Resource res = Resource.Load(OPTIC_DEDAL);
		if (!res || !res.IsValid())
			return;

		EntitySpawnParams p = new EntitySpawnParams();
		p.TransformMode = ETransformMode.WORLD;
		p.Transform[3] = character.GetOrigin();

		IEntity optic = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), p);
		if (!optic)
			return;

		// Meme forme que JWK_RespawnLoadoutHandler.TryInsertItem : si l'insertion echoue
		// (rail incompatible, emplacement occupe par une autre lunette), on DETRUIT l'item
		// au lieu de le laisser tomber au sol -- sinon le sol se couvre d'optiques la nuit.
		// Le soldat garde son bonus de perception : le tirage fait autorite, l'objet n'est
		// qu'un habillage (cf. FFRX_AIDifficulty).
		if (!mgr.TryInsertItemInStorage(optic, rail))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(optic);
			s_iGrantFailed++;
			return;
		}

		s_iGranted++;
	}

	// Combien d'optiques ont ete reellement posees, et combien ont echoue faute de rail
	// compatible. Sert a savoir si "0 par dotation" vient de nous ou des armes.
	protected static int s_iGranted;
	protected static int s_iGrantFailed;

	//------------------------------------------------------------------------------------------------
	static bool HasNightOptic(IEntity character)
	{
		if (!character)
			return false;

		BaseWeaponManagerComponent wm = JWK_CompTU<BaseWeaponManagerComponent>.FindIn(character);
		if (!wm)
			return false;

		BaseWeaponComponent weapon = wm.GetCurrentWeapon();
		if (!weapon)
			return false;

		IEntity weaponEnt = weapon.GetOwner();
		if (!weaponEnt)
			return false;

		// Les attachements sont des entites ENFANTS de l'arme : on parcourt la fratrie
		// plutot que de depender d'une API de stockage qui varie selon les versions.
		IEntity child = weaponEnt.GetChildren();
		while (child)
		{
			EntityPrefabData pd = child.GetPrefabData();
			if (pd && pd.GetPrefabName() == OPTIC_DEDAL)
				return true;

			child = child.GetSibling();
		}

		return false;
	}
}

// ---------------------------------------------------------------------------
//  #nvg -- etat de la vision nocturne et des gilets suicide, a la demande.
//
//  Les deux systemes sont probabilistes et invisibles : sans ce bilan, on ne peut pas
//  distinguer "ca marche et c'est rare" de "ca ne marche pas". Les compteurs sont
//  cumulatifs depuis le demarrage du serveur.
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_NvgCommand : ScrServerCommand
{
	override string GetKeyword() { return "nvg"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Handle(); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return Handle(); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }

	protected ScrServerCmdResult Handle()
	{
		string nvg  = FFRX_NightVision.Render();
		string vest = FFRX_SuicideVest.Render();
		string tier = FFRX_SquadTier.Render();

		Print(nvg, LogLevel.NORMAL);
		Print(vest, LogLevel.NORMAL);
		Print(tier, LogLevel.NORMAL);

		// Le chat ne tient pas deux lignes longues : le detail part dans le log.
		return ScrServerCmdResult(nvg, EServerCmdResultType.OK);
	}
}
