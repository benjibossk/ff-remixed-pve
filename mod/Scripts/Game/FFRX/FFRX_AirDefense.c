// FF - REMIXED - PVE
// Pillar 6/7 (air + ground denial, Brique 2): enemy AA/AT specialists from the AIUsingStingers
// mod (ready per-faction characters):
//   - Character_USSR_Air     = Igla MANPADS soldier -> shoots down player HELICOPTERS (~2200 m).
//   - Character_USSR_Javelin = Javelin soldier       -> kills player GROUND VEHICLES (~1800 m).
//
// PLACEMENT: instead of a script that drops them on points, these characters are added
// straight into the enemy GROUP COMPOSITIONS (see the modded USSR group prefabs) so FF/DARC
// spawn them naturally in garrisons/patrols. This file only keeps a TEST helper used by the
// #spawnaa / #spawnat dev commands to verify the AI actually locks + fires.
//
// Server only. ASCII in strings.
class FFRX_AirDefense
{
	static const ResourceName AA_PREFAB = "{D5D63723D11CA561}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Air.et";
	static const ResourceName AT_PREFAB = "{FF505580F1633936}Prefabs/Characters/Factions/OPFOR/USSR_Army/Character_USSR_Javelin.et";

	// Spawn one specialist ~120 m from a position, assigned to the FF enemy faction (test only).
	static bool SpawnNear(vector fromPos, ResourceName prefab, bool antiTank)
	{
		if (!Replication.IsServer()) return false;
		if (prefab == string.Empty)
		{
			Print("[FFRX][AA] Prefab specialiste non defini -> spawn ignore.", LogLevel.WARNING);
			return false;
		}

		float ang = JWK.Random.RandFloat01() * Math.PI2;
		vector pos = fromPos;
		pos[0] = fromPos[0] + Math.Cos(ang) * 120;
		pos[2] = fromPos[2] + Math.Sin(ang) * 120;

		string key = FFRX_EnemyKey();
		SCR_AIGroup group = SDRC_AIHelper.SpawnGroup(prefab, pos, key);
		if (!group)
		{
			Print("[FFRX][AA] Echec spawn du specialiste (prefab/faction ?).", LogLevel.WARNING);
			return false;
		}

		string kind = "AA Igla";
		if (antiTank) kind = "AT Javelin";
		Print(string.Format("[FFRX][AA] +++ Specialiste %1 spawn a %2 (test).", kind, pos.ToString()), LogLevel.NORMAL);
		return true;
	}

	protected static string FFRX_EnemyKey()
	{
		JWK_Faction enemy = JWK_Faction.GetByRole(JWK_EFactionRole.ENEMY);
		if (enemy) return enemy.GetKey();
		return "USSR";
	}
}
