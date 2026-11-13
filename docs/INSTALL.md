# Installing ReJuiced

ReJuiced is built on your Mac from this repository and your own copy of the game. Nothing of the game is in the
repository, and no ready app is published. The steps below take about half an hour, most of it compiling.

## 1. What you need

**A Mac** on Apple Silicon (M1 or later), macOS 14 or newer, about 15 GB free during the build.

**Juiced, the PC version (2005, THQ / Juice Games), patched to 1.00:**
- **The installed game folder**, the one holding `textures.dat`, `audio/`, `scripts/`, … ReJuiced reads its data
  files as they are. Any way of installing it works: on a Windows PC, in a virtual machine, or in CrossOver or Wine.
  Copy the folder to the Mac afterwards; the Mac does not need Windows or Wine to run the game.
- **`Juiced.exe` version 1.00 without copy protection.** The EXE on the disc is protected and does not work; you need
  the unprotected 1.00 EXE (the one no-CD patches ship). Check it:

  ```sh
  shasum -a 256 Juiced.exe
  # 6e9b5d5ed5e3bd2319da5ab7d73dd396c78fc95cacbd40101427dc6f03ca9d1b
  ```

  Any other hash means a different version, and the build stops at step 3.

The Xbox and PlayStation 2 versions do not work: ReJuiced recompiles the PC EXE.

**Developer tools:** the Xcode command line tools (`xcode-select --install`) and [Homebrew](https://brew.sh), then

```sh
brew install cmake ninja python sdl3 ffmpeg freetype llvm@22 glslang spirv-cross
```

## 2. Get the source

```sh
git clone https://github.com/olbEsprit/ReJuiced.git
cd ReJuiced
tools/fetch-x86recomp.sh            # SwARc, the recompiler and runtime, at the pinned commit
```

## 3. Prepare the EXE and recompile it

```sh
tools/prepare-exe.sh /path/to/Juiced.exe   # work/Juiced.exe; checks both hashes
tools/lift.sh                              # x86 code to C++ in generated/ (a minute)
```

## 4. Build the app

```sh
cmake --preset release -DJUICED_GAME_DIR=/path/to/installed/Juiced \
      -DRECOMP_LLVM_ROOT=/opt/homebrew/opt/llvm@22
cmake --build --preset release              # the long part
tools/pack-app.sh                           # ~/Applications/ReJuiced.app
```

`pack-app.sh` also fetches the widescreen UI (4D-Eclipse's JuicedWidescreenFix, see `widescreen.md`) and puts the
game inside the app as an APFS clone, so it takes no extra space. The app is yours alone: it holds the game.

## 5. Play

Open `~/Applications/ReJuiced.app`. It runs full screen at the desktop's resolution. A gamepad (DualSense, Xbox, …)
works when connected; L3+R3 (or F4) opens ReJuiced's settings menu.

Saves and settings: `~/Library/Application Support/juiced/` (`rejuiced.log` is the log to send with a bug report).

## Building with an AI agent

The steps above are plain shell commands, so a coding agent (Claude Code, Codex, …) can run them for you. Open it
in an empty folder and give it, for example:

> Build ReJuiced from https://github.com/olbEsprit/ReJuiced following docs/INSTALL.md. My installed Juiced is at
> `<path to the game folder>`, the 1.00 EXE without copy protection is `<path to Juiced.exe>`. Install what is
> missing with Homebrew, stop and tell me if a hash check fails, and finish with ReJuiced.app in ~/Applications.

The agent needs only those two paths; it should never need to download the game.

## When something fails
- `prepare-exe.sh` hash mismatch: the EXE is not the unprotected 1.00 one.
- `cmake` cannot find SDL3, glslang or LLVM: rerun the `brew install` line; pass `-DRECOMP_LLVM_ROOT` as above.
- The app closes at start: look at the end of `~/Library/Application Support/juiced/rejuiced.log`; the second intro
  movie can hang now and then, and restarting gets past it.
