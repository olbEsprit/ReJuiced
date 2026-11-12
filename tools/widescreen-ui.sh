#!/bin/sh
# Usage: widescreen-ui.sh <installed game> [shape...] (16x9 16x10 15x9 25x16 64x27; default all): ws<shape> in
# JUICED_WS_DIR (default game/), the game with the HUD and menu layouts of 4D-Eclipse's Juiced Widescreen Fix V1.0 for
# that screen shape. Its release is downloaded from GitHub (not redistributed here); the game folder is linked, not
# copied, and ui.dat left out so the game reads the unpacked ui. The juiced binary picks the folder nearest the
# screen's shape by itself.
set -eu
cd "$(dirname "$0")/.."
game=$(cd "$1" && pwd); shift
[ $# -gt 0 ] || set -- 16x9 16x10 15x9 25x16 64x27
out=${JUICED_WS_DIR:-$PWD/game}
dl=$out/.download
url=https://github.com/4D-Eclipse/JuicedWidescreenFix/releases/download/V1.0/Juiced.Widescreen.Fix.Manual.install.zip
mkdir -p "$dl"
z=$dl/manual.zip
[ -f "$z" ] || curl -fsSL -o "$z" "$url"
echo "741590e4077b2514ba62b93f0f95a2161f0f695df68fa2c0007dc157f10eaada  $z" | shasum -a 256 -c - >/dev/null
[ -d "$dl/x" ] || unzip -q "$z" -d "$dl/x"
mod="$dl/x/Juiced Widescreen Fix"
for shape; do
  g=$out/ws$shape
  rm -rf "$g"; mkdir -p "$g"
  for f in "$game"/*; do [ "$(basename "$f")" = ui.dat ] || ln -s "$f" "$g/"; done
  cp -cR "$mod/Juiced/ui" "$g/ui" 2>/dev/null || cp -R "$mod/Juiced/ui" "$g/ui"
  (cd "$g" && unzip -o -q "$mod/$shape.zip")
  echo "$g"
done
