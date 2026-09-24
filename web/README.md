# The companion website

REMIXED talks to a web app: live map, arsenal loadouts, ranks, squads, the test
feedback page, the design docs. **It is entirely optional** — without it the mod
logs a warning at startup and those systems stay dormant. Nothing else breaks.

## This directory is an overlay, not a copy

The site is **[GTG Live Map](https://github.com/SebastianUnterscheutz/GTG-livemap)**
by Sebastian Unterscheutz / German Tactical Group. We did not write it and we do
**not** redistribute it — there is no LICENSE file on that repository, so no
permission to republish it is on record.

What lives here is only our own work, split by ownership:

| Directory | What it is | Whose code |
|---|---|---|
| `overlay/` | 22 Go handlers, 17 pages, `theme.css`, vendored JS libs | ours (+ MIT/BSD libs, see `overlay/static/cdn/VENDOR.md`) |
| `patches/` | 16 diffs against GTG's own files | the diffs are ours, the files are theirs |
| `notes/` | design notes per web feature | ours |

`apply.sh` puts them together: clone upstream → apply patches → copy overlay.
Same model as the mod's Workshop dependencies — you fetch the third-party part
yourself, we only ship what references it.

## Install

```bash
./apply.sh ~/gtg-livemap        # clone + patch + overlay
cd ~/gtg-livemap
# edit config.yaml and docker-compose.yaml (every CHANGEME_)
docker compose up --build -d
```

Then follow [`../docs/HOSTING.md`](../docs/HOSTING.md) for TLS, the reverse proxy,
and pointing the mod at it.

## Changing the site afterwards

Edit the real running site, get it working, then sort your changes back into this
directory automatically:

```bash
./make-patches.sh ~/gtg-livemap
```

It rebuilds `overlay/` and `patches/` from scratch and **refuses to finish if a
credential survived scrubbing**. You never edit `patches/` by hand.

> One rule: a file GTG owns must stay a patch. If you find yourself copying one of
> their files into `overlay/` to avoid a conflict, you have started
> redistributing their code.

## What we changed in GTG's own files, and why

| File | Why |
|---|---|
| `main.go` | register our 22 handlers' routes |
| `models/models.go` | our tables; `BohemiaUID` on `User` to link Discord to the in-game character |
| `database/database.go` | our migrations; **fix**: the seeded API key was stored as plaintext in the hash column, so it could never authenticate |
| `docker-compose.yaml` | upstream's compose had no database, no Redis and no published port — rebuilt as a full stack, with `static/` mounted as a volume so frontend edits need no rebuild |
| `api/handlers/positions.go` | accept the mod's payload shape; radio-quality field |
| `api/handlers/proxy.go`, `public.go`, `users.go`, `server_api_map.go` | Bohemia UID linking, public endpoints for our pages |
| `api/middleware/ownership.go` | let an `admin` account reach servers it does not own |
| `pkg/logfetcher/fetcher.go` | parse Reforger's newer log format |
| `static/index.html` | REMIXED home page, update journal, tiles to our pages |
| `static/map.html` | squad colours, drawings, objectives, radio fog of war |
| `static/admin.html`, `dashboard.html`, `privacy.html` | our sections and wording |

Two of these are genuine upstream bugs (the plaintext API key, the incomplete
compose file) and are worth contributing back rather than carrying forever.
