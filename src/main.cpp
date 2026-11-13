#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include <SDL3/SDL.h>

#include "runtime/log.h"
#include "runtime/display.h"
#include "runtime/gfx/overlay.h"
#include "win32/process.h"

// JUICED_KEYS="25:Return,33:+Up,40:-Up": press (or +hold/-release) keys at seconds since start, without needing focus.
static void ScriptKeys(std::string spec) {
  std::thread([spec] {
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < spec.size();) {
      size_t comma = spec.find(',', i), colon = spec.find(':', i);
      if (comma == std::string::npos) comma = spec.size();
      std::this_thread::sleep_until(t0 + std::chrono::milliseconds(long(std::stod(spec.substr(i, colon - i)) * 1000)));
      std::string name = spec.substr(colon + 1, comma - colon - 1);
      const char edge = (name[0] == '+' || name[0] == '-') ? name[0] : 0;
      if (edge) name.erase(0, 1);
      const bool cmd = name.rfind("Cmd+", 0) == 0;
      if (cmd) name.erase(0, 4);
      SDL_Scancode sc = SDL_GetScancodeFromName(name.c_str());
      int n = 0;
      SDL_Window** w = SDL_GetWindows(&n);
      for (bool down : {true, false}) {
        if ((edge == '+' && !down) || (edge == '-' && down)) continue;
        SDL_Event e{};
        e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
        e.key.windowID = n ? SDL_GetWindowID(w[0]) : 0;
        e.key.scancode = sc;
        e.key.mod = cmd ? SDL_KMOD_LGUI : 0;
        e.key.down = down;
        SDL_PushEvent(&e);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
      }
      SDL_free(w);
      i = comma + 1;
    }
  }).detach();
}

