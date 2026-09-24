// Suivi de la "patience" par civil (session). À force de solliciter un civil
// amical (option "Demander de l'aide"), sa patience baisse ; épuisée, il se lasse.
// Clé = EntityID. NON réinitialisé par conversation (persiste dans la session),
// comme le registre "réaction déjà tirée". La persistance durable viendra avec
// le chantier des identités persistantes (voir BACKLOG EPIC A/F).

class MCD_PatienceRegistry
{
	static const float PATIENCE_MAX = 100.0;

	// ⚠️ Pas d'initialiseur immediat sur un champ statique : Enfusion les hisse tous dans
	// UNE fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko
	// deborde en "Too many instructions per function" sur des fichiers innocents. Un
	// `static = new ...` coute ~6 unites sur les ~1000 disponibles pour l'ensemble des
	// mods (les `const` sont gratuits). On construit donc a la premiere utilisation.
	// Cf. memoire `enfusion-script-compile-ceiling`.
	private static ref map<EntityID, float> s_mPatience;

	private static map<EntityID, float> Patience()
	{
		if (!s_mPatience)
			s_mPatience = new map<EntityID, float>();

		return s_mPatience;
	}

	static float Get(IEntity civEntity)
	{
		if (!civEntity)
			return 0.0;
		if (!Patience().Contains(civEntity.GetID()))
			return PATIENCE_MAX;
		return Patience()[civEntity.GetID()];
	}

	static void Set(IEntity civEntity, float value)
	{
		if (!civEntity)
			return;
		Patience()[civEntity.GetID()] = value;
	}

	// Retire "cost" de patience et retourne la valeur restante (bornée à 0).
	static float Consume(IEntity civEntity, float cost)
	{
		float remaining = Get(civEntity) - cost;
		if (remaining < 0.0)
			remaining = 0.0;
		Set(civEntity, remaining);
		return remaining;
	}
}
