// FF - REMIXED - PVE -- guard contre un JWK.GetGameConfig() null.
//
// SYMPTOME (en boucle, sur CHAQUE agglomeration, des l'ouverture du monde) :
//   NULL pointer to instance -- JWK_FactionManager::GetPlayerFactionKey
//   JWK_FactionManager.c:349            <- FF core
//   ... GetRoleByFactionKey:440 / GetRole:410
//   FFRO_FIATownReinforcement.c:426 FFRO_IsFIAControlled / :142 OnPostInit  <- Reoccupation
//
// CAUSE : le null n'est PAS le faction manager (le message trompe), c'est
// `JWK.GetGameConfig()` -- la config du game mode, qui n'existe pas encore quand les
// composants de ville font leur OnPostInit. FF derefence ce getter **16 fois sans
// aucune garde** dans JWK_FactionManager.c (l.305-386, les 4 familles de cles :
// enemy / supporting / player / ambient). Reoccupation 5.x l'atteint depuis
// OnPostInit -> exception.
//
// C'est EXACTEMENT le meme schema que FFRX_BattleSubjectGuard.c (IsBattleActive_S) :
// Reoccupation appelle une API FF trop tot, et FF ne se protege pas.
//
// C'est aussi ce qui faisait passer le scenario Anizay de TernaryOperator pour
// "casse" : le bug n'etait pas ses villes, mais cet appel premature.
//
// FIX : garder les 4 getters de cles a la source. Les 12 autres deferencements
// passent tous par eux ou par GetFactionByKey(...), donc les 4 suffisent.
// Retourner "" est la bonne reponse : GetRoleByFactionKey traite deja la chaine vide
// (-> JWK_EFactionRole.NONE), donc aucun comportement n'est invente. Une fois le game
// mode initialise, super() reprend la main et rien ne change.
//
// NOTE: ASCII only (Enforce compiler desyncs on UTF-8 accents).
modded class JWK_FactionManager
{
	override string GetPlayerFactionKey()
	{
		if (!JWK.GetGameConfig())
			return ""; // trop tot : le game mode n'est pas encore initialise
		return super.GetPlayerFactionKey();
	}

	override string GetEnemyFactionKey()
	{
		if (!JWK.GetGameConfig())
			return "";
		return super.GetEnemyFactionKey();
	}

	override string GetSupportingFactionKey()
	{
		if (!JWK.GetGameConfig())
			return "";
		return super.GetSupportingFactionKey();
	}

	override string GetAmbientFactionKey()
	{
		if (!JWK.GetGameConfig())
			return "";
		return super.GetAmbientFactionKey();
	}
}
