# Hosting FF - REMIXED - PVE

Start to finish: a dedicated server running the campaign, and optionally the
companion website. Roughly an hour, most of it downloads.

There are two independent halves. **The mod works alone.** Do part 1, play, and come
back for part 2 if you want the live map.

---

## What you need

| | Minimum | Comment |
|---|---|---|
| OS | Windows 10/11 or Linux | scripts here are Windows `.bat`; the Linux commands are given inline |
| Disk | ~25 GB | 11 GB server + ~10 GB mods + saves |
| RAM | 8 GB for 20 players | Reforger is more memory- than CPU-hungry |
| Ports | 2001/UDP, 17777/UDP | forwarded and open. **UDP, not TCP.** |
| Website (optional) | Docker + a domain with TLS | Discord OAuth refuses plain HTTP redirects |

---

# Part 1 — The dedicated server

## 1.1 Install the server binary

Copy `install/server/scripts/*.bat` to an empty folder — say `D:\arma-server\` — and
run:

```
install-server.bat
```

It fetches SteamCMD, then Arma Reforger Server (App ID 1874900, ~11 GB, no Steam
account needed).

On Linux:

```bash
mkdir -p ~/arma-server && cd ~/arma-server
steamcmd +force_install_dir ~/arma-server/server +login anonymous \
         +app_update 1874900 validate +quit
```

> The very first `app_update` on a fresh SteamCMD often fails with *Missing
> configuration*. Run `steamcmd +quit` once to let it self-update, then retry. The
> `.bat` already does this for you.

## 1.2 Write config.json

```
copy install\server\config.example.json  D:\arma-server\config.json
```

Then either edit the JSON directly, or use the small form-based editor:

```
start-panel.bat          →  http://localhost:8080
```

(Needs Python 3. Its interface is in French — the only French left in the project.
If you also host the website locally, note it *also* wants port 8080; close one
before starting the other.)

**The fields that actually matter:**

| Field | Set it to |
|---|---|
| `publicAddress`, `publicPort` | your public IP, and 2001 |
| `game.name` | what players see in the browser |
| `game.passwordAdmin` | **change it.** The template is a placeholder, not a password |
| `rcon.password` | **change it.** Minimum 3 characters, no spaces — the engine rejects shorter with `Param "#/rcon/password" is bellow the minimum limit`. Leave the whole `rcon` block out to disable RCON |
| `game.scenarioId` | already `{6FFEC0DEDA000504}Missions/FFRX_Anizay.conf` |
| `game.maxPlayers` | 20 is the tested figure |
| `operating.slotReservationTimeout` | **keep it above 0.** See below |

> ### slotReservationTimeout — read this one
> At `0`, a player whose game crashes or who briefly loses connection **loses their
> character on the spot**: inventory, position, progress. The template ships `180`
> (three minutes to reconnect into the same body). Crashes happen; this setting is
> the difference between an annoyance and a ruined evening.

## 1.3 The mod list

`config.example.json` already carries the 10 entries REMIXED needs under
`game.mods`. Each is `{ "modId": "...", "name": "..." }` and the server downloads
them all on first boot.

**Do not pin versions.** The Workshop resolves dependencies against the *latest*
release of each mod regardless, so a pinned version buys you nothing and breaks
resolution in confusing ways.

> ### ⚠️ The script budget — the failure that looks like someone else's bug
>
> Arma compiles every mod's load-time static initialisers into **one** generated
> function, with a hard **64 KB bytecode budget shared between vanilla and every mod
> combined**. Vanilla already uses most of it; roughly **1 000 units** remain, where
> one `static x = new ...` costs about **6**.
>
> Overflow it and the server refuses to start with `Too many instructions per
> function`, pointing at **innocent base-game files** — nothing to do with whichever
> mod actually tipped it over.
>
> REMIXED is written to consume **zero** of that budget: every static is built
> lazily inside a getter. But stacking other script-heavy mods on top can still
> break it. If you hit it, remove script mods one at a time — not prefab or asset
> mods, they cost nothing.
>
> Class-level `const` and `enum` are free. Only eager `static` initialisers count.

## 1.4 The profile directory, and where these files go

Launch with `-profile ./profile` and Reforger **nests it**: the real profile lands in
`./profile/profile/`. That inner directory is where REMIXED reads its files.

```
D:\arma-server\
├─ config.json
├─ server\                      the game binary
└─ profile\
   └─ profile\                  ← "$profile:" means HERE
      ├─ ffrx-groups.json       squads (required)
      ├─ Fleet\GTG.json         website link (optional)
      └─ logs\                  one folder per run