// JUICED_KEYS_FIFO=path: the same key names (Return, +Up, -Up), one per line, read as they arrive.
static void FifoKeys(const char* path) {
  std::thread([path = std::string(path)] {
    for (;;) {
      std::ifstream in(path);
      for (std::string line; std::getline(in, line);) {
        const char edge = (line[0] == '+' || line[0] == '-') ? line[0] : 0;
        SDL_Scancode sc = SDL_GetScancodeFromName(line.c_str() + (edge ? 1 : 0));
        int n = 0;
        SDL_Window** w = SDL_GetWindows(&n);
        for (bool down : {true, false}) {
          if ((edge == '+' && !down) || (edge == '-' && down)) continue;
          SDL_Event e{};
          e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
          e.key.windowID = n ? SDL_GetWindowID(w[0]) : 0;
          e.key.scancode = sc;
          e.key.down = down;
          e.key.which = 0xD51D51;  // the runtime's test input, taken in JUICED_BACKGROUND runs
          SDL_PushEvent(&e);
          if (!edge) std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
        SDL_free(w);
      }
    }
  }).detach();
}

// 4D-Eclipse's Juiced Widescreen Fix (JuicedWidescreenFix.asi 1.0, docs/widescreen.md) for 1.00: the
// widescreen camera's aspect and field of view, the 2D text's horizontal scale and the speed blur, per screen shape;
// its HUD and menu layouts (ui\windows) from ws<shape> in JUICED_WS_DIR (default game/, tools/widescreen-ui.sh) when
// that folder was made from this game folder.
namespace recomp::gen {
extern std::atomic<bool> g_overdrive_mod;  // src/hooks.cpp
extern std::atomic<bool> g_smart_crew, g_smart_opponents;  // src/ai.cpp
}

// The camera's horizontal field of view (1.333 in the EXE), widened by the widescreen fix and by the menu's FOV item.
constexpr uint32_t kFovVa = 0x6FEA14;
float g_fov = 1.333f, g_fov_scale = 1.0f;

void WidescreenFix(recomp::win32::GameConfig& g, int w, int h, std::string* game_dir) {
  struct Shape {
    const char* name;
    int w, h;
    float aspect, fov;
  };
  static const Shape kShapes[] = {{"25x16", 25, 16, 1.1175f, 1.3615f}, {"16x10", 16, 10, 1.0915f, 1.385f},
                                  {"15x9", 15, 9, 1.0915f, 1.4185f},   {"16x9", 16, 9, 1.0f, 1.43f},
                                  {"64x27", 64, 27, 0.795f, 1.675f}};
  const float r = float(w) / float(h);
  float aspect = 16.0f * h / (9.0f * w), fov = std::min(1.75f, 0.388f * r + 0.88f);
  const Shape* nearest = &kShapes[0];
  for (const Shape& s : kShapes) {
    if (s.w * h == s.h * w) aspect = s.aspect, fov = s.fov;
    if (std::fabs(float(s.w) / s.h - r) < std::fabs(float(nearest->w) / nearest->h - r)) nearest = &s;
  }
  const float kerning = r < 1.5625f ? 0.0015625f : r < 1.6666666f ? 0.0013333f : r < 1.7777778f ? 0.0013021f
                        : r < 2.3703704f                ? 0.0011710f
                                                     : 0.00087873f;
  const float blur = std::clamp(1.3f / fov, 0.0f, 0.97f);
  auto patch = [&](uint32_t va, float was, float v) {
    std::vector<uint8_t> b(4), e(4);
    std::memcpy(b.data(), &v, 4);
    std::memcpy(e.data(), &was, 4);
    g.image_patches.push_back({va, b, e});
  };
  patch(0x72477C, 1.15f, aspect);
  g_fov = fov;
  patch(0x6FE670, 0.0015625f, kerning);
  patch(0x6FEAF8, 0.975f, blur);
  namespace fs = std::filesystem;
  const char* ws_dir = std::getenv("JUICED_WS_DIR");
  const fs::path dir = fs::path(ws_dir ? ws_dir : JUICED_PORT_DIR "/game") / (std::string("ws") + nearest->name);
  std::error_code ec;
  if (fs::equivalent(dir / "textures.dat", fs::path(*game_dir) / "textures.dat", ec)) *game_dir = dir.string();
}

int main(int argc, char** argv) {
  const char* env_dir = std::getenv("JUICED_GAME_DIR");
  const char* env_exe = std::getenv("JUICED_EXE");
  std::string game_dir = env_dir ? env_dir : JUICED_DEFAULT_GAME_DIR;
  std::string exe = env_exe ? env_exe : JUICED_DEFAULT_EXE;
  std::string args;
  for (int i = 1; i < argc; i++) args += (args.empty() ? "" : " ") + std::string(argv[i]);
  if (game_dir.empty() || !std::filesystem::is_directory(game_dir)) {
    std::fprintf(stderr, "juiced: the installed game not found (JUICED_GAME_DIR=%s)\n", game_dir.c_str());
    return 2;
  }
  if (!std::filesystem::is_regular_file(exe)) {
    std::fprintf(stderr, "juiced: %s not found (JUICED_EXE)\n", exe.c_str());
    return 2;
  }

  recomp::g_trace = std::getenv("JUICED_TRACE") != nullptr;
  if (const char* k = std::getenv("JUICED_KEYS")) ScriptKeys(k);
  if (const char* f = std::getenv("JUICED_KEYS_FIFO")) FifoKeys(f);
  namespace overlay = recomp::gfx::overlay;
  if (const char* home = std::getenv("HOME"))
    overlay::SetIni(std::string(home) + "/Library/Application Support/juiced/rejuiced.ini");
  overlay::SetTitle("REJUICED");
  overlay::Add({"fov", "Field of view", {"-10%", "Game", "+10%", "+20%", "+30%"},
                [](int v) {
                  g_fov_scale = 0.9f + 0.1f * float(v);
                  if (recomp::g_proc) recomp::g_proc->mem.write<float>(kFovVa, g_fov * g_fov_scale);
                },
                1});
  overlay::Add({"overdrive", "Overdrive in the tuning menu", {"Off", "On"},
                [](int v) { recomp::gen::g_overdrive_mod = v != 0; }, 0});
  overlay::Add({"smart_crew", "Smarter crew", {"Off", "On"}, [](int v) { recomp::gen::g_smart_crew = v != 0; }, 1});
  overlay::Add({"smart_opponents", "Smarter opponents", {"Off", "On"},
                [](int v) { recomp::gen::g_smart_opponents = v != 0; }, 1});
  recomp::win32::GameConfig g;
  g.id = "juiced";
  g.guest_dir = "C:\\Program Files\\THQ\\Juiced";
  g.exe_name = "Juiced.exe";
  // JuicedConfig.exe's choice of display adapter: the DeviceIdentifier GUID the
  // d3d9 HLE reports ({7B2D3B1A-5C8E-4F4D-9A1E-5245434F4D50}). Without it the game
  // runs JuicedConfig.exe. Its other settings default from the EXE when absent.
  g.registry.push_back({"HKCU\\Software\\THQ\\Juiced", "Adapter", "", 3,
                        {0x1A, 0x3B, 0x2D, 0x7B, 0x8E, 0x5C, 0x4D, 0x4F, 0x9A, 0x1E, 0x52, 0x45,
                         0x43, 0x4F, 0x4D, 0x50}});
  // The controls file's (scripts\\controls_dx9_e.txt) DIBUTTON_ANY(n) on a pad: menus select on cross, SELECT2 on
  // triangle, back on circle; in a race the same for throttle, camera and nitro, square the handbrake, L3 reverse, R3
  // rear view, and in a race the player watches cross and triangle the previous and next car (SELECT, ANY(10), and
  // NEXTCAR, a key only). Its crew orders are keys only too: the D-pad for drivers 1 (up/down) and 2 (right/left),
  // create and the touchpad for driver 3. The rest would land on buttons the race already uses.
  g.pad_buttons = {{0xFF004400, SDL_GAMEPAD_BUTTON_SOUTH},      {0xFF004401, SDL_GAMEPAD_BUTTON_NORTH},
                   {0xFF004402, SDL_GAMEPAD_BUTTON_EAST},       {0xFF004403, -1},
                   {0xFF004404, SDL_GAMEPAD_BUTTON_WEST},       {0xFF004405, SDL_GAMEPAD_BUTTON_LEFT_STICK},
                   {0xFF004406, -1},                            {0xFF004407, SDL_GAMEPAD_BUTTON_RIGHT_STICK},
                   {0xFF004408, -1},                            {0xFF004409, -1},
                   {0xFF00440A, SDL_GAMEPAD_BUTTON_SOUTH},      {0xFF00440B, -1},
                   {0x810004D1, SDL_GAMEPAD_BUTTON_NORTH},  // DIKEYBOARD_NEXT: NEXTCAR
                   {0x81000411, SDL_GAMEPAD_BUTTON_DPAD_UP},    {0x81000410, SDL_GAMEPAD_BUTTON_DPAD_DOWN},   // W, Q
                   {0x8100041F, SDL_GAMEPAD_BUTTON_DPAD_RIGHT}, {0x8100041E, SDL_GAMEPAD_BUTTON_DPAD_LEFT},   // S, A
                   {0x8100042D, SDL_GAMEPAD_BUTTON_BACK},       {0x8100042C, SDL_GAMEPAD_BUTTON_TOUCHPAD}};   // X, Z
  // JuicedConfig.exe's display settings, as the game reads them. JUICED_RESOLUTION=WxH|auto (auto:
  // the desktop's size), JUICED_WIDESCREEN=0|1 (default: on for a mode wider than 4:3, the stock's 16:9 camera).
  int rw = 0, rh = 0;
  const char* r = std::getenv("JUICED_RESOLUTION");
  if (r && std::strcmp(r, "auto")) {
    if (std::sscanf(r, "%dx%d", &rw, &rh) != 2 || rw < 640 || rh < 480) {
      std::fprintf(stderr, "juiced: JUICED_RESOLUTION=%s: expected WxH or auto\n", r);
      return 2;
    }
  } else if (r) {
    // The desktop in points, not pixels: the game's text and some HUD are drawn in pixels and too small past ~3000.
    recomp::DesktopSize(&rw, &rh);
    const SDL_DisplayMode* m = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (m && m->pixel_density > 1) rw = int(std::lround(rw / m->pixel_density)), rh = int(std::lround(rh / m->pixel_density));
  }
  if (r) {
    const char* ws = std::getenv("JUICED_WIDESCREEN");
    const uint32_t wide = ws ? (*ws != '0') : (rw * 3 > rh * 4 + rh / 8);
    auto dword = [&](const char* name, uint32_t v) {
      g.registry.push_back({"HKCU\\Software\\THQ\\Juiced", name, "", 4,
                            {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)}});
    };
    dword("RezWidth", uint32_t(rw));
    dword("RezHeight", uint32_t(rh));
    dword("RezDepth", 32);
    dword("Windowed", 0);
    dword("widescreen", wide);
    if (wide) WidescreenFix(g, rw, rh, &game_dir);
  }
  {
    std::vector<uint8_t> b(4), e(4);
    const float fov = g_fov * g_fov_scale, was = 1.333f;
    std::memcpy(b.data(), &fov, 4);
    std::memcpy(e.data(), &was, 4);
    g.image_patches.push_back({kFovVa, b, e});
  }
  // The pad's hint pictures (src/hooks.cpp) are loose files the game finds beside its ui archive: the install overlay.
  if (const char* home = std::getenv("HOME")) {
    namespace fs = std::filesystem;
    const fs::path to = fs::path(home) / "Library/Application Support" / g.id / "install/ui/textures";
    std::error_code ec;
    fs::create_directories(to, ec);
    const char* assets = std::getenv("JUICED_ASSETS_DIR");
    for (const auto& f : fs::directory_iterator(fs::path(assets ? assets : JUICED_ASSETS_DIR) / "ui/textures", ec))
      fs::copy_file(f.path(), to / f.path().filename(), fs::copy_options::overwrite_existing, ec);
  }
  return recomp::RunProcess(g, exe, game_dir, args);
}
