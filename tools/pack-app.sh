#!/bin/sh
# pack-app.sh [out dir, default ~/Applications]: ReJuiced.app from out/build/release, with its libraries (SDL3,
# FFmpeg and theirs from Homebrew) and work/Juiced.exe inside. On its first start it asks for the installed game and
# makes the widescreen UI folders (tools/widescreen-ui.sh) in ~/Library/Application Support/juiced; the game itself
# stays where it is. The app runs at the desktop's resolution unless JUICED_RESOLUTION says otherwise.
set -eu
cd "$(dirname "$0")/.."
build=out/build/release
dest=${1:-$HOME/Applications}
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
# The game folder is asked for once and kept in ~/Library/Application Support/juiced/game-dir.
here=$(cd "$(dirname "$0")/.." && pwd)
data="$HOME/Library/Application Support/juiced"
mkdir -p "$data"
exec >"$data/rejuiced.log" 2>&1
game=$(cat "$data/game-dir" 2>/dev/null || true)
while [ ! -f "$game/textures.dat" ]; do
  game=$(osascript -e 'POSIX path of (choose folder with prompt "Choose the installed Juiced folder (the one with textures.dat):")') || exit 0
  game=${game%/}
  [ -f "$game/textures.dat" ] || osascript -e 'display alert "ReJuiced" message "That folder has no textures.dat: choose the folder Juiced is installed in."'
done
printf '%s' "$game" > "$data/game-dir"
if [ ! -e "$data/ws16x9/textures.dat" ] || [ "$(cd "$data/ws16x9" && realpath textures.dat)" != "$(realpath "$game/textures.dat")" ]; then
  osascript -e 'display notification "Setting up the widescreen UI, once." with title "ReJuiced"' || true
  JUICED_WS_DIR="$data" sh "$here/Resources/tools/widescreen-ui.sh" "$game" || true
fi
export JUICED_GAME_DIR="$game" JUICED_EXE="$here/Resources/Juiced.exe" JUICED_ASSETS_DIR="$here/Resources/assets"
export JUICED_WS_DIR="$data" JUICED_RESOLUTION="${JUICED_RESOLUTION:-auto}"
exec "$here/MacOS/juiced" "$@"
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
codesign --force --deep -s - "$stage" >/dev/null 2>&1
mkdir -p "$dest" && rm -rf "$dest/ReJuiced.app" && ditto "$stage" "$dest/ReJuiced.app" && rm -rf work/pack
echo "$dest/ReJuiced.app"