```

Copy the templates in:

```
install\server\profile\ffrx-groups.example.json   →  profile\profile\ffrx-groups.json
install\server\profile\Fleet\GTG.example.json     →  profile\profile\Fleet\GTG.json
```

`ffrx-groups.json` defines the squads and their role flags (`etatMajor`, `genie`,
`medic`). Nothing is hardcoded: two engineer squads, or none, both work.

## 1.5 First boot

```
start-server.bat
```

On Linux, the same requirement applies — **you must `cd` into `server/` first**, or
the engine cannot resolve `./addons` and dies with
`Game addon '58D0FB3206B6F859' not found`:

```bash
cd ~/arma-server/server
./ArmaReforgerServer -config ../config.json -profile ../profile \
                     -maxFPS 60 -logStats 60000
```

The first start downloads every mod — slow, and players cannot join until it
finishes. Watch `profile/profile/logs/<newest>/console.log`.

**What success looks like:**

```
Game successfully created.
```

**What to grep for when it does not:**

| Pattern | Meaning |
|---|---|
| `SCRIPT (E)` | a real compile error — but see the two exceptions below |
| `SCRIPT (E): Leaked '<type>' script instance` | **normal.** Fires on every reload. Ignore |
| `... is obsolete: Use X instead` | harmless deprecation warning |
| `Too many instructions per function` | the script budget, §1.3 |
| `Cannot locate dependencies` | a mod in the list failed to download |
| `SYSTEM_FAILURE ... ReplicationError` | a hand-edited prefab with unbaked entity IDs |

> The log directory is the **newest by modification time**, not by name. Sorting by
> name gives you the wrong run.

---

# Part 2 — The website (optional)

Live map, arsenal loadouts, ranks, squads, the test-feedback page, the design docs.

**Skip this and nothing breaks.** Without `GTG.json` the mod logs one warning per
affected system at startup and those systems stay dormant.

## 2.1 Build the tree

The site is [GTG Live Map](https://github.com/SebastianUnterscheutz/GTG-livemap) by
Sebastian Unterscheutz / German Tactical Group. **We do not redistribute it** — see
[`PERMISSIONS.md`](PERMISSIONS.md). `apply.sh` clones it and lays our additions over
the top:

```bash
cd web
./apply.sh ~/gtg-livemap
```

Expect `16/16 ok`. A `FAIL` means upstream changed the same lines we did; the script
prints exactly how to resolve it.

## 2.2 Configure

```bash
cd ~/gtg-livemap
cp /path/to/install/web/config.example.yaml config.yaml
```

Two things must agree or the app cannot reach its database:

- `docker-compose.yaml` → `MARIADB_PASSWORD`
- `config.yaml` → `database.password`

Both ship as `CHANGEME_db_password`. Also in `config.yaml`:

| Key | Value |
|---|---|
| `encryption.aes_key` | **exactly 64 hex characters.** `openssl rand -hex 32`. The app panics at startup on anything else |
| `session.secret` | any long random string. Changing it logs everyone out |
| `discord.client_id` / `client_secret` | from https://discord.com/developers/applications |
| `discord.redirect_uri` | `https://<your-domain>/auth/discord/callback` — must match the Discord app **exactly**, trailing slash included |

## 2.3 Start it

```bash
docker compose up --build -d
docker logs gtg-livemap-app --tail 30
```

It answers on `http://localhost:8080`. Startup prints the migrations, then one line
per HTTP request.

## 2.4 TLS and the reverse proxy

**Discord OAuth will not redirect to plain HTTP.** You need a domain with a
certificate. With Caddy, the whole config is:

```
arma.example.com {
    reverse_proxy gtg-livemap-app:8080
}
```

Caddy obtains the certificate itself. Put it on the same Docker network as the app.

