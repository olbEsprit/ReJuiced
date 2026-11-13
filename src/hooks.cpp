#include <cstring>
#include <mutex>
#include <vector>

#include "runtime/hle/kernel32_state.h"
#include "runtime/hle/xinput.h"
#include "runtime/runtime.h"

namespace recomp::gen {

void f_00510090_lifted(Ctx& c);
void f_004F4B90_lifted(Ctx& c);
void f_004EEA60(Ctx& c);

// See manifest/juiced.manifest: the CPU the runtime reports has SSE and
// SSE2 and nothing the lifter does not implement (MMX, 3DNow!, SSE3).
void f_005A16D0(Ctx& c) {
  const uint32_t feature = Rd32(c.esp + 4);
  c.eax = (feature == 2 || feature == 4) ? 1 : 0;
  c.esp += 8;  // ret 4
}

// Texture load by name (manifest): with a pad connected, the menus' keyboard hints load the pad's pictures
// (assets/ui/textures, copied into the install overlay by main.cpp). The menus load their pictures once, so the
// choice is made when they load.
void f_00510090(Ctx& c) {
  static const char* const kPrompts[][2] = {
      {"ui_selecta", "oj_pad_a"}, {"ui_back", "oj_pad_b"}, {"ui_selectb", "oj_pad_y"}, {"ui_selectc", "oj_pad_x"}};
  static gaddr names[4];
  const gaddr name = Rd32(c.esp + 8);
  if (name && !hle::XInputPads().empty()) {
    for (int i = 0; i < 4; i++) {
      if (std::strcmp(reinterpret_cast<const char*>(GPtr(name)), kPrompts[i][0]) != 0) continue;
      if (!names[i]) {
        names[i] = HeapAllocProcess(16, true);
        std::strcpy(reinterpret_cast<char*>(GPtr(names[i])), kPrompts[i][1]);
      }
      g_proc->mem.write<uint32_t>(c.esp + 8, uint32_t(names[i]));
      break;
    }
  }
  f_00510090_lifted(c);
}

// Overdrive (the menu, main.cpp): 4- and 5-speed cars get gears up to a sixth, each the last ratio squared over the one
// before. Car scripts are read once at start, so each such car's ratios get room for six there (the game's operator new,
// 0x004EEA60) and the menu only moves the vector's end.
namespace {
struct Gearbox {
  uint32_t car;
  int gears;
};
std::mutex g_gear_mu;
std::vector<Gearbox> g_gearboxes;
bool g_overdrive = false;

void FitGears(const Gearbox& g) {
  const uint32_t first = Rd32(g.car + 0x12C);
  int n = g.gears;
  if (g_overdrive)
    for (; n < 6; n++) {
      const float last = g_proc->mem.read<float>(first + (n - 1) * 4), before = g_proc->mem.read<float>(first + (n - 2) * 4);
      g_proc->mem.write<float>(first + n * 4, last * last / before);
    }
  g_proc->mem.write<uint32_t>(g.car + 0x130, first + uint32_t(n) * 4);
}
}  // namespace

void SetOverdrive(bool on) {
  std::lock_guard<std::mutex> l(g_gear_mu);
  g_overdrive = on;
  if (g_proc)
    for (const Gearbox& g : g_gearboxes) FitGears(g);
}

void f_004F4B90(Ctx& c) {
  const uint32_t reader = c.ecx;
  f_004F4B90_lifted(c);
  const uint32_t car = Rd32(reader + 0x68), first = Rd32(car + 0x12C);
  const int gears = int((Rd32(car + 0x130) - first) / 4);
  if (gears != 4 && gears != 5) return;
  if (Rd32(car + 0x138) - first < 6 * 4) {
    const uint32_t eax = c.eax, ecx = c.ecx, edx = c.edx;
    c.esp -= 8;
    g_proc->mem.write<uint32_t>(c.esp + 4, 6 * 4);
    g_proc->mem.write<uint32_t>(c.esp, 0);  // the return address the callee pops
    f_004EEA60(c);
    c.esp += 4;
    const uint32_t room = c.eax;
    c.eax = eax, c.ecx = ecx, c.edx = edx;
    if (!room) return;
    std::memcpy(GPtr(room), GPtr(first), size_t(gears) * 4);
    g_proc->mem.write<uint32_t>(car + 0x12C, room);
    g_proc->mem.write<uint32_t>(car + 0x138, room + 6 * 4);
  }
  std::lock_guard<std::mutex> l(g_gear_mu);
  g_gearboxes.push_back({car, gears});
  FitGears(g_gearboxes.back());
}

}  // namespace recomp::gen
