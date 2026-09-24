# Configuration templates

Nothing here contains a credential. Copy each template, fill it in, and the real
file is gitignored.

| Template | Copy to | Needed for |
|---|---|---|
| `server/config.example.json` | your server root, as `config.json` | the dedicated server. **Required.** |
| `server/profile/ffrx-groups.example.json` | `$profile:ffrx-groups.json` | squads and their roles. **Required.** |
| `server/profile/Fleet/GTG.example.json` | `$profile:Fleet/GTG.json` | the website link. Optional. |
| `web/config.example.yaml` | the site directory, as `config.yaml` | the website. Only if you host it. |

> On a dedicated server, `$profile:` is whatever you passed to `-profile`. Note that
> Reforger nests it: a `-profile ./profile` gives you `./profile/profile/`, and that
> inner directory is where these files go.

## Fill these in first

**`config.json`**

- `game.passwordAdmin` and `rcon.password` — the templates are placeholders. Change them.
- `publicAddress` / `publicPort` — your public IP and port.
- `operating.slotReservationTimeout` — **keep it above 0.** At 0, a player whose
  game crashes loses their character instantly instead of reconnecting to it. This
  is the single most common way to ruin a session.
- `game.scenarioId` — already set to `{6FFEC0DEDA000504}Missions/FFRX_Anizay.conf`.

**`ffrx-groups.json`** — the squad list, their callsigns, and their role flags:

| Flag | Grants |
|---|---|
| `etatMajor` | the build menu, and validation of vehicle requests |
| `genie` | the build menu, and 2.5× build speed |
| `medic` | better healing, longer-lasting bandages |

Nothing is hardcoded: run two engineer squads, or none. Everyone outside those
squads can still shovel on a construction site someone else placed — that split is
deliberate, see [`../docs/DESIGN.md`](../docs/DESIGN.md).

**`GTG.json`** — one URL per feature plus the API key the site issued you. Drop the
file entirely and every website-backed system goes quiet on its own.

## Website passwords

Two files must agree, or the app cannot reach its database:

- `docker-compose.yaml` → `MARIADB_PASSWORD`
- `config.yaml` → `database.password`

Both ship as `CHANGEME_db_password`. Change both to the same new value.

`config.yaml` also needs an `encryption.aes_key` of **exactly 64 hex characters** —
the app panics at startup on anything else:

```bash
openssl rand -hex 32
```

Full walkthrough, including TLS and the reverse proxy:
[`../docs/HOSTING.md`](../docs/HOSTING.md).
