class MCD_GreetRegistry
{
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : Enfusion les hisse tous dans
	// UNE fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko
	// deborde en "Too many instructions per function" sur des fichiers innocents. Un
	// `static = new ...` coute ~6 unites sur les ~1000 disponibles pour l'ensemble des
	// mods (les `const` sont gratuits). On construit donc a la premiere utilisation.
	// Cf. memoire `enfusion-script-compile-ceiling`.
	private static ref map<EntityID, bool> s_mGreeted;

	private static map<EntityID, bool> Greeted()
	{
		if (!s_mGreeted)
			s_mGreeted = new map<EntityID, bool>();

		return s_mGreeted;
	}

	// Réaction au salut DÉJÀ tirée pour ce civil (y compris un résultat neutre).
	// NON réinitialisé par Reset() → persiste entre conversations, ce qui empêche
	// de re-tirer le dé en spammant l'option "saluer".
	private static ref map<EntityID, bool> s_mGreetRolled;

	private static map<EntityID, bool> Rolled()
	{
		if (!s_mGreetRolled)
			s_mGreetRolled = new map<EntityID, bool>();

		return s_mGreetRolled;
	}

	static bool IsGreeted(IEntity civEntity)
	{
		if (!civEntity)
			return false;
		return Greeted().Contains(civEntity.GetID()) && Greeted()[civEntity.GetID()];
	}

	static void SetGreeted(IEntity civEntity, bool value)
	{
		if (!civEntity)
			return;
		Greeted()[civEntity.GetID()] = value;
	}

	static void Reset(IEntity civEntity)
	{
		if (!civEntity)
			return;
		// On ne touche PAS à Rolled() : la réaction reste figée pour ce civil.
		Greeted().Remove(civEntity.GetID());
	}

	static bool HasRolledGreet(IEntity civEntity)
	{
		if (!civEntity)
			return false;
		return Rolled().Contains(civEntity.GetID()) && Rolled()[civEntity.GetID()];
	}

	static void SetRolledGreet(IEntity civEntity)
	{
		if (!civEntity)
			return;
		Rolled()[civEntity.GetID()] = true;
	}
}
