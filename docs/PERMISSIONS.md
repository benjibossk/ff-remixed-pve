# Permissions & credits — settle these BEFORE going public

> This file exists because a public repository is not only a technical question.
> REMIXED **extends** other people's work. Some authors have given their consent,
> others have never been contacted. While a line is ⬜, do not publish what it covers.

## Why this matters

Freedom Fighters ships under a **modified APL-ND licence** — the *ND* stands for
**No Derivatives**. Extending a mod through `modded class` and redistributing the result
is not automatically allowed: it has to be asked for.

There are two kinds of link, and they do not commit us equally:

- **Plain dependency** — the mod is declared in `addon.gproj` and we use its prefabs or
  items. None of its code is reused.
- **Code extension** — we write `modded class` against its classes, so we rely on its
  internal architecture. This is where consent genuinely matters.

**No third-party mod is redistributed here.** They all remain Workshop dependencies that
each host downloads themselves. What this repository publishes is *our* code, which
references their classes.

---

## Consent obtained

| Mod | Author | Scope | Status |
|---|---|---|---|
| **Freedom Fighters** (`CAFEBEEFF0CACC1A`) | Johnny Kerner | 42 `modded class` on `JWK_*`, plus 18 derived civilian-dialogue files | ✅ **Granted** (to Benji, Sept 2026). Credit in README. |

⚠️ FF is by far the most committing case in this project: nearly all of REMIXED builds on
its architecture. The 18 `JWK_MCD_*` / `JWK_customconvo*` files under
`Scripts/Game/FFRX/Civilian/Conversation/` are **derived from FF's code**, not merely
inspired by it — one of them still carries FF's own licence header.

---

## Still to ask

| Mod | Author | What we do with it | Status |
|---|---|---|---|
| **FF - Reoccupation** (`69A47272BCF14AD5`) | Erdfuchs | 3 `modded class` on `FFML_*` (martial-law HUD silenced) + 1 `FFRO_*` | ⬜ in contact on Discord, never explicitly asked |
| **FF - Civilian Ownership** (`58CD60EC5CFA455A`) | Erdfuchs | 4 `modded class` on `FFCO_*` — we **hide its UI** (door actions, theft HUD) | ⬜ worth asking: we disable features of his mod |
| **DARC** (`631EE12D448D7FCC`, `5ED0FAC84A48D018`) | darc | 6 `modded class` on `SDRC_*` (no cash reward, no mission icons, convoy = supply raid) | ⬜ never contacted |
| **RealisticCombatDrones / JamAiDrones** | Salami._. | 1 `modded class` on `SAL_DroneJammerComponent` (battery, situational jamming) | ⬜ never contacted |
| **AIUsingStingers / WCS** (`68B2E735099BA541`) | — | 3 `modded class` on `WCS_*` (debug log silencing, SAM specialists) | ⬜ never contacted |
| **GameMasterFX** (`5994AD5A9F33BE57`) | — | 1 `modded class` on `GMFX_MinePressureTriggerComponent` (faction immunity on our IEDs) | ⬜ never contacted |
| **Fleet Arma Reforger Plugin** (`65A4BD29CD32109E`) | — | 2 `modded class` on `Flt_*` (XP, livemap signal quality) | ⬜ to clarify |
| **AMF-FANTASSIN / AMF-VEHICULES** | AMF | Prefabs, rank patches, uniforms, French vehicles. **No `modded class`** | ⬜ plain dependency, but we redistribute prefab overrides |
| **TacticalFlava** (`5D550926D43F1409`) | — | Sophie binoculars derived from its SOFLAM; guided shells in the arsenal | ⬜ worth asking: we derive one of its prefabs |
| **IED Emporium 2.0** (`5DD55EE55380FA5C`) | P34NUTZ | IED prefabs used as-is (scattered IEDs, trapped wrecks) | ⬜ plain dependency |
| **ACE Explosives / ACE Carrying** | ACE team | Plain dependency + public calls (`ACE_Carrying_DragCasualty`) | ⬜ ACE has its own licence, to check |
| **Anizay** (`6266DCA9193C705E`) | — | **Prefab overrides redistributed** (houses, antennas) | ⬜ worth asking: we redistribute modified content |
| **CRX Enfusion A.I.** | ATiM- | **No code reused** — behaviours rewritten against the vanilla API, credited in `CREDITS.md` | ✅ nothing to ask, keep the credit |

### The website — a separate and sharper case

| Project | Author | What we do with it | Status |
|---|---|---|---|
| **[GTG Live Map](https://github.com/SebastianUnterscheutz/GTG-livemap)** | Sebastian Unterscheutz / German Tactical Group | We run a fork. 22 new Go handlers and 17 new pages are ours; **16 of their files are patched** by us | ⬜ **never contacted — ask before anything else** |

Sharper than the mod dependencies for one reason: **their repository has no LICENSE
file.** No licence means all rights reserved by default — there is no permission on
record to redistribute their code at all, not even with credit.

So `web/` is built as an **overlay**: our own files shipped in full, their files
shipped only as diffs, and `apply.sh` clones their repository from source. We
redistribute none of their code. That is why the layout is what it is, and why it has
to stay that way — the moment someone copies one of their files into `web/overlay/`
to dodge a patch conflict, we have started republishing their work.

Two of our patches fix real bugs in their code: a seeded API key written as plaintext
into the bcrypt-hash column, so it could never authenticate; and a
`docker-compose.yaml` with no database, no Redis and no published port. **Offering
those upstream is the natural way to open the conversation** — a contribution before
a request.

What to ask for, in order:

1. That they add a licence (MIT would settle everything at once). This helps all of
   their users, not only us.
2. Failing that, explicit permission to vendor their code here — which would let us
   delete `patches/` and reduce installation to a single clone.

Until one of those lands, the overlay is the correct arrangement, and it is
publishable exactly as it stands.

---

## Template for asking

> Hi — I maintain **FF - REMIXED - PVE**, a PVE guerrilla scenario built on Freedom Fighters.
> It depends on your mod **<name>** and extends it with `modded class` on `<classes>` to
> `<what it does>`. I'd like to publish the scenario's own source on a public repository so
> other servers can host it.
>
> Your mod is **not redistributed** — it stays a Workshop dependency that hosts download
> themselves. What would be published is only our code, which references your classes.
>
> Are you OK with that? Happy to add any credit or wording you'd prefer.

---

## Current status

**This repository is public as of 2026-09-25**, with several ⬜ lines above still
unresolved. That was a deliberate call by the author, not an oversight. What it means
in practice:

- **The requests above are still owed.** Going public first does not cancel them; it
  makes sending them more urgent, not less.
- **Credit is in place for everyone**, resolved or not — see `CREDITS.md`.
- **Any author who objects gets their part removed, promptly and without argument.**
  Open an issue or message the maintainer. That is the commitment attached to having
  published early.

### What is exposed, precisely

Being accurate about this matters more than reassuring wording:

| | |
|---|---|
| Third-party mods | **Not redistributed.** Workshop dependencies, downloaded by each host |
| GTG Live Map source | **Not redistributed** as files — but `web/patches/*.patch` carry **context lines**, i.e. short literal excerpts of their source around each change |
| Anizay / AMF prefab overrides | **Redistributed as modified content.** These are real derivative files, and the most committing item on this page after Freedom Fighters itself |
| Freedom Fighters | 42 `modded class` plus 18 derived dialogue files — ✅ permission granted by Johnny Kerner |

### If you fork this

The permissions on this page were granted to **this** project and do not travel. In
particular, Johnny Kerner's consent for the Freedom Fighters modifications covers
Benji's work, not yours. Ask him yourself.
