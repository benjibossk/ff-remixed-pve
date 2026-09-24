// FF - REMIXED - PVE
// TRACANTES : tirer une balle lumineuse doit se payer.
//
// ------------------------------------------------------------------------------------
// LE PROBLEME
//
// Une tracante est une fleche lumineuse qui pointe la position de depart. Dans le jeu,
// elle ne coute STRICTEMENT RIEN : la perception de l'IA les ignore totalement.
// Verifie dans les sources du jeu de base -- recherche sur l'ensemble des scripts IA :
// aucune occurrence (les rares resultats d'une recherche insensible a la casse sont des
// `traceResult`, sans rapport). Le moteur sait pourtant ce qu'est une tracante : il
// existe une classe d'entite dediee `TracerProjectile`, un `Ammo_BulletTracer_Base.et`
// et 7 munitions tracantes par calibre. Mais rien ne relie ca a la detection.
//
// Ce n'est donc pas un reglage trop faible qu'on pourrait monter : c'est une ABSENCE DE
// CABLAGE, exactement comme le fumigene de l'assaut (cf. FFRX_AIAssault.FFRX_PopSmoke).
//
// ------------------------------------------------------------------------------------
// CE QU'ON FAIT
//
// Quand un joueur tire une tracante, les ennemis qui ont une LIGNE DE VUE sur lui le
// reperent plus vite pendant quelques secondes. Rien d'autre : on ne leur donne pas sa
// position, on ne les fait pas converger. Ils regardent mieux, c'est tout -- le joueur
// garde la possibilite de se deplacer avant d'etre acquis.
//
// L'interet de jeu : ca ferme une boucle deja ouverte. Le silencieux doit REDUIRE la
// signature sonore (note dans l'anti-camping) ; la tracante AUGMENTE la signature
// visuelle. Les deux ensemble font du choix des munitions et des accessoires une vraie
// decision tactique, au lieu de "prendre le mieux disponible".
//
// ------------------------------------------------------------------------------------
// TROIS CHOIX QUI COMPTENT
//
// 1. DETECTION PAR TYPE D'ENTITE, pas par nom de prefab.
//    `TracerProjectile.Cast(projectile)` -- donc ca marche aussi avec les munitions des
//    mods, sans liste a maintenir. (Verifie cote dotation : AMF fournit bien des
//    tracantes, dont la bande MINIMI 4Ball/1Tracer et les chargeurs STANAG 5-tracantes,
//    donc le systeme se declenchera reellement.)
//
// 2. AUCUNE REQUETE SPATIALE.
//    Le registre des composants de combat ennemis existe deja (FFRX_AIDifficulty), il
//    suffit de le filtrer par distance. Une requete spatiale par balle serait ruineuse --
//    meme lecon que l'anti-camping, qui ne fait son controle de portee qu'une fois.
//
// 3. LE JOUR, PRESQUE RIEN. LA NUIT, BEAUCOUP.
//    De jour une tracante se remarque a peine, de nuit elle se voit de tres loin. Sans
//    cette distinction on punirait surtout les joueurs de jour, qui ne comprendraient
//    meme pas pourquoi.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict).

class FFRX_TracerWatch
{
	protected static bool s_bStarted;

	//! Balayage des echeances. Pas besoin d'etre precis : c'est un retour a la normale.
	protected static const int TICK_MS = 2000;

	//! Duree pendant laquelle un ennemi reste "eclaire" apres la derniere tracante.
	protected static const float BOOST_MS = 12000;

	//! Au-dela, meme une tracante ne designe plus personne de facon utile.
	protected static const float RADIUS_M = 400;

	//! Une rafale de MINIMI, c'est ~15 balles/s dont une tracante sur cinq. On ne refait
	//! pas le balayage a chaque impulsion : une fois par seconde et par tireur suffit,
	//! puisque l'effet dure 12 s de toute facon.
	protected static const float PER_SHOOTER_COOLDOWN_MS = 1000;

	//! La nuit multiplie l'effet (cf. choix 3 en tete de fichier).
	protected static const float NIGHT_FACTOR = 2.0;

	//! Dernier balayage par tireur. Cle = identifiant joueur.
	protected static ref map<int, float> s_mLastScan;

	//! Statistiques pour #tracante.
	protected static int s_iScans;
	protected static int s_iBoosted;

