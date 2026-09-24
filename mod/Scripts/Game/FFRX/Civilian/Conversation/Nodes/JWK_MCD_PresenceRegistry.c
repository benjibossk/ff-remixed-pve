// FF - More Civilian Discussion
// Anti-spam for the "ask enemy presence" question. A civilian answers it only ONCE
// (frozen per civilian across conversations, same idea as MCD_GreetRegistry's roll
// freeze), so a player can't spam the dialogue wheel to farm answers OR intel tips:
// re-asking gives the refusal without a new roll. Key = civilian EntityID (session).
class MCD_PresenceRegistry
{
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : Enfusion les hisse tous dans
	// UNE fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko
	// deborde en "Too many instructions per function" sur des fichiers innocents. Un
	// `static = new ...` coute ~6 unites sur les ~1000 disponibles pour l'ensemble des
	// mods (les `const` sont gratuits). On construit donc a la premiere utilisation.
	// Cf. memoire `enfusion-script-compile-ceiling`.
	private static ref map<EntityID, bool> s_mAsked;

	private static map<EntityID, bool> Asked()
	{
		if (!s_mAsked)
			s_mAsked = new map<EntityID, bool>();

		return s_mAsked;
	}

	static bool HasAsked(IEntity civEntity)
	{
		if (!civEntity)
			return false;
		return Asked().Contains(civEntity.GetID()) && Asked()[civEntity.GetID()];
	}

	static void SetAsked(IEntity civEntity)
	{
		if (!civEntity)
			return;
		Asked()[civEntity.GetID()] = true;
	}
}
