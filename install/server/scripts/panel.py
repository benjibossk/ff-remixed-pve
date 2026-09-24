#!/usr/bin/env python3
"""Mini panel web pour le serveur Arma Reforger.

Lance start-panel.bat (ou directement: python panel.py), puis ouvre
http://localhost:8080 dans le navigateur. Ne sert que sur 127.0.0.1.
"""
import html
import json
import os
import re
import subprocess
import time
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs

ROOT = os.path.dirname(os.path.abspath(__file__))
CONFIG = os.path.join(ROOT, "config.json")
START_BAT = os.path.join(ROOT, "start-server.bat")
UPDATE_BAT = os.path.join(ROOT, "update-server.bat")
EXE_NAME = "ArmaReforgerServer.exe"
PORT = 8080

# Un mod Workshop est un GUID hex de 16 caracteres.
MOD_ID_RE = re.compile(r"[0-9A-Fa-f]{16}")

CSS = """
* { box-sizing: border-box; }
body { font-family: system-ui, sans-serif; max-width: 820px; margin: 2rem auto; padding: 0 1rem;
       background: #1a1a1a; color: #e0e0e0; }
h1 { color: #6fc; margin-bottom: 0.5rem; }
h2 { color: #6fc; margin-top: 2rem; border-bottom: 1px solid #333; padding-bottom: 0.3rem; }
.status { padding: 0.6rem 1rem; border-radius: 4px; margin-bottom: 1rem; font-weight: bold; }
.status.up { background: #2a7; }
.status.down { background: #c33; }
.msg { background: #336; padding: 0.6rem 1rem; border-radius: 4px; margin-bottom: 1rem;
       white-space: pre-wrap; }
form { display: grid; gap: 0.6rem; }
form.inline { display: flex; gap: 0.5rem; align-items: center; margin: 0; }
label { display: grid; grid-template-columns: 200px 1fr; align-items: center; gap: 1rem; }
input[type=text], input[type=number], input[type=password] {
  padding: 0.45rem; background: #2a2a2a; color: #eee; border: 1px solid #444; border-radius: 4px;
  font-family: inherit; font-size: 0.95rem; width: 100%; }
input[type=checkbox] { transform: scale(1.3); justify-self: start; }
.actions { display: flex; gap: 0.5rem; margin-top: 1rem; flex-wrap: wrap; }
button { padding: 0.55rem 1rem; border: 0; border-radius: 4px; cursor: pointer; font-weight: bold;
         font-size: 0.9rem; color: white; }
.save { background: #2a7; }
.restart { background: #f73; }
.stop { background: #c33; }
.start { background: #39c; }
.update { background: #93c; }
.del { background: #c33; padding: 0.35rem 0.7rem; font-size: 0.8rem; }
table { width: 100%; border-collapse: collapse; margin-bottom: 0.8rem; }
th, td { text-align: left; padding: 0.4rem 0.5rem; border-bottom: 1px solid #333; }
th { color: #888; font-weight: normal; font-size: 0.85rem; text-transform: uppercase; }
td.mono { font-family: monospace; font-size: 0.85rem; color: #aaa; }
.add-mod { background: #222; padding: 1rem; border-radius: 4px; }
.add-mod input[type=text] { margin-bottom: 0.3rem; }
.hint { color: #888; font-size: 0.85rem; margin-top: 0.4rem; }
details { background: #222; padding: 0.6rem 1rem; border-radius: 4px; margin-top: 1.5rem; }
summary { cursor: pointer; color: #6fc; font-weight: bold; }
pre { background: #111; padding: 0.5rem; border-radius: 4px; overflow-x: auto; font-size: 0.85rem; }
"""


def load_config():
    with open(CONFIG, "r", encoding="utf-8") as f:
        return json.load(f)


def save_config(cfg):
    with open(CONFIG, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)


def is_running():
    try:
        out = subprocess.run(
            ["tasklist", "/FI", f"IMAGENAME eq {EXE_NAME}"],
            capture_output=True, text=True, timeout=5,
        )
        return EXE_NAME.lower() in out.stdout.lower()
    except Exception:
        return False


