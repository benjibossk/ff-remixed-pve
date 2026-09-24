// FF - REMIXED - PVE
// Fleet bridge for LIVE group management from the web.
//
// REQUIRES the "Fleet Arma Reforger Plugin" (GUID 65A4BD29CD32109E) as an addon
// dependency -- it provides Flt_MarkerBridge. Fleet PULLs orders from the web on
// GET /api/commands and applies them via Flt_MarkerBridge.ApplyCommandsJson. We
// piggyback on that same JSON to handle group_create / group_edit / group_delete
// before letting Fleet process its own marker/objective commands.
//
// Web command JSON (one entry in the shared {"commands":[...]}):
//   { "type":"group_edit", "id":1000, "name":"ALPHA - Cavalerie",
//     "desc":"...", "max":8, "freq":39.0, "role":-1, "priv":-1, "flag":-1 }
//   { "type":"group_create", "faction":"FIA", "name":"...", "desc":"...",
//     "max":8, "freq":41.0 }
//   { "type":"group_delete", "id":1005 }
//   { "type":"group_whitelist", "id":1011, "uids":"uid1;uid2;uid3" }
//        -> replaces that group's access whitelist (only those UIDs may join).
//           uids "" = clear the whitelist (group open to all). UIDs are the same
//           IDs as the loadout owner / Discord #link on the site.
// Sentinels for "leave unchanged" on edit: name/desc "", max/freq 0, role/priv/flag -1.

class FFRX_GroupCmd : JsonApiStruct
{
	string type;
	int    id;
	string faction;
	string name;
	string desc;
	int    max;
	float  freq;
	int    role;
	int    priv;
	int    flag;
	string uids;   // semicolon-separated UID list for group_whitelist

	void FFRX_GroupCmd()
	{
		RegV("type"); RegV("id"); RegV("faction");
		RegV("name"); RegV("desc"); RegV("max"); RegV("freq");
		RegV("role"); RegV("priv"); RegV("flag"); RegV("uids");
	}
}

class FFRX_GroupCmdList : JsonApiStruct
{
	ref array<ref FFRX_GroupCmd> commands;
	void FFRX_GroupCmdList() { RegV("commands"); }
}

modded class Flt_MarkerBridge
{
	override void ApplyCommandsJson(string json)
	{
		FFRX_HandleGroupCommands(json);
		super.ApplyCommandsJson(json); // let Fleet handle markers/objectives/etc.
	}

	protected void FFRX_HandleGroupCommands(string json)
	{
		if (!Replication.IsServer() || json == "") return;

		FFRX_GroupCmdList list = new FFRX_GroupCmdList();
		list.ExpandFromRAW(json);
		if (!list.commands) return;

		foreach (FFRX_GroupCmd c : list.commands) {
			if (!c) continue;

			if (c.type == "group_edit")
				FFRX_GroupsManager.EditGroup(c.id, c.name, c.desc, c.max, c.freq, c.role, c.priv, c.flag);
			else if (c.type == "group_create")
				FFRX_GroupsManager.CreateGroupLive(c.faction, c.name, c.desc, c.max, c.freq);
			else if (c.type == "group_delete")
				FFRX_GroupsManager.DeleteGroupById(c.id);
			else if (c.type == "group_whitelist")
				FFRX_GroupsManager.SetGroupWhitelistCsv(c.id, c.uids);
			// Remise a zero du recensement depuis la page /recensement.
			//
			// Vider la table cote SITE ne suffirait pas : le compteur du jeu est CUMULATIF
			// depuis le demarrage du serveur, et il reecrase l'instantane du site a chaque
			// rapport. Sans remettre le compteur du jeu a zero, les anciens chiffres
			// reviendraient 10 minutes plus tard. C'est donc le jeu qui fait autorite.
			else if (c.type == "census_reset")
				FFRX_SpawnCensus.ResetFromWeb();
		}
	}
}
