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
  # The fix's menus keep the 4:3 boxes of the button hints, so they come out wide: each is made square (width =
  # height / the screen's aspect) about its centre, as the fix's race HUD has them.
  r=$(echo "$shape" | awk -Fx '{print $1 / $2}')
  for f in "$g"/ui/windows/*.ui; do
    awk -v r="$r" '
      /^[ \t]*BeginBox/ { n = 0; inbox = 1; hint = 0 }
      inbox { buf[n++] = $0; if ($0 ~ /Texture (ui_selecta|ui_selectb|ui_selectc|ui_back)[ \t\r]*$/) hint = 1
              if ($0 ~ /^[ \t]*EndBox/) { inbox = 0
                if (hint) { split(buf[0], a, " "); h = a[6] + 0; w = a[5] + 0; nw = h / r
                  if (w > nw * 1.05) { sub(/[ \t\r]*$/, "", buf[0]); m = match(buf[0], /[^ \t]/)
                    buf[0] = sprintf("%s%s %s %.4f %s %.4f %s\r", substr(buf[0], 1, m - 1), a[1], a[2], a[3] + (w - nw) / 2, a[4], nw, a[6]) } }
                for (i = 0; i < n; i++) print buf[i] }
              next }
      { print }' "$f" > "$f.tmp" && mv "$f.tmp" "$f"
  done
  # The tuning menu's overdrive (src/hooks.cpp sells it, and takes the item out where it is not for sale).
  awk '{ print } /AddTexturedItem ui_gears_off/ { print "\t\t AddTexturedItem ui_gears_off  ui_gears_on -1 1.00 overdrive 10 \r" }' \
    "$g/ui/windows/tuningmenu.ui" > "$g/ui/windows/tuningmenu.ui.tmp" && mv "$g/ui/windows/tuningmenu.ui.tmp" "$g/ui/windows/tuningmenu.ui"
  echo "$g"
done