def stop_server():
    subprocess.run(["taskkill", "/IM", EXE_NAME, "/F"], capture_output=True, timeout=10)


def start_server():
    NEW_CONSOLE = 0x00000010
    subprocess.Popen(
        ["cmd", "/c", "start", "", "cmd", "/c", START_BAT],
        cwd=ROOT,
        creationflags=NEW_CONSOLE,
    )


def update_server():
    NEW_CONSOLE = 0x00000010
    subprocess.Popen(
        ["cmd", "/c", "start", "", "cmd", "/c", UPDATE_BAT],
        cwd=ROOT,
        creationflags=NEW_CONSOLE,
    )


def extract_mod_id(raw):
    """Recupere l'ID hex 16 caracteres depuis un input qui peut etre un ID
    nu ou une URL Workshop (https://reforger.armaplatform.com/workshop/<ID>)."""
    if not raw:
        return None
    m = MOD_ID_RE.search(raw)
    if not m:
        return None
    return m.group(0).upper()


def e(s):
    return html.escape(str(s), quote=True)


def render_mods_section(mods):
    rows = []
    if not mods:
        rows.append('<tr><td colspan="3" style="color:#888;">Aucun mod configure.</td></tr>')
    for i, mod in enumerate(mods):
        mod_id = mod.get("modId", "")
        name = mod.get("name", "(sans nom)")
        rows.append(f"""
        <tr>
          <td>{e(name)}</td>
          <td class="mono">{e(mod_id)}</td>
          <td>
            <form method="post" action="/" class="inline">
              <input type="hidden" name="action" value="remove_mod">
              <input type="hidden" name="modId" value="{e(mod_id)}">
              <button type="submit" class="del" onclick="return confirm('Supprimer {e(name)} ?');">Supprimer</button>
            </form>
          </td>
        </tr>""")
    rows_html = "".join(rows)

    return f"""
<h2>Mods ({len(mods)})</h2>
<table>
  <thead><tr><th>Nom</th><th>Mod ID</th><th></th></tr></thead>
  <tbody>{rows_html}</tbody>
</table>
<div class="add-mod">
  <form method="post" action="/">
    <input type="hidden" name="action" value="add_mod">
    <input type="text" name="mod_input" placeholder="ID Workshop (16 hex) OU URL https://reforger.armaplatform.com/workshop/..." required>
    <input type="text" name="mod_name" placeholder="Nom du mod (optionnel, juste pour s'y retrouver)">
    <div class="actions">
      <button type="submit" class="save">Ajouter le mod</button>
    </div>
    <p class="hint">Penser a ajouter aussi les dependances du mod (visibles sur sa page Workshop) sinon le serveur peut refuser de demarrer. Necessite un redemarrage pour prendre effet.</p>
  </form>
</div>
"""


