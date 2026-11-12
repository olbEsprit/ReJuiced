#!/bin/sh
# pack-app.sh: ~/Applications/ReJuiced.app from out/build/release, for this Mac: the binary and the recomp
# libraries inside; Homebrew's libraries, the game (JUICED_GAME_DIR), work/Juiced.exe and game/ws* stay where they
# are. The launcher runs at the desktop's resolution unless JUICED_RESOLUTION says otherwise.
set -eu
cd "$(dirname "$0")/.."
build=out/build/release
app="$HOME/Applications/ReJuiced.app"
cmake --build --preset release >/dev/null
stage="work/pack/ReJuiced.app"
rm -rf work/pack && mkdir -p "$stage/Contents/MacOS" "$stage/Contents/Frameworks"
cp $build/juiced "$stage/Contents/MacOS/juiced"
cp $build/x86recomp/librecomp_*.dylib "$stage/Contents/Frameworks/"
install_name_tool -delete_rpath "$PWD/$build/x86recomp" "$stage/Contents/MacOS/juiced"
install_name_tool -add_rpath @executable_path/../Frameworks "$stage/Contents/MacOS/juiced"
for l in "$stage"/Contents/Frameworks/*.dylib; do
  install_name_tool -delete_rpath "$PWD/$build/x86recomp" "$l" 2>/dev/null || true
  install_name_tool -add_rpath @loader_path "$l" 2>/dev/null || true
done
printf '#!/bin/sh\nexport JUICED_RESOLUTION="${JUICED_RESOLUTION:-auto}"\nexec "$(dirname -- "$0")/juiced" "$@"\n' \
  > "$stage/Contents/MacOS/launch"
chmod +x "$stage/Contents/MacOS/launch"
cat > "$stage/Contents/Info.plist" <<'P'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleName</key><string>ReJuiced</string>
<key>CFBundleIdentifier</key><string>local.rejuiced</string>
<key>CFBundleExecutable</key><string>launch</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>CFBundleShortVersionString</key><string>0.1</string>
<key>LSMinimumSystemVersion</key><string>14.0</string>
<key>NSHighResolutionCapable</key><true/>
<key>LSApplicationCategoryType</key><string>public.app-category.racing-games</string>
</dict></plist>
P
codesign --force --deep -s - "$stage" >/dev/null 2>&1
rm -rf "$app" && mkdir -p "$HOME/Applications" && ditto "$stage" "$app" && rm -rf work/pack
echo "$app"
