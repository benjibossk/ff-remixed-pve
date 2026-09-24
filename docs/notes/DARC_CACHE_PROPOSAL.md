# DARC proposal — cache resolved spawn lists (skip the full prefab re-scan every boot)

> ✅ **POSTÉ : https://github.com/mokdevel/DarcMods/issues/49** (2026-07-23). La version postée utilise l'accesseur corrigé `SDRC_Conf.cacheLists` (static, comme `SDRC_Conf.subDir`) au lieu du `SDRC_Core.GetInstance().GetCoreConfig()` supposé plus bas.


Pour: https://github.com/mokdevel/DarcMods — auteur **mokdevel**.
But: éviter le re-scan complet des dossiers de prefabs à chaque boot (~36s mesuré sur install lourdement moddé), en le refaisant seulement quand la liste des mods change.

---

## Issue text (à poster tel quel)

**Title:** Feature: cache resolved spawn lists to skip the full prefab re-scan on every boot

**Body:**

**Summary**

`SDRC_ListConfig.Populate()` → `DoScan()` re-scans all prefab directories (`SDRC_Resources.GetList` → `ResourceDatabase.SearchResources` + include/exclude filtering) on **every** init / server boot, for every list (enemies, vehicles, loot/weapons…). On a heavily-modded install this is slow — I measured **~36s**, during which the "Creating entities" load screen is stuck. The result is identical every boot unless the loaded mod set changed, and several lists re-scan overlapping folders.

`m_modList` is already gathered in `Populate()` via `SDRC_Misc.GetAddonList`, so we already know the loaded mod set — we can use it as a cache key.

**Proposal — opt-in cache, invalidated when the mod set changes**

- Add `bool m_bCacheLists` to `SDRC_CoreConfig` (default `false` to keep current behaviour).
- Persist a signature of the loaded mod list in each list-config (`string m_sModListHash`).
- In `Populate()`: if caching is on, the stored hash matches the current mod set, **and** the lists already have `items[]` → skip `DoScan()` entirely (only rebuild `factions[]` if empty). Otherwise run the normal scan and store the new hash.
- Add an admin command `#darc rescan` to force a rebuild (mod update without a hash change, or manual refresh).

**Benefit**

Near-instant boots after the first scan — big QoL for testing and faster dedicated restarts — with automatic invalidation so lists never go stale.

---

## Proposed patch (Enforce script)

### 1. `DarcCore/scripts/Game/Conf/SDRC_CoreConfig.c` — add the opt-in flag

```enforce
class SDRC_CoreConfig : SDRC_Config
{
    string author = "darc";
    DC_LogLevel logLevel;
    string subDir;
    // ... existing fields ...
    bool showOnGMMapMissionMarker = true;
    bool m_bCacheLists = false;               // NEW: reuse saved items[] until the mod set changes
    ref array<string> buildingExcludeFilter = {};
    // ... rest unchanged ...
}
```

### 2. `DarcCore/scripts/Game/Helpers/SDRC_ListConfig.c` — signature + skip logic

Add a persisted field to `SDRC_ListConfig`:

```enforce
class SDRC_ListConfig : SDRC_Config
{
    string author = "darc";
    bool m_bPrintList = true;
    bool m_bScanReady = false;
    string m_sModListHash = "";               // NEW: mod set present when items[] were last scanned

    ref array<string> m_modList = {};
    ref array<ref SDRC_List> m_lists = {};
    ref array<ref SDRC_Aka> m_akas = {};
```

Guard the scan in `Populate()` (only the marked block is new):

```enforce
    void Populate(bool fastScan = true, bool printList = true)
    {
        SDRC_Log.Add("[SDRC_ListConfig:Populate] Creating lists..", LogLevel.NORMAL);
        m_bPrintList = printList;

        if (m_modList.IsEmpty())
        {
            array<string> addonList = {};
            SDRC_Misc.GetAddonList(addonList, false);
            foreach (string addon : addonList)
                m_modList.Insert(addon);
        }

        // --- NEW: cache check -------------------------------------------------
        string currentHash = BuildModListHash();
        if (SDRC_Core.GetInstance().GetCoreConfig().m_bCacheLists   // opt-in
            && m_sModListHash == currentHash                        // same mod set
            && HasCachedItems())                                    // items[] already present
        {
            SDRC_Log.Add("[SDRC_ListConfig:Populate] Cache hit (mod set unchanged) -> skipping scan.", LogLevel.NORMAL);
            EnsureFactions();     // factions[] are not persisted -> rebuild if missing
            m_bScanReady = true;
            return;
        }
        m_sModListHash = currentHash;   // will be saved with the config
        // ----------------------------------------------------------------------

        foreach (int idx, SDRC_List list : m_lists)
        {
            bool lastItem = (idx == m_lists.Count() - 1);
            if (fastScan)
                DoScan(list, lastItem);
            else
                GetGame().GetCallqueue().CallLater(DoScan, 2000 + idx * 300, false, list, lastItem);
        }
    }

    // --- NEW helpers ---------------------------------------------------------
    protected string BuildModListHash()
    {
        // Order-independent, cheap signature of the loaded mod set.
        int h = m_modList.Count();
        foreach (string m : m_modList)
            h += m.Hash();
        return h.ToString() + "_" + m_modList.Count().ToString();
    }

    protected bool HasCachedItems()
    {
        foreach (SDRC_List list : m_lists)
            if (!list.items.IsEmpty())
                return true;   // at least one list resolved -> cache is usable
        return false;
    }

    protected void EnsureFactions()
    {
        foreach (SDRC_List list : m_lists)
        {
            if (list.items.Count() == list.factions.Count())
                continue;
            list.factions.Clear();
            foreach (string item : list.items)
                list.factions.Insert(SDRC_Resources.GetResourceFaction(item));
        }
    }
```

> Notes for review:
> - Keeps the current behaviour unless `m_bCacheLists` is enabled.
> - `factions[]` is intentionally not persisted (per the existing comment "This is NOT autofilled"), so `EnsureFactions()` rebuilds it from the cached `items[]` — still far cheaper than a full folder scan.
> - Cache auto-invalidates when a mod is added/removed/updated (mod set → different hash). A `#darc rescan` command (or deleting the JSONs) forces a rebuild if an author ships new prefabs under the same mod version.
> - Persisting `m_sModListHash` needs it to be part of the config JSON (it is, being a plain field on the config class).

---

## Local workaround (in the meantime, no upstream needed)

Set each list's `modDir` to `[]` in the `$profile:/DarcMods/*.json` once the `items[]` are fully populated — `GetList` then scans nothing and the saved `items[]` are used as-is. Re-enable `modDir` + reboot once to rescan after a mod change.
