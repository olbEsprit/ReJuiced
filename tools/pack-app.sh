#!/bin/sh
# pack-app.sh [installed game, default the build's JUICED_GAME_DIR] [out dir, default ~/Applications]: ReJuiced.app
# from out/build/release with everything inside: its libraries (SDL3, FFmpeg and theirs from Homebrew), work/Juiced.exe,
# the game (an APFS clone where it can be, so it takes no more space) and the widescreen UI (tools/widescreen-ui.sh).
# It is yours alone: it holds the game. It runs at the desktop's resolution unless JUICED_RESOLUTION says otherwise.
set -eu
cd "$(dirname "$0")/.."
build=out/build/release
game=${1:-$(sed -n 's/^JUICED_GAME_DIR:PATH=//p' $build/CMakeCache.txt)}
dest=${2:-$HOME/Applications}
[ -f "$game/textures.dat" ] || { echo "pack-app.sh: no game in '$game'" >&2; exit 1; }
cmake --build --preset release >/dev/null
stage="work/pack/ReJuiced.app"
c="$stage/Contents"
rm -rf work/pack && mkdir -p "$c/MacOS" "$c/Frameworks" "$c/Resources/tools"
cp $build/juiced "$c/MacOS/juiced"
cp $build/x86recomp/librecomp_*.dylib "$c/Frameworks/"
cp work/Juiced.exe "$c/Resources/Juiced.exe"
cp -R assets "$c/Resources/assets"
cp tools/widescreen-ui.sh "$c/Resources/tools/"
cp assets/ReJuiced.icns "$c/Resources/ReJuiced.icns"

# Homebrew's libraries the binary and the recomp libraries load, and the ones those load, become @rpath ones.
brew_deps() { otool -L "$1" | awk 'NR>1 {print $1}' | grep -E '^/(opt/homebrew|usr/local)/' || true; }
todo="$c/MacOS/juiced $(ls "$c"/Frameworks/*.dylib)"
while [ -n "$todo" ]; do
  set -- $todo; f=$1; shift; todo="$*"
  for d in $(brew_deps "$f"); do
    n=$(basename "$d")
    install_name_tool -change "$d" "@rpath/$n" "$f" 2>/dev/null
    if [ ! -f "$c/Frameworks/$n" ]; then
      cp "$(realpath "$d")" "$c/Frameworks/$n" && chmod u+w "$c/Frameworks/$n"
      install_name_tool -id "@rpath/$n" "$c/Frameworks/$n" 2>/dev/null
      todo="$todo $c/Frameworks/$n"
    fi
  done
done
for f in "$c/MacOS/juiced" "$c"/Frameworks/*.dylib; do
  for r in $(otool -l "$f" | awk '/cmd LC_RPATH/ {getline; getline; print $2}'); do
    install_name_tool -delete_rpath "$r" "$f" 2>/dev/null || true
  done
done
install_name_tool -add_rpath @executable_path/../Frameworks "$c/MacOS/juiced"
for f in "$c"/Frameworks/*.dylib; do install_name_tool -add_rpath @loader_path "$f" 2>/dev/null || true; done

cat > "$c/MacOS/launch" <<'L'
#!/bin/sh
r=$(cd "$(dirname "$0")/../Resources" && pwd)
data="$HOME/Library/Application Support/juiced"
mkdir -p "$data"
export JUICED_GAME_DIR="$r/game" JUICED_EXE="$r/Juiced.exe" JUICED_ASSETS_DIR="$r/assets" JUICED_WS_DIR="$r/ws"
export JUICED_RESOLUTION="${JUICED_RESOLUTION:-auto}"
exec "$(dirname "$0")/juiced" "$@" >"$data/rejuiced.log" 2>&1
L
chmod +x "$c/MacOS/launch"
cat > "$c/Info.plist" <<'P'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>ReJuiced</string>
<key>CFBundleDisplayName</key><string>ReJuiced</string>
<key>CFBundleIdentifier</key><string>io.github.olbesprit.rejuiced</string>
<key>CFBundleExecutable</key><string>launch</string>
<key>CFBundleIconFile</key><string>ReJuiced</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.1</string>
<key>LSMinimumSystemVersion</key><string>14.0</string>
<key>NSHighResolutionCapable</key><true/>
<key>LSApplicationCategoryType</key><string>public.app-category.racing-games</string>
</dict></plist>
P
mkdir -p "$dest" && rm -rf "$dest/ReJuiced.app" && ditto "$stage" "$dest/ReJuiced.app" && rm -rf work/pack
r="$dest/ReJuiced.app/Contents/Resources"
cp -cR "$game" "$r/game" 2>/dev/null || cp -R "$game" "$r/game"
JUICED_WS_DIR="$r/ws" tools/widescreen-ui.sh "$r/game" >/dev/null
rm -rf "$r/ws/.download"
codesign --force --deep -s - "$dest/ReJuiced.app" >/dev/null 2>&1
echo "$dest/ReJuiced.app"
