#!/usr/bin/env bash
# Build a ready-to-run GTG Live Map tree carrying the REMIXED features.
#
# We do NOT redistribute GTG Live Map. This script clones it from its own
# repository, then lays our additions on top:
#
#   1. clone  github.com/SebastianUnterscheutz/GTG-livemap
#   2. copy   overlay/   -> our own new handlers and pages (we wrote these)
#   3. apply  patches/   -> our edits to GTG's own files
#
# Usage:  ./apply.sh [target-directory]      (default: ./build)
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${1:-$HERE/build}"
UPSTREAM="https://github.com/SebastianUnterscheutz/GTG-livemap.git"

# The commit our patches were generated against. If you bump this, re-check every
# patch: a patch that no longer applies means upstream changed the same lines.
UPSTREAM_REF="649596f"

command -v git >/dev/null || { echo "git is required"; exit 1; }

# ---------------------------------------------------------------- 1. upstream
if [ -d "$TARGET/.git" ]; then
  echo "==> reusing existing clone at $TARGET"
  git -C "$TARGET" fetch --all --quiet
  git -C "$TARGET" checkout --quiet --force "$UPSTREAM_REF"
  git -C "$TARGET" clean -fdq -e config.yaml -e build
else
  echo "==> cloning GTG Live Map into $TARGET"
  git clone --quiet "$UPSTREAM" "$TARGET"
  git -C "$TARGET" checkout --quiet "$UPSTREAM_REF"
fi

# ---------------------------------------------------------------- 2. patches
# Patches BEFORE overlay: they touch GTG's files only, and must see them pristine.
echo "==> applying $(ls -1 "$HERE/patches"/*.patch | wc -l) patches to GTG files"
failed=0
for p in "$HERE"/patches/*.patch; do
  # Escalating attempts. --ignore-whitespace is not cosmetic here: the patches are
  # generated on Windows, where the worktree is CRLF and the index is LF, so a
  # strict apply fails on the carriage returns alone. --3way is the last resort,
  # using blob hashes instead of context.
  if   git -C "$TARGET" apply --whitespace=nowarn "$p" 2>/dev/null;                      then how=exact
  elif git -C "$TARGET" apply --whitespace=nowarn --ignore-whitespace "$p" 2>/dev/null;  then how=whitespace
  elif git -C "$TARGET" apply --whitespace=nowarn --3way "$p" 2>/dev/null;               then how=3way
  else how=""; fi

  if [ -n "$how" ]; then
    printf '    ok   %-38s (%s)\n' "$(basename "$p")" "$how"
  else
    printf '    FAIL %s\n' "$(basename "$p")"
    failed=$((failed + 1))
  fi
done

if [ "$failed" -gt 0 ]; then
  cat <<EOF

!! $failed patch(es) did not apply.

   This means upstream GTG changed the same lines we did. Nothing is broken —
   the clone is simply missing those edits. Resolve each one by hand:

       cd "$TARGET"
       git apply --3way ../patches/<name>.patch     # then fix the conflict markers

   Once the tree works again, regenerate our patch set so the next install is
   clean:  ../make-patches.sh "$TARGET"

EOF
  exit 1
fi

# ---------------------------------------------------------------- 3. overlay
echo "==> copying our own files (overlay)"
cp -r "$HERE/overlay/." "$TARGET/"

# ---------------------------------------------------------------- 4. config
if [ ! -f "$TARGET/config.yaml" ]; then
  cp "$HERE/../install/web/config.example.yaml" "$TARGET/config.yaml"
  echo "==> wrote a placeholder config.yaml -- EDIT IT before starting"
fi

cat <<EOF

Done. The tree at $TARGET is ready.

Next:
  1. edit $TARGET/config.yaml           (DB password, Discord OAuth, session secret)
  2. edit $TARGET/docker-compose.yaml   (the two CHANGEME_ database passwords)
  3. cd "$TARGET" && docker compose up --build -d

The site then answers on http://localhost:8080 . Put it behind a reverse proxy
with TLS before exposing it; see ../install/README.md.
EOF
