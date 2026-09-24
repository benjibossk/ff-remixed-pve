# How REMIXED works, and why

> This document maps the **design intent** onto the **code that implements it**.
> `GAME_DESIGN.md` states what we want; this file explains how each system delivers it,
> and — more usefully — *why it is built the way it is* rather than the obvious way.
>
> Every system named here lives in `Scripts/Game/FFRX/<name>.c`.

---

## The single idea

**Information is earned, never given.**

Arma hands players a map with everything on it. Freedom Fighters already pushes back on
that; REMIXED takes it to its conclusion. Nothing is displayed because it exists — it is
displayed because someone went and got it, and it can be wrong, stale, or lost.

Every pillar below is a consequence of that one sentence.

---

## Pillar 1 — Fog of war and intelligence

### The map starts empty

`EmptyMap`, `MapAccess`, `MarkerPermissions`, `SilenceNotifications`

A non-admin opens the map and sees nothing: no POIs, no territory, no roads, no task
markers. Reoccupation's free intel notifications are intercepted at the single dispatch
choke point rather than disabled feature by feature — disabling its settings would delete
the *content*, not just the text.

Marker editing follows military hierarchy: your own markers, a squad leader's over their
squad, command over everything.

### Intel has a cost, a source, and a reliability

`RadioIntel`, `IntelSystem`, `EnemyIntelDrop`, `CacheNote`

| Source | Precision | Lies |
|---|---|---|
| Civilian tip | ±150 m | ~20 % false leads |
| Officer papers (off a body) | ±25 m | never |
| Radio dispatch | exact | never — but must be collected **in person** |

Reoccupation produces genuinely good reports, and their quality already scales with radio
tower coverage. What did not fit was the *delivery* — a full-screen card pushed to every
player for free. We keep the content and change the access: the report is held pending
until a player travels to a radio site they hold and stays near it for a few seconds.
An unread report expires after 30 minutes. **Intel perishes.**

> **Why presence and not a user action:** Enfusion has no API to graft a UserAction onto an
> existing entity. Doing it would mean overriding FF's radio-site prefab — fragile, and it
> drags in the unbaked-entity-ID problem. Presence + dwell gives the same "go there and
> work for it" gameplay, touches no prefab, and survives FF updates.

### Signal quality ties the whole pillar together

`Beacon`, `BeaconActions`, `RadioRelay`, `RadioSites`, `LivemapSignal`

A **GPS beacon** transmits a position to the squad. But:

- it runs on the **same drone batteries** as jammers and FPVs — one logistics currency;
- its refresh rate and precision depend on **distance to a radio site you hold**;
- out of range, the marker freezes on its **last known position** rather than disappearing.

The same ladder drives the **live map**: far from a relay, a soldier's position on the
website goes stale. And a **long-range radio backpack** stretches the thresholds by 1.54 —
which is not an invented number, it is the real ratio between a chest radio (1 300 m) and a
pack radio (2 000 m) in the base game.

> **The point:** capturing a radio tower improves beacons *and* the live map *and* unlocks
> Reoccupation's dispatches. Several mechanics converge on one piece of terrain. That is the
> only place in the mod where that happens, and it is deliberate.

> **A trap worth knowing:** Anizay ships with **no radio site at all**. Every one of these
> systems was silently inert — `NearestFriendlySiteDistance()` returned −1 forever. We now
> place FF radio-site controllers next to the map's existing antennas at startup, and turn
> those antennas into real FF radio towers via prefab override.

---

## Pillar 2 — A vulnerable economy

### No cash

`DarcIntegration`

DARC missions normally pay money and paint an icon on the map. Both are removed. A defeated
convoy truck is instead **filled with supplies to haul home** — the reward is logistics, not
currency, and it has to be driven back.

### Supply has a direction and can be attacked

`MarineResupply`, `EnemyAirResupply`, `ProductionTracker`, `Procurement`

Supplies arrive by sea; the enemy resupplies by helicopter, which you can intercept.
Factories produce. Vehicles are **requested**, not taken: a non-command player creates a
pending request that a command-squad member validates.

> **Why a request and not a price:** a price is a wall you either can or cannot pay alone.
> A request is a conversation with another player. It makes command a role rather than a
> label.

---

## Pillar 3 — Forced cooperation

`Groups`, `GroupsFleet`, `RoleBonus`, `RankPatch`, `BuildCommand`, `BuildDirect`

Squads carry flags (`etatMajor`, `genie`, `medic`) defined in `ffrx-groups.json`, not
hardcoded — a server can have two engineer squads or none.

| Role | What it alone can do |
|---|---|
| Command (KILO) | open the build menu, validate procurement |
| Engineer (ECHO) | open the build menu, build 2.5× faster |
| Medic (JULIETT) | better healing, bandages last longer |

**Everyone else can still shovel on a construction site someone already placed.** That
distinction is the whole point: the *decision* of what to build and where belongs to two
squads; the *labour* belongs to everyone.

