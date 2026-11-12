# ReJuiced

A native Apple Silicon (arm64) build of **Juiced** (2005, PC), with no Wine, CrossOver or emulator. The game's x86
code is statically recompiled to C++ with [SwARc](https://github.com/JTM-rootstorm/SwARc) (formerly x86recomp), and
Windows (Direct3D 9, DirectSound, DirectInput, Bink, kernel32) is replaced by SwARc's runtime on Metal, SDL3 and
CoreAudio.

> **Work in progress.** The game plays from the menus through races on an M-series Mac, but it has not been
> played through, and the build steps below are the only way to get it for now.

**This repository contains no part of the game.** You need your own copy of Juiced: its installed data files and the
version 1.00 `Juiced.exe` without copy protection (sha256
`6e9b5d5ed5e3bd2319da5ab7d73dd396c78fc95cacbd40101427dc6f03ca9d1b`). Neither is distributed here.

## What works
- Menus, career and races, at the desktop's resolution.
- Gamepads (SDL3: DualSense, Xbox, …), with pad button hints in the menus.
- Widescreen: camera, field of view and HUD for 16:10, 16:9 and wider, with the UI from 4D-Eclipse's
  JuicedWidescreenFix (downloaded by a script, see `docs/widescreen.md`).

## Known issues
- The game can hang on the second intro movie now and then; restarting gets past it.
- With a gamepad connected, the log fills with the game's wheel-wrapper messages.

## Build
Needs macOS 14+ on Apple Silicon, Xcode command line tools, CMake 3.25+, Ninja, Python 3, and Homebrew `sdl3`
and `ffmpeg`.

```sh
tools/fetch-x86recomp.sh                  # SwARc at the commit X86RECOMP pins, into vendor/
tools/prepare-exe.sh /path/to/Juiced.exe  # work/Juiced.exe, the lifter's input
tools/lift.sh                             # x86 → C++ in generated/
cmake --preset release -DJUICED_GAME_DIR=/path/to/installed/Juiced
cmake --build --preset release
tools/pack-app.sh                         # ~/Applications/ReJuiced.app, with the game inside
```

`ReJuiced.app` carries everything: the libraries, your prepared EXE, the game (an APFS clone of the folder the build
names, so it takes no more space) and its widescreen UI. It is yours alone; its log is
`~/Library/Application Support/juiced/rejuiced.log`. The build runs without the app too, as `out/build/release/juiced`;
`tools/widescreen-ui.sh /path/to/installed/Juiced` makes the widescreen UI for it in `game/`.

| variable | meaning |
|---|---|
| `JUICED_GAME_DIR` | the installed game, overrides the build's default |
| `JUICED_RESOLUTION` | `WxH` or `auto` (the desktop's size in points, e.g. 3008x1692 on a 6016x3384 screen); unset, the game's own settings |
| `JUICED_WIDESCREEN` | `0`/`1`; by default on for a mode wider than 4:3 |
| `JUICED_WS_DIR` | where `tools/widescreen-ui.sh` put the widescreen UI (default `game/`) |

## Layout
- `src/`: the port: the game's configuration for the runtime, a few replaced functions, widescreen values.
- `manifest/`, `symbols/`: what the lifter needs: functions, jump tables, replaced functions.
- `prep/`, `tools/prepare_exe.py`: turns the EXE into the lifter's input: a list of the original code's instruction
  addresses, so code split away from the EXE is put back in place.
- `assets/`: pad button pictures (from Xelu's CC0 prompts) in the game's texture format.

## SwARc
The runtime changes the game needs (Direct3D 9, DirectInput pads, DirectShow audio, Bink, the lifter) are on the
[`juiced`](https://github.com/olbEsprit/SwARc/tree/juiced) branch of a fork, pinned in `X86RECOMP`; the general ones
are meant to go upstream.

## License
MIT, see `LICENSE`. Third-party parts: `THIRD_PARTY_NOTICES.md`. Juiced is a trademark of its owners; this project
is not affiliated with them.
