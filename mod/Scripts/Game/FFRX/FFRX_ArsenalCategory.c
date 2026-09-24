// FF - REMIXED - PVE
// Category classification for the arsenal / shop menu.
//
// Every arsenal item carries, in its faction ENTITY CATALOG (EEntityCatalogType.ITEM),
// an SCR_ArsenalItem catalog-data whose GetItemType() returns an SCR_EArsenalItemType
// (RIFLE, PISTOL, SNIPER_RIFLE, MACHINE_GUN, ROCKET_LAUNCHER, HEADWEAR, VEST_AND_WAIST,
// EXPLOSIVES, HEAL, ...). We read it per prefab (cached) so the shop UI can filter the
// grid by category. See FFRX_ShopCategories.c for the buttons + the filter hook.

class FFRX_CategoryDef
{
	string m_sLabel;   // button label (ASCII — in-script string, dedicated-safe)
	int m_iMask;       // OR of SCR_EArsenalItemType flags this button shows (0 = "all")

	void FFRX_CategoryDef(string label, int mask)
	{
		m_sLabel = label;
		m_iMask = mask;
	}
}

class FFRX_ArsenalCategory
{
	// prefab (ResourceName as string) -> SCR_EArsenalItemType value (int). 0 = unknown.
	// ⚠️ Pas d'initialiseur immediat sur un champ statique : ils sont hisses dans UNE
	// fonction d'init partagee par vanilla et TOUS les mods, dont le buffer de 64 Ko deborde
	// en "Too many instructions per function" sur des fichiers innocents (~6 unites par
	// static, ~1000 pour l'ensemble des mods ; les `const` sont gratuits).
	// Cf. memoire `enfusion-script-compile-ceiling`.
	protected static ref map<string, int> s_mCache;

	protected static map<string, int> CacheMap()
	{
		if (!s_mCache)
			s_mCache = new map<string, int>();

		return s_mCache;
	}

	// The category buttons, in display order. "Tout" (mask 0) shows everything.
	static array<ref FFRX_CategoryDef> GetCategories()
	{
		array<ref FFRX_CategoryDef> c = {};
		c.Insert(new FFRX_CategoryDef("Tout",          0));
		c.Insert(new FFRX_CategoryDef("Fusils",        SCR_EArsenalItemType.RIFLE));
		c.Insert(new FFRX_CategoryDef("Precision",     SCR_EArsenalItemType.SNIPER_RIFLE));
		c.Insert(new FFRX_CategoryDef("Mitrailleuses", SCR_EArsenalItemType.MACHINE_GUN));
		c.Insert(new FFRX_CategoryDef("Roquettes",     SCR_EArsenalItemType.ROCKET_LAUNCHER));
		c.Insert(new FFRX_CategoryDef("Pistolets",     SCR_EArsenalItemType.PISTOL));
		c.Insert(new FFRX_CategoryDef("Grenades",      SCR_EArsenalItemType.LETHAL_THROWABLE | SCR_EArsenalItemType.NON_LETHAL_THROWABLE));
		c.Insert(new FFRX_CategoryDef("Explosifs",     SCR_EArsenalItemType.EXPLOSIVES));
		c.Insert(new FFRX_CategoryDef("Attachements",  SCR_EArsenalItemType.WEAPON_ATTACHMENT));
		c.Insert(new FFRX_CategoryDef("Casques",       SCR_EArsenalItemType.HEADWEAR));
		c.Insert(new FFRX_CategoryDef("Gilets",        SCR_EArsenalItemType.VEST_AND_WAIST));
		c.Insert(new FFRX_CategoryDef("Torse",         SCR_EArsenalItemType.TORSO));
		c.Insert(new FFRX_CategoryDef("Jambes",        SCR_EArsenalItemType.LEGS));
		c.Insert(new FFRX_CategoryDef("Chaussures",    SCR_EArsenalItemType.FOOTWEAR));
		c.Insert(new FFRX_CategoryDef("Sacs",          SCR_EArsenalItemType.BACKPACK | SCR_EArsenalItemType.RADIO_BACKPACK));
		c.Insert(new FFRX_CategoryDef("Soins",         SCR_EArsenalItemType.HEAL));
		c.Insert(new FFRX_CategoryDef("Equipement",    SCR_EArsenalItemType.EQUIPMENT));
		c.Insert(new FFRX_CategoryDef("Mortiers",      SCR_EArsenalItemType.MORTARS));
		return c;
	}

	// True if an item passes the selected category mask (0 = show all).
	static bool Matches(ResourceName prefab, int mask)
	{
		if (mask == 0)
			return true;

		int t = GetItemType(prefab);
		return (t & mask) != 0;
	}

	// SCR_EArsenalItemType value of an item (cached). 0 if not an arsenal item / unknown.
	static int GetItemType(ResourceName prefab)
	{
		string key = prefab;
		int cached;
		if (CacheMap().Find(key, cached))
			return cached;

		int result = 0;

		SCR_EntityCatalogManagerComponent mgr = SCR_EntityCatalogManagerComponent.GetInstance();
		SCR_Faction fac = GetPlayerFaction();
		if (mgr && fac)
		{
			SCR_EntityCatalogEntry entry = mgr.GetEntryWithPrefabFromAnyCatalog(EEntityCatalogType.ITEM, prefab, fac);
			if (entry)
			{
				SCR_ArsenalItem ai = SCR_ArsenalItem.Cast(entry.GetEntityDataOfType(SCR_ArsenalItem));
				if (ai)
					result = ai.GetItemType();
			}
		}

		CacheMap().Set(key, result);
		return result;
	}

	// Local player's faction (the shop is for the player's faction catalog).
	protected static SCR_Faction GetPlayerFaction()
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return null;

		IEntity ent = pc.GetControlledEntity();
		if (!ent)
			return null;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return null;

		return SCR_Faction.Cast(fac.GetAffiliatedFaction());
	}
}