Rank insignia are applied automatically from the real persisted rank, and rank patches were
removed from the arsenal — otherwise a fresh recruit wears colonel's stripes and rank means
nothing.

---

## Pillar 4 — Asymmetric warfare

The enemy cannot beat a well-equipped squad in a stand-up fight, so it does not try.

### Threats that follow the player

`SuicideBomber`, `SuicideVest`, `BoobyTrapCars`, `VBIED`, `DisguisedSpies`

Bombers, booby-trapped cars, disguised spies, and **rolling VBIEDs** — a civilian car driven
at you that you can see coming, identify, and stop by killing the driver.

> **Why the VBIED matters:** a static booby-trapped car is a punishment — the player suffers
> without seeing anything. A car that *drives at you* is a readable threat you can answer.
> Same mod, opposite feel.

### Threats that belong to a place

`IEDScatter`, `TrappedWrecks`, `MinePlacement`

The systems above are all *reactive* — they appear around the player. Eventually he
understands that danger follows him, so an empty place is safe.

So these do the opposite. IEDs are placed **in advance** on the map's own landmarks
(FF's level-design anchors plus enemy checkpoints), and a minority of the map's **scenery
wrecks** carry a charge. The danger becomes a property of the *location*, and the tension
exists even when nothing happens.

> **Dosage is the mechanic.** Only ~18 % of found wrecks are trapped. If every wreck
> exploded, that would not create doubt — it would create a rule to learn, and players would
> simply stop searching.

All of these only trigger under the resistance side. The enemy AI walks over its own
devices, or garrisons would destroy themselves and the map would empty itself out.

---

## Pillar 5 — An enemy that adapts

`AdaptiveThreat`, `SquadTier`, `SpecialistWeights`, `AIDifficulty`, `NightVision`,
`HealDiscipline`, `CasualtyEvac`, `AntiCamping`, `Tracer`, `VehicleSmokeScreen`

- **Adaptive threat** fields anti-tank and SAM specialists in the zones where players
  actually hurt the enemy — measured in armour kills and helicopter flight time.
- **Squad tiers** vary quality, so not every patrol is the same patrol.
- **Night vision** is modelled by script, because the engine models neither darkness nor NV
  optics for AI perception.
- **Medics drag casualties to cover** before healing, instead of standing up in a firefight.
- **Tracers cost something**: the engine ignores them entirely, so firing tracer gives
  enemies with line of sight a perception bonus — doubled at night.
- **Armour answers guided missiles** with a smoke screen, so an AT gunner must reposition
  instead of firing once from safety.

> **Why specialist weighting exists at all:** FF picks infantry groups with a flat
> `GetRandomIndex()` over *distinct* entries — repeating a prefab in the config to make it
> more common does nothing. Squad frequency was therefore not tunable at all. We override the
> composition generator so each specialist team has a real percentage.

---

## Pillar 6 — A living, persistent civilian population

`CivRoster`, `CivIdentityRegistry`, `CounterEspionage`, `CivilianTraffic`, conversation nodes

Civilians have persistent identities and a **trust** value that survives restarts and drives
what they tell you. Treating them well produces intelligence; frisking and killing produces
leaks to the enemy.

They stay **anonymous on screen** — a deliberate call. The systems track identity; the player
experiences people, not labels.

---

## Cross-cutting engineering rules

These are not design, but they shape everything and cost us the most time to learn.

| Rule | Consequence |
|---|---|
| A client `GameSystem` never boots on a dedicated client | Arm client code from `SCR_PlayerController.OnControlledEntityChanged` |
| A `ScriptedUserAction` gating on client-only state does nothing on a dedicated server | The authority re-validates with an empty registry |
| `modded class` on a class referenced by a `.conf` → `Unknown class`, entry silently dropped | Mod components and controllers, never contexts named in configs |
| An `.et` override at the **same GUID** replaces the file | Any component not re-declared is lost without an error |
| `static x = new ...` costs ~6 units of a **64 KB budget shared by vanilla and every mod** | Build statics lazily in a getter; REMIXED consumes zero |
| `string.Format` silently truncates at ~8 KB | Build large payloads with `+` concatenation |
| No ternary operator in Enforce | Use if/else |

The last three cost a dedicated server that would not boot, a website returning 400 on
valid data, and a full day of debugging respectively. They are in the code comments where
they bite, not only here.

---

## What is deliberately *not* built

- **No cash economy** — supplies and favours only.
- **No mission markers** — coordinates from intel, or nothing.
- **No player property or furnishing** — Civilian Ownership ships as a forced dependency of
  Reoccupation; its UI is hidden (`HideCivilianOwnership`) because REMIXED is a guerrilla
  campaign, not a life sim.
- **No video relay to HQ** — verified twice against the engine: a client can only render
  what is streamed to it. Telemetry and markers instead. See `RECHERCHE_DroneTV_relais.md`.
