// Smarter AI (the menu's "Smarter crew" and "Smarter opponents"), hooked into the stock driving code
// (manifest `hook` lines): a car blocked by a slower one changes line instead of staying behind it, brakes only as
// much as closing the gap needs, and dodges a car ahead in every race type (stock: sprints only).
#include <algorithm>
#include <atomic>

#include "win32/process.h"

namespace recomp::gen {

std::atomic<bool> g_smart_crew{true}, g_smart_opponents{true};

namespace {

uint32_t Rd32(uint32_t a) { return g_proc->mem.read<uint32_t>(a); }
float RdF(uint32_t a) { return g_proc->mem.read<float>(a); }

// A competitor's car's forward speed.
float Speed(uint32_t competitor) { return RdF(Rd32(Rd32(competitor + 0x08) + 0x168) + 0x1A0); }

// The player's crew in a career race, or any other AI car.
bool Smart(uint32_t competitor) {
  const uint32_t car = Rd32(competitor + 0x08);
  if (Rd32(car + 0x08) != 1) return false;
  const bool crew = Rd32(car + 0x47C) == 0 && Rd32(Rd32(0x0075DC74) + 0x6C) == 2;
  return crew ? g_smart_crew.load() : g_smart_opponents.load();
}

uint32_t g_race_type_esp = 0, g_race_type = 0;

}  // namespace

// chooseLine (0x0049B120), at its revert of a line change next to a neighbour (esi competitor, edi neighbour,
// ebx the old line): keep the change when the neighbour ahead is slower.
void ai_line(Ctx& c) {
  if (!Smart(c.esi) || RdF(c.edi + 0x0C) < 150.0f || Speed(Rd32(c.edi)) > Speed(c.esi) - 1.0f) return;
  c.ebx = Rd32(c.esi + 0x7C);
  g_proc->mem.write<uint32_t>(c.esi + 0xC0, 0);   // magnet
  g_proc->mem.write<float>(c.esi + 0xBC, 3.0f);  // line hold, as the stock's own change
}

// speed (0x0049D8E0), after it set full brake for a neighbour ahead (ebp competitor, esi neighbour): as much as
// the closing speed needs.
void ai_brake(Ctx& c) {
  if (!Smart(c.ebp)) return;
  const float brake = std::clamp((Speed(c.ebp) - Speed(Rd32(c.esi))) / 5.0f, 0.0f, 1.0f);
  g_proc->mem.write<float>(c.ebp + 0x3C, brake);
}

// steer (0x0049C450): its dodge of a car ahead runs for race type 6 (sprint) only; it sees 6 from here
// (ebx competitor) to the block's end (ai_dodge_end).
void ai_dodge(Ctx& c) {
  if (!Smart(c.ebx)) return;
  const uint32_t at = c.esp + 0x4C, type = Rd32(at);
  if (type == 6) return;
  g_race_type_esp = c.esp;
  g_race_type = type;
  g_proc->mem.write<uint32_t>(at, 6);
}

void ai_dodge_end(Ctx& c) {
  if (!g_race_type_esp || g_race_type_esp != c.esp) return;
  g_proc->mem.write<uint32_t>(c.esp + 0x4C, g_race_type);
  g_race_type_esp = 0;
}

}  // namespace recomp::gen