def render(msg=""):
    cfg = load_config()
    up = is_running()
    g = cfg["game"]
    mods = g.get("mods", [])
    msg_html = f'<div class="msg">{e(msg)}</div>' if msg else ""

    main_form = f"""
<form method="post" action="/">
  <label>Nom du serveur <input type="text" name="name" value="{e(g["name"])}"></label>
  <label>Max joueurs <input type="number" name="maxPlayers" value="{e(g["maxPlayers"])}" min="1" max="128"></label>
  <label>Scenario ID <input type="text" name="scenarioId" value="{e(g["scenarioId"])}"></label>
  <label>Mot de passe (jeu) <input type="text" name="password" value="{e(g["password"])}"></label>
  <label>Mot de passe admin <input type="text" name="passwordAdmin" value="{e(g["passwordAdmin"])}"></label>
  <label>Mot de passe RCON (vide = RCON desactive ; sinon 3 car. min, sans espace) <input type="text" name="rconPassword" value="{e(cfg.get("rcon", {}).get("password", ""))}"></label>
  <label>BattlEye <input type="checkbox" name="battlEye" {"checked" if g["gameProperties"]["battlEye"] else ""}></label>
  <label>Visible (server browser) <input type="checkbox" name="visible" {"checked" if g["visible"] else ""}></label>
  <div class="actions">
    <button type="submit" name="action" value="save" class="save">Sauvegarder</button>
    <button type="submit" name="action" value="save_restart" class="restart">Sauver + Redemarrer</button>
    <button type="submit" name="action" value="start" class="start">Demarrer</button>
    <button type="submit" name="action" value="restart" class="restart">Redemarrer</button>
    <button type="submit" name="action" value="stop" class="stop">Arreter</button>
    <button type="submit" name="action" value="update" class="update"
            onclick="return confirm('Arreter le serveur et lancer la mise a jour SteamCMD ?');">Mettre a jour</button>
  </div>
</form>
"""

    scenarios_block = """
<details>
  <summary>Scenarios courants (clique pour deplier)</summary>
  <pre>FF Arland       : {E2EC49F13FBAC56F}Missions/FreedomFighters/Arland.conf
Conflict Everon : {ECC61978EDCC2B5A}Missions/23_Campaign.conf
GM Everon       : {59AD59368755F41A}Missions/21_GM_Eden.conf
GM Arland       : {2BBBE828037C6F4B}Missions/22_GM_Arland.conf</pre>
</details>
"""

    return f"""<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<title>Panel Arma Reforger</title>
<style>{CSS}</style>
</head>
<body>
<h1>Panel Arma Reforger</h1>
<div class="status {"up" if up else "down"}">Serveur : {"EN LIGNE" if up else "ARRETE"}</div>
{msg_html}
{main_form}
{render_mods_section(mods)}
{scenarios_block}
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    def _send(self, body, status=200, ctype="text/html; charset=utf-8"):
        b = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            self._send(render())
        else:
            self._send("Not found", 404, "text/plain")

    def do_POST(self):
        if self.path != "/":
            self._send("Not found", 404, "text/plain")
            return

        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode("utf-8")
        form = {k: v[0] for k, v in parse_qs(body, keep_blank_values=True).items()}
        action = form.get("action", "save")
        msg_parts = []

        if action in ("save", "save_restart"):
            try:
                cfg = load_config()

                # GARDE-FOU (ajoute le 2026-09-08 apres incident) : un formulaire
                # soumis a vide ECRASAIT toute la config -- nom, scenario et mots de
                # passe se retrouvaient vides d'un coup. Sans scenarioId le serveur ne
                # peut pas demarrer, et sans nom il est introuvable dans le navigateur.
                # Ces deux champs ne sont JAMAIS legitimement vides : on refuse la
                # sauvegarde au lieu de detruire une config qui marchait.
                new_name = form.get("name", "").strip()
                new_scenario = form.get("scenarioId", "").strip()
                if not new_name or not new_scenario:
                    manquants = []
                    if not new_name:
                        manquants.append("nom du serveur")
                    if not new_scenario:
                        manquants.append("scenarioId")
                    raise ValueError(
                        "sauvegarde REFUSEE, champ(s) vide(s) : " + ", ".join(manquants)
                        + ". La config precedente est conservee."
                    )

                cfg["game"]["name"] = new_name
                cfg["game"]["maxPlayers"] = max(1, min(128, int(form.get("maxPlayers", 20))))
                cfg["game"]["scenarioId"] = new_scenario
                cfg["game"]["password"] = form.get("password", "")
                cfg["game"]["passwordAdmin"] = form.get("passwordAdmin", "")
                # RCON : le schema du serveur EXIGE un mot de passe d'au moins 3
                # caracteres SANS espace. Ecrire un mot de passe vide produit un
                # config.json que le serveur REFUSE au demarrage :
                #   BACKEND (E): Param "#/rcon/password" is bellow the minimum limit
                #   ENGINE  (E): Unable to initialize the game
                # ... et le serveur ne demarre plus DU TOUT, avec un message qui ne
                # ressemble pas a un probleme de RCON. Arrive le 2026-09-08 : le champ
                # etait vide dans le formulaire, la sauvegarde l'a ecrit tel quel.
                #
                # RCON etant OPTIONNEL, on retire le bloc quand le mot de passe n'est
                # pas valide, plutot que d'ecrire une config que le serveur rejettera.
                # ATTENTION : le bloc rcon a d'AUTRES champs obligatoires que le mot de
                # passe -- "address" au minimum. Le recreer avec le seul password donne
                #   BACKEND (E): The following required params are missing: address
                # et le serveur refuse encore de demarrer. On complete donc systematiquement
                # les champs manquants avec les valeurs d'origine (RCON en local uniquement).
                RCON_DEFAULTS = {
                    "address": "127.0.0.1",
                    "port": 19999,
                    "maxClients": 16,
                    "permission": "admin",
                    "blacklist": [],
                    "whitelist": [],
                }
                rcon_pw = form.get("rconPassword", "")
                if len(rcon_pw) >= 3 and " " not in rcon_pw:
                    rcon = cfg.setdefault("rcon", {})
                    rcon["password"] = rcon_pw
                    for k, v in RCON_DEFAULTS.items():
                        rcon.setdefault(k, v)
                else:
                    cfg.pop("rcon", None)
                    if rcon_pw:
                        msg_parts.append("RCON desactive : mot de passe invalide (3 caracteres minimum, sans espace).")
                    else:
                        msg_parts.append("RCON desactive (aucun mot de passe defini).")
                cfg["game"]["gameProperties"]["battlEye"] = "battlEye" in form
                cfg["game"]["visible"] = "visible" in form
                save_config(cfg)
                msg_parts.append("Config sauvegardee.")
            except Exception as ex:
                msg_parts.append(f"Erreur sauvegarde: {ex}")

        elif action == "add_mod":
            mod_id = extract_mod_id(form.get("mod_input", ""))
            mod_name = form.get("mod_name", "").strip() or "(sans nom)"
            if not mod_id:
                msg_parts.append("Mod ID invalide. Attendu: 16 caracteres hex, ou URL Workshop.")
            else:
                cfg = load_config()
                mods = cfg["game"].setdefault("mods", [])
                if any(m.get("modId", "").upper() == mod_id for m in mods):
                    msg_parts.append(f"Mod {mod_id} deja present.")
                else:
                    mods.append({"modId": mod_id, "name": mod_name})
                    save_config(cfg)
                    msg_parts.append(f"Mod ajoute: {mod_name} ({mod_id}). Pense au redemarrage pour qu'il se charge, et a ajouter ses dependances si besoin.")

        elif action == "remove_mod":
            mod_id = form.get("modId", "").upper()
            cfg = load_config()
            mods = cfg["game"].setdefault("mods", [])
            before = len(mods)
            cfg["game"]["mods"] = [m for m in mods if m.get("modId", "").upper() != mod_id]
            if len(cfg["game"]["mods"]) < before:
                save_config(cfg)
                msg_parts.append(f"Mod {mod_id} retire. Pense au redemarrage.")
            else:
                msg_parts.append(f"Mod {mod_id} introuvable.")

        if action in ("restart", "save_restart"):
            stop_server()
            time.sleep(2)
            start_server()
            msg_parts.append("Serveur redemarre (peut prendre ~30s pour etre joignable).")

        elif action == "stop":
            stop_server()
            msg_parts.append("Serveur arrete.")

        elif action == "start":
            if is_running():
                msg_parts.append("Le serveur tourne deja.")
            else:
                start_server()
                msg_parts.append("Serveur demarre (peut prendre ~30s pour etre joignable).")

        elif action == "update":
            was_running = is_running()
            if was_running:
                stop_server()
                time.sleep(2)
            update_server()
            msg_parts.append(
                "Mise a jour lancee dans une nouvelle console (SteamCMD, peut prendre plusieurs minutes). "
                + ("Serveur arrete avant la MAJ ; relance-le manuellement quand la console SteamCMD est finie." if was_running
                   else "Relance le serveur quand la console SteamCMD est finie.")
            )

        self._send(render(msg=" ".join(msg_parts)))

    def log_message(self, format, *args):
        pass  # quiet


if __name__ == "__main__":
    print(f"Panel lance sur http://localhost:{PORT}")
    print("Ctrl+C pour quitter.")
    HTTPServer(("127.0.0.1", PORT), Handler).serve_forever()
