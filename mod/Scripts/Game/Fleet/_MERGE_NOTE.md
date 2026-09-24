FF - REMIXED - PVE -- FLEET, MERGED IN (2026-09-24)

These files used to be a separate Workshop addon, "Fleet Arma Reforger Plugin"
(GUID 65A4BD29CD32109E). They were merged into REMIXED because only this project
ever used them, and keeping two addons in sync for 11 files cost more than it gave.

What the merge changed:
  - Fleet's GUID was REMOVED from addon.gproj dependencies;
  - AnarchyMarkers (69A510CE600D1126) was ADDED, because Fleet brought it and
    Flt_MarkerBridge still needs it;
  - MapExport/ (66 MB of Arland map tiles) was deliberately NOT carried over --
    it was dead weight for a scenario played on Anizay.

⚠️ If you ever load the old Fleet addon alongside this one, every Flt_ class is
declared twice and the whole script module fails to compile. Disable it.

The web contract these files implement is documented in docs/FLEET_GTG_CONTRACT.md.
