# mod/ — the Arma Reforger addon

This is the addon itself. **Workshop GUID `69DF0268E97F72A3`.**

| | |
|---|---|
| What it does, and why it is built this way | [`../docs/DESIGN.md`](../docs/DESIGN.md) |
| How to host it | [`../docs/HOSTING.md`](../docs/HOSTING.md) |
| Whose work it extends, and who has consented | [`../docs/PERMISSIONS.md`](../docs/PERMISSIONS.md) |
| Vision, roadmap, engineering notes | `GAME_DESIGN.md`, `ROADMAP.md`, `DOC_TECHNIQUE.md` |

## Working on it in the Workbench

The Workbench expects addons under
`Documents\My Games\ArmaReforgerWorkbench\addons\`. The real files live here, in the
repository, and that path is a **directory junction** pointing at this folder:

```
mklink /J "%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\FF - REMIXED - PVE" "<repo>\mod"
```

The Workbench cannot tell the difference, and every edit you make in the editor lands
directly in the repository. Recreate the junction after a fresh clone — git does not
store it.

## Two rules that bite hardest

**Never hold an eager static.** Arma compiles every mod's load-time static
initialisers into one function with a **64 KB budget shared with vanilla and all
other mods** — roughly 1 000 units left, ~6 per `static x = new ...`. Overflow reports
errors in *innocent base-game files* and the server will not start. Build statics
lazily in a getter; this addon consumes **zero**. `const` and `enum` are free.

**Keep strings ASCII.** The dedicated server compiles more strictly than the
Workbench: a non-ASCII byte inside a string desyncs it, and the reported line number
is usually wrong. Comments may use accents; string literals may not.

More of these, with the cost each one incurred, at the end of
[`../docs/DESIGN.md`](../docs/DESIGN.md).
