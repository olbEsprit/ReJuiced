#!/bin/sh
# Usage: widescreen-ui.sh <installed game> [shape...] (16x9 16x10 15x9 25x16 64x27; default all): game/ws<shape>, the
# game with the HUD and menu layouts of 4D-Eclipse's Juiced Widescreen Fix V1.0 for that screen shape. Its release is
# downloaded from GitHub (not redistributed here); the game folder is linked, not copied, and ui.dat left out so the
# game reads the unpacked ui. The juiced binary picks the folder nearest the screen's shape by itself.
set -eu
cd "$(dirname "$0")/.."
game=$1; shift
[ $# -gt 0 ] || set -- 16x9 16x10 15x9 25x16 64x27
url=https://github.com/4D-Eclipse/JuicedWidescreenFix/releases/download/V1.0/Juiced.Widescreen.Fix.Manual.install.zip
mkdir -p work/widescreen
z=work/widescreen/manual.zip
[ -f $z ] || curl -fL -o $z "$url"
echo "741590e4077b2514ba62b93f0f95a2161f0f695df68fa2c0007dc157f10eaada  $z" | shasum -a 256 -c - >/dev/null
[ -d work/widescreen/x ] || unzip -q $z -d work/widescreen/x
mod="work/widescreen/x/Juiced Widescreen Fix"
for shape; do
  g=game/ws$shape
  rm -rf "$g"; mkdir -p "$g"
  for f in "$game"/*; do [ "$(basename "$f")" = ui.dat ] || ln -s "$(cd "$(dirname "$f")" && pwd)/$(basename "$f")" "$g/"; done
  cp -cR "$mod/Juiced/ui" "$g/ui" 2>/dev/null || cp -R "$mod/Juiced/ui" "$g/ui"
  (cd "$g" && unzip -o -q "../../$mod/$shape.zip")
  echo "$g"
done
