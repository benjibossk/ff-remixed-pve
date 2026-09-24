# FF - REMIXED - PVE

A PVE guerrilla campaign for **Arma Reforger**, and everything needed to run it:
the mod, the companion website, and the server configuration.

Built on [Freedom Fighters](https://reforger.armaplatform.com/workshop/CAFEBEEFF0CACC1A)
by Johnny Kerner, played on the **Anizay** desert map — a French army (AMF)
resistance against Middle-Eastern insurgents.

**Workshop GUID:** `69DF0268E97F72A3`

---

## The design in one idea

**Information is earned, never given.**

The map starts empty. No mission markers, no free intel, no cash economy. What you
know, you went and got:

- **Radio dispatches** are collected in person, at a radio site you hold.
- **Civilians** give imprecise tips (±150 m, ~20 % false leads); **officer papers**
  taken off a body are exact.
- **GPS beacons** you plant report to the squad — but precision and refresh rate
  degrade with distance from the towers you control, and they run on the same drone
  batteries as everything else.
- **Enemy convoys** are worth raiding. You only learn where they are through intel.

Everything points the same way: **hold the radio towers and the map opens up.**

Read [`docs/DESIGN.md`](docs/DESIGN.md) for how each system works and *why it is
built that way* — including the traps we fell into.

---

## What is in this repository

| Directory | Contents | Whose code |
|---|---|---|
| **`mod/`** | The Arma Reforger addon — 149 script files, 309 classes | ours |
| **`web/`** | Our additions to the companion website: 22 Go handlers, 17 pages | ours, as an **overlay** over a third-party site |
| **`install/`** | Configuration templates for the dedicated server and the website | ours |
| **`docs/`** | Design, hosting guide, credits, permission status | ours |

### Two things this repository does *not* contain

**No credentials.** Every configuration file ships as `*.example.*`. Copy, fill in,
and the real file is gitignored. If you fork this, that discipline is on you too.

**No third-party code.** Not the ~20 Arma mods it depends on (the Workshop resolves
those), and not the website (`web/` is a patch set over a repository you clone
yourself). See [`web/README.md`](web/README.md) for how that works.

---

## Quick start

### 1. The mod

Every mod listed in `mod/addon.gproj` must be available to your server — the
Workshop downloads them. Notable ones: Freedom Fighters, FF - Reoccupation,
AMF-FANTASSIN, AMF-VEHICULES, DARC, TacticalFlava, ACE, IED Emporium, Anizay.

```bash
cp install/server/config.example.json            /path/to/server/config.json
cp install/server/profile/ffrx-groups.example.json  $profile/ffrx-groups.json
```

Then set, at minimum, `game.passwordAdmin` and `rcon.password` — **the templates
are placeholders, not passwords.**

> ⚠️ **Script budget.** Arma enforces a hard limit on load-time static
> initialisers, *shared across vanilla and every mod combined* — roughly 1 000
> units, where one `static x = new ...` costs about 6. Overflow produces
> `Too many instructions per function` pointing at **innocent vanilla files**, and
> the server refuses to start. REMIXED is written to consume **zero** of that
> budget (every static is lazily initialised), but stacking other heavy script mods
> on top can still break it. See [`docs/HOSTING.md`](docs/HOSTING.md).

### 2. The website (optional)

```bash
cd web && ./apply.sh ~/gtg-livemap
cd ~/gtg-livemap
# fill in config.yaml and the CHANGEME_ passwords in docker-compose.yaml
docker compose up --build -d
```

Without it, the systems that talk to it log a warning and stay dormant. Nothing
else breaks.

Full walkthrough: [`docs/HOSTING.md`](docs/HOSTING.md).

---

## In-game admin commands

Typed in chat. `#help` lists everything, grouped by topic. The ones that matter
while setting up:

| Command | What it tells you |
|---|---|
| `#pelle` | why *you* cannot build right now — tool, zone, available items |
| `#radiosites` | radio sites: total, operable, held by the resistance |
| `#census` | what the enemy actually spawned, with proportions |
| `#menace` | adaptive threat gauges, per zone |

---

## Before you fork or republish

REMIXED **extends other people's work**, mostly through `modded class`. Some
authors have consented, others have never been asked.

[`docs/PERMISSIONS.md`](docs/PERMISSIONS.md) tracks the status of each one, and
says plainly what is settled and what is not. **Read it before making a fork
public.**

Freedom Fighters ships under a modified **APL-ND** licence — *ND* for **No
Derivatives**. Johnny Kerner granted permission for the modifications in *this*
project. That permission does not travel: if you fork, ask him yourself.

Credits: [`docs/CREDITS.md`](docs/CREDITS.md).