	//------------------------------------------------------------------------------------------------
	static void Boot()
	{
		if (!Replication.IsServer())
			return;
		if (s_bStarted)
			return;

		s_bStarted = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		Print("[FFRX][Tracante] Signature visuelle des tracantes active.", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Appele pour CHAQUE tir joueur depuis l'unique point d'ecoute
	//! (FFRX_VehicleSmokeScreen.c, modded SCR_PlayerController.FFRX_OnProjectileShot).
	//! Doit donc sortir le plus tot possible dans le cas general.
	static void OnPlayerShot(int playerId, IEntity shooter, IEntity projectile)
	{
		if (!projectile)
			return;

		// Le filtre le moins cher en premier : la immense majorite des balles ne sont pas
		// des tracantes, et ce test est un simple controle de type.
		if (!TracerProjectile.Cast(projectile))
			return;

		if (!shooter)
			return;

		JWK_GameSettingsCache cache = JWK.GameSettingsCache();
		if (!cache)
			return;

		float boostPct = cache.m_fFFRX_TracerBoost;
		if (boostPct <= 0)
			return;   // reglage a 0 = desactive

		float now = GetGame().GetWorld().GetWorldTime();

		if (!s_mLastScan)
			s_mLastScan = new map<int, float>();

		float last;
		if (s_mLastScan.Find(playerId, last) && now - last < PER_SHOOTER_COOLDOWN_MS)
			return;

		s_mLastScan.Set(playerId, now);

		Scan(shooter, boostPct, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Passe en revue les ennemis vivants et eclaire ceux qui VOIENT le tireur.
	protected static void Scan(IEntity shooter, float boostPct, float now)
	{
		array<SCR_AICombatComponent> registry = SCR_AICombatComponent.FFRX_Registry();
		if (!registry)
			return;

		float mult = 1 + boostPct * 0.01;
		if (FFRX_NightVision.IsNight())
			mult = 1 + boostPct * 0.01 * NIGHT_FACTOR;

		vector shooterPos = shooter.GetOrigin();
		// La tracante part de l'arme, pas des pieds : on vise le torse, sinon le trace
		// bute sur le relief a chaque tir en position couchee ou derriere un muret.
		vector shooterEye = Vector(shooterPos[0], shooterPos[1] + 1.4, shooterPos[2]);

		float radiusSq = RADIUS_M * RADIUS_M;
		int lit = 0;

		s_iScans++;

		foreach (SCR_AICombatComponent comp : registry)
		{
			if (!comp)
				continue;

			IEntity ai = comp.GetOwner();
			if (!ai)
				continue;

			if (vector.DistanceSq(ai.GetOrigin(), shooterPos) > radiusSq)
				continue;

			if (!comp.FFRX_IsEnemyCombatant())
				continue;

			if (!HasLineOfSight(ai, shooterEye))
				continue;

			comp.FFRX_SetTracerBoost(mult, now + BOOST_MS);
			lit++;
		}

		if (lit > 0)
			s_iBoosted = s_iBoosted + lit;
	}

	//------------------------------------------------------------------------------------------------
	//! Le soldat voit-il le point d'ou part la tracante ?
	//!
	//! Sans ce controle, un ennemi derriere une colline "verrait" la tracante et se
	//! mettrait a chercher le tireur -- exactement le genre de reaction qui donne
	//! l'impression que le jeu triche.
	protected static bool HasLineOfSight(IEntity ai, vector targetEye)
	{
		vector pos = ai.GetOrigin();

		TraceParam trace = new TraceParam();
		trace.Start = Vector(pos[0], pos[1] + 1.5, pos[2]);   // hauteur des yeux, debout
		trace.End = targetEye;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = ai;

		// TraceMove rend 1.0 quand RIEN n'a ete touche sur le trajet : vue degagee.
		return GetGame().GetWorld().TraceMove(trace, null) == 1.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Balayage des echeances : rend sa perception normale a qui n'est plus eclaire.
	protected static void Tick()
	{
		array<SCR_AICombatComponent> registry = SCR_AICombatComponent.FFRX_Registry();
		if (!registry)
			return;

		float now = GetGame().GetWorld().GetWorldTime();

		foreach (SCR_AICombatComponent comp : registry)
		{
			if (comp)
				comp.FFRX_ExpireTracerBoost(now);
		}
	}

	//------------------------------------------------------------------------------------------------
	static string Render()
	{
		return string.Format("[FFRX][Tracante] %1 salves tracantes reperees, %2 designations d'ennemi au total.",
			s_iScans, s_iBoosted);
	}
}

// ---------------------------------------------------------------------------
//  #tracante -- bilan, a la demande.
//
//  Comme la vision nocturne, l'effet est invisible en jeu : sans ce bilan on ne peut
//  pas distinguer "ca marche mais personne ne tire de tracantes" de "ca ne marche pas".
//  Les compteurs sont cumulatifs depuis le demarrage du serveur.
// ---------------------------------------------------------------------------
[BaseContainerProps()]
class FFRX_TracerCommand : ScrServerCommand
{
	override string GetKeyword() { return "tracante"; }
	override bool IsServerSide() { return true; }
	override int RequiredChatPermission() { return EPlayerRole.ADMINISTRATOR; }
	override int RequiredRCONPermission() { return ERCONPermissions.PERMISSIONS_ADMIN; }

	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId) { return Handle(); }
	override ref ScrServerCmdResult OnRCONExecution(array<string> argv)                     { return Handle(); }
	override ref ScrServerCmdResult OnChatClientExecution(array<string> argv, int playerId) { return ScrServerCmdResult("", EServerCmdResultType.OK); }
	override ref ScrServerCmdResult OnUpdate()                                              { return ScrServerCmdResult("", EServerCmdResultType.OK); }

	protected ScrServerCmdResult Handle()
	{
		string line = FFRX_TracerWatch.Render();
		Print(line, LogLevel.NORMAL);
		return ScrServerCmdResult(line, EServerCmdResultType.OK);
	}
}
