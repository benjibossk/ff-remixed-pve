// FF - REMIXED - PVE
// LE CORPS D'UN JOUEUR DECONNECTE RESTE, ET IL EST INCONSCIENT.
//
// ======================================================================================
//  L'INTENTION (Benji, 2026-09-23)
// ======================================================================================
// "Un joueur qui deco, on met son perso inconscient comme ca ses collegues peuvent quand
// meme le deplacer, et quand il se reco il se reco porte."
//
// Aujourd'hui, un joueur qui decroche en plein assaut disparait purement et simplement :
// l'escouade perd un homme sans corps a evacuer, sans decision a prendre. Avec ce
// changement, sa deconnexion devient une situation a gerer -- on le traine a couvert, ou on
// l'abandonne.
//
// ======================================================================================
//  CE QUI EXISTAIT DEJA (et qu'on ne refait pas)
// ======================================================================================
// Le JEU DE BASE garde deja le corps dans certains cas. `SCR_BaseGameMode.OnPlayerDisconnected`
// demande a `SCR_ReconnectComponent.HandlePlayerDisconnect()` s'il faut conserver l'entite ;
// si oui, il saute le `DeleteRplEntity` et le joueur la retrouve en revenant.
//
// ⚠️ MAIS C'EST TRES RESTREINT, et ca explique pourquoi ca ne s'est jamais vu :
//
//   1. UNIQUEMENT les coupures RESEAU. `HandlePlayerDisconnect` commence par
//      `if (group != RplKickCauseGroup.REPLICATION) return false;` -- un joueur qui quitte
//      proprement par le menu voit son corps supprime. Seuls les crashs et les pertes de
//      connexion conservent le personnage.
//
//   2. IL FAUT UN DELAI DE RESERVATION. `m_iReconnectTime` vaut 120 s par defaut, MAIS il
//      est ecrase par `config.operating.slotReservationTimeout` des que le serveur tourne
//      avec une config -- et cette cle est ABSENTE de notre config.json (verifie le
//      2026-09-23). Elle vaut donc probablement 0, ce qui invalide immediatement la reserve.
//      >> A REGLER COTE SERVEUR : ajouter "slotReservationTimeout" dans le bloc "operating"
//      >> du config.json. Sans ca, tout ce fichier ne sert a rien.
//
//   3. Et surtout : le corps conserve garde son ETAT. Un joueur qui crashe debout reste
//      debout, fige. `SCR_BaseGameMode` se contente d'un `SetMovement(0, ...)`.
//
// ======================================================================================
//  CE QU'ON AJOUTE : L'INCONSCIENCE
// ======================================================================================
// Le point 3 est bloquant pour l'intention de Benji, parce qu'ACE Carry EXIGE que la cible
// soit inconsciente : `ACE_Carrying_CanCarryOrDragCasualty` refuse si
// `GetLifeState() != ECharacterLifeState.INCAPACITATED`. Un corps fige debout n'est donc ni
// portable ni trainable -- c'est un mannequin au milieu du terrain.
//
// On force donc l'incapacitation a la deconnexion, APRES avoir laisse le jeu de base faire
// son travail. Si le corps a ete supprime (deconnexion propre, delai expire), il n'y a rien
// a faire et on ne fait rien.
//
// EFFET DE BORD HEUREUX SUR LE BEGAIEMENT. Benji signale qu'un joueur qui se reconnecte
// pendant qu'il est porte "begayait". Explication probable : le porte est place dans un
// COMPARTIMENT (ACE Carry passe par `AnimateWithHelperCompartment`, comme un siege de
// vehicule). Un personnage CONSCIENT qui reprend le controle dans un compartiment se met a
// lutter contre lui : ses entrees de mouvement contredisent l'ancrage. En le rendant
// inconscient, il n'y a plus d'entree a opposer -- le conflit disparait a la source.
// ⚠️ C'est un raisonnement, pas une mesure : a confirmer en jeu.
//
// ======================================================================================
//  CE QU'ON NE FAIT PAS
// ======================================================================================
// On ne PROLONGE pas artificiellement la conservation aux deconnexions volontaires. Un
// joueur qui quitte proprement ne doit pas laisser un corps exploitable : ce serait la porte
// ouverte au "je me deconnecte pour ne pas mourir", et a des corps fantomes accumules sur la
// carte. Le jeu de base a raison sur ce point, on le laisse decider.
//
// Serveur uniquement. Chaines ASCII (le dedie compile en strict).

modded class SCR_BaseGameMode
{
	//------------------------------------------------------------------------------------------------
	override protected void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		// On capture l'entite AVANT : apres `super`, le joueur n'est plus associe a rien, et
		// `GetPlayerControlledEntity` ne rendrait plus le corps meme s'il existe encore.
		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		super.OnPlayerDisconnected(playerId, cause, timeout);

		if (!Replication.IsServer())
			return;

		// Corps supprime par le jeu de base (deconnexion propre, reserve refusee ou expiree)
		// -> rien a faire.
		if (!body || body.IsDeleted())
			return;

		FFRX_KnockOut(body, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Met le corps conserve a l'etat INCONSCIENT, pour qu'il devienne portable.
	protected void FFRX_KnockOut(IEntity body, int playerId)
	{
		SCR_ChimeraCharacter chr = SCR_ChimeraCharacter.Cast(body);
		if (!chr)
			return;

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(chr.GetCharacterController());
		if (!cc)
			return;

		// Deja mort ou deja inconscient : on n'y touche pas. Reveiller un mort ou re-assommer
		// un blesse fausserait son etat medical.
		ECharacterLifeState state = cc.GetLifeState();
		if (state == ECharacterLifeState.DEAD || state == ECharacterLifeState.INCAPACITATED)
			return;

		cc.SetUnconscious(true);

		Print(string.Format("[FFRX][Deco] Joueur %1 deconnecte : corps conserve et mis inconscient (portable).",
			playerId), LogLevel.NORMAL);
	}
}
