#!/usr/bin/env bash
# Regenerate patches/ and overlay/ from a working GTG Live Map tree.
#
# You never edit patches/ by hand. You edit the real site, get it working, then run
# this script — it sorts your changes into the two categories automatically:
#
#   a file GTG does not have  ->  overlay/   (copied verbatim, it is ours)
#   a file GTG does have      ->  patches/   (stored as a diff, theirs)
#
# Usage:  ./make-patches.sh /path/to/your/GTG-livemap
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${1:?usage: make-patches.sh /path/to/GTG-livemap}"

[ -d "$SRC/.git" ] || { echo "$SRC is not a git checkout of GTG Live Map"; exit 1; }

# ---------------------------------------------------------------------------
# Credential scrubbing.
#
# Your real passwords and API keys must never land in a patch. The list of values
# to replace lives in scrub.local, which is GITIGNORED — it must not be in this
# script, because the script itself is published and the left-hand side of each
# rule IS the secret.
#
# See scrub.example for the format. No scrub.local means no scrubbing, so if your
# tree contains any credential at all, create one.
# ---------------------------------------------------------------------------
SCRUB_FILE="$HERE/scrub.local"
SED_ARGS=()
SECRETS=()

if [ -f "$SCRUB_FILE" ]; then
  while IFS= read -r line; do
    case "$line" in ''|'#'*) continue ;; esac
    real="${line%%=*}"
    fake="${line#*=}"
    [ -n "$real" ] && [ "$real" != "$line" ] || continue
    SED_ARGS+=(-e "s|${real}|${fake}|g")
    SECRETS+=("$real")
  done < "$SCRUB_FILE"
  echo "scrub: ${#SECRETS[@]} rule(s) from scrub.local"
else
  echo "scrub: no scrub.local -- NOTHING will be redacted."
  echo "       If your tree holds any password or API key, copy scrub.example first."
fi

scrub() {
  if [ "${#SED_ARGS[@]}" -gt 0 ]; then sed "${SED_ARGS[@]}"; else cat; fi
}

rm -rf "$HERE/patches" "$HERE/overlay"
mkdir -p "$HERE/patches" "$HERE/overlay" "$HERE/notes"

cd "$SRC"
n_over=0
n_patch=0

# --- ours: untracked files, i.e. upstream has never heard of them ---
while IFS= read -r f; do
  [ -n "$f" ] || continue
  case "$f" in
    TASK_*.md) cp "$f" "$HERE/notes/" ;;
    */)  mkdir -p "$HERE/overlay/$f"; cp -r "$f." "$HERE/overlay/$f"
         n_over=$((n_over + $(find "$f" -type f | wc -l))) ;;
    *)   mkdir -p "$HERE/overlay/$(dirname "$f")"; cp "$f" "$HERE/overlay/$f"
         n_over=$((n_over + 1)) ;;
  esac
done < <(git status --short | grep '^??' | sed 's/^...//')

# --- theirs: tracked files we modified ---
while IFS= read -r f; do
  [ -n "$f" ] || continue
  # --ignore-cr-at-eol, and NEVER override core.autocrlf: on Windows the worktree is
  # CRLF while the index is LF, so forcing autocrlf=false makes git report every
  # single line as changed. The patch then becomes a full-file rewrite of somebody
  # else's source — exactly what we must not ship.
  git diff --ignore-cr-at-eol -- "$f" | scrub \
    > "$HERE/patches/$(echo "$f" | tr '/' '_').patch"
  n_patch=$((n_patch + 1))
done < <(git status --short | grep -E '^ M|^M ' | sed 's/^...//')

echo "overlay: $n_over files    patches: $n_patch files"

# --- the guard rail: refuse to leave a secret behind ---
leaked=0
for s in "${SECRETS[@]}"; do
  if grep -rqF "$s" "$HERE/overlay" "$HERE/patches" 2>/dev/null; then
    echo "!! a scrubbed value still appears in the output (rule: ${s:0:4}...)"
    leaked=1
  fi
done
# Belt and braces: a filled-in credential in any yaml that slipped into overlay/.
if grep -rqE 'client_secret: *"[^"<]' "$HERE/overlay" 2>/dev/null; then
  echo "!! a real client_secret is present in overlay/"
  leaked=1
fi
[ "$leaked" -eq 0 ] || { echo "aborting: fix scrub.local and re-run."; exit 1; }
echo "secret scan: clean"