> Editing the Caddyfile needs an explicit reload — `docker compose up -d` does **not**
> pick it up:
> ```bash
> docker exec caddy caddy reload --config /etc/caddy/Caddyfile --adapter caddyfile
> ```

## 2.5 Make yourself an admin

Log in once through Discord (this creates your account), then promote it:

```bash
docker exec -it gtg-mariadb mariadb -ugtg -p<password> gtglivemap \
  -e "UPDATE users SET account_type='admin' WHERE username='<your name>';"
```

Log out and back in.

## 2.6 Point the mod at the site

In the site's dashboard, add your server. It issues an **API key**. Put it in
`profile/profile/Fleet/GTG.json` alongside your domain:

```json
{
  "url":       "https://arma.example.com/api/v1/positions",
  "markersUrl":"https://arma.example.com/api/v1/markers",
  "apiKey":    "<the key the site gave you>",
  "intervalSec": 5
}
```

Restart the server. Player positions should appear within ~10 seconds.

Endpoint-by-endpoint contract: [`FLEET_GTG_CONTRACT.md`](FLEET_GTG_CONTRACT.md).

> Two URLs are still **hardcoded** to our own site in
> `mod/Scripts/Game/FFRX/FFRX_LoadingFeed.c` (loading-screen news, player card).
> Change them or your loading screen reads from ours.

---

# Running it day to day

## Updating

```
update-server.bat
```

After a **game** update, mods re-download on the next start: the first launch is
slow and nobody can join meanwhile. After a **mod** update, restart the server; the
Workshop resolves it automatically.

> ### Save your saves before updating a mod
> Reoccupation in particular has lost player vehicles across versions. Back up
> `profile/profile/.db/` first. It costs nothing and has already saved us once.

## Admin commands

Typed into chat. `#help` lists everything by topic.

| Command | Use |
|---|---|
| `#help` | everything |
| `#pelle` | why *you* cannot build right now — tool, zone, items |
| `#radiosites` | radio sites: total, operable, held by the resistance |
| `#census` | what the enemy actually spawned, with proportions |
| `#menace` | adaptive threat gauges, per zone |

## Known failures and what they really mean

| Symptom | Cause | Fix |
|---|---|---|
| `SCRIPT_MISMATCH` on join | the client compiled scripts *before* finishing a mod download | restart the game client. Not a server problem |
| Player loses their character after a crash | `slotReservationTimeout: 0` | set it to 180 |
| `Too many instructions per function` on innocent vanilla files | the shared script budget | remove script mods one at a time |
| `Game addon '58D0FB3206B6F859' not found` | launched from the wrong directory | `cd` into `server/` first |
| `SYSTEM_FAILURE ... ReplicationError` | a hand-edited prefab with unbaked entity IDs | in the Workbench: right-click the prefab → Edit Prefab(s) → Force Save All. **Not** Reimport |
| Garbled text with a nonsense line number | non-ASCII characters inside a script string | the dedicated server compiles strictly; keep strings ASCII. The reported line number is usually wrong |
| `[CAMPAIGN][WAIT]` every 30 s, nothing ever starts | Civilian Ownership blocked the save load | check `console.log` for `[FFCO][PERSISTENCE][BLOCKED]` |
| `VM Exception NULL pointer ... m_bAutoEnabled` | a known Freedom Fighters bug in its ambient vehicle-crew spawn | harmless spam, not yours |
| `Function was not set for event: OnSuccess` | benign engine noise from REST callbacks | ignore |

## Where the logs are

| What | Where |
|---|---|
| Dedicated server | `profile/profile/logs/<newest by mtime>/console.log` |
| Website | `docker logs gtg-livemap-app` |
| Database | `docker logs gtg-mariadb` |

Also in the server log directory: `script.log`, `error.log`, `crash.log`.

---

## Before you publish a fork

REMIXED extends other people's work, mostly through `modded class`. Some authors have
consented; others have never been asked. [`PERMISSIONS.md`](PERMISSIONS.md) tracks
each one. **Read it before making a fork public.**

Freedom Fighters is under a modified **APL-ND** licence — *No Derivatives*. Johnny
Kerner granted permission for the modifications in *this* project; that permission
does not transfer to yours.
