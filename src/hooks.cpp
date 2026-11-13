#include <atomic>
#include <cstring>
#include <initializer_list>
#include <map>
#include <mutex>
#include <strings.h>

#include "runtime/hle/kernel32_state.h"
#include "runtime/hle/xinput.h"
#include "runtime/runtime.h"

namespace recomp::gen {

void f_00510090_lifted(Ctx& c);
void f_0048DFE0_lifted(Ctx& c);
void f_0043A890_lifted(Ctx& c);
void f_0043A9C0_lifted(Ctx& c);
void f_004427C0_lifted(Ctx& c);
void f_00441670_lifted(Ctx& c);
void f_00441910_lifted(Ctx& c);
void f_005A5D80(Ctx& c);
void f_005AEB60(Ctx& c);
void f_005AEEA0(Ctx& c);
void f_005A8920(Ctx& c);
void f_005AF510(Ctx& c);
void f_005A9910(Ctx& c);
void f_005AF7A0(Ctx& c);
void f_005AF740(Ctx& c);
void f_005A86C0(Ctx& c);
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

// An overdrive, sold in the tuning menu as in OpenJuice: the car's gearbox part byte (+0x17) 0x7F gives a 5-speed car a
// sixth gear for $1500, 0x7E a 4-speed car a fifth and a sixth for $3000, each the last ratio squared over the one before.
// The menu's item (tools/widescreen-ui.sh adds it to tuningmenu.ui) shows while ReJuiced's menu has the mod on and the
// car fits; the game's own buymod window asks, takes the money and sets the garage's bought flag.
std::atomic<bool> g_overdrive_mod{false};

namespace {
constexpr uint32_t kGaragePrice = 0x766D28, kGarageRating = 0x766D40, kGarageRefund = 0x766D44,
                   kGarageBought = 0x766D54, kGarageView = 0x75DCDC;  // the view's +0x8C: the car
std::mutex g_gear_mu;
std::map<uint32_t, int> g_stock_gears;  // by car, from its last fitting
uint32_t g_pending_car = 0;
uint8_t g_pending_mod = 0;

uint32_t CurrentCar() {
  const uint32_t view = Rd32(kGarageView);
  return view ? Rd32(view + 0x8C) : 0;
}

// The levels of overdrive a car takes (6 gears at most: 1 for a 5-speed car, 2 for a 4-speed one), or 0.
int OverdriveLevels(uint32_t car) {
  std::lock_guard<std::mutex> l(g_gear_mu);
  auto it = g_stock_gears.find(car);
  return it == g_stock_gears.end() || it->second < 4 || it->second > 5 ? 0 : 6 - it->second;
}

// The fitted overdrive's level: 0x7F level 1 (a gear more), 0x7E level 2 (two).
int FittedLevel(uint32_t car) {
  const uint8_t b = g_proc->mem.read<uint8_t>(car + 0x17);
  return b == 0x7F ? 1 : b == 0x7E ? 2 : 0;
}

uint32_t GuestString(const char* s) {
  const uint32_t va = HeapAllocProcess(uint32_t(std::strlen(s) + 1), true);
  std::strcpy(reinterpret_cast<char*>(GPtr(va)), s);
  return va;
}

uint32_t GuestText(const char16_t* s) {
  size_t n = 0;
  while (s[n]) n++;
  const uint32_t va = HeapAllocProcess(uint32_t(n + 1) * 2, true);
  std::memcpy(GPtr(va), s, (n + 1) * 2);
  return va;
}

// Calls a lifted function from a hook with the stack arguments `args` (the callee pops them) and keeps the caller's
// eax, ecx, edx.
uint32_t GuestCall(Ctx& c, void (*f)(Ctx&), std::initializer_list<uint32_t> args) {
  const uint32_t eax = c.eax, ecx = c.ecx, edx = c.edx;
  c.esp -= uint32_t(args.size() + 1) * 4;
  uint32_t at = c.esp + 4;
  for (uint32_t a : args) g_proc->mem.write<uint32_t>(at, a), at += 4;
  g_proc->mem.write<uint32_t>(c.esp, 0);  // the return address the callee pops
  f(c);
  const uint32_t r = c.eax;
  c.eax = eax, c.ecx = ecx, c.edx = edx;
  return r;
}

// A window's menu by name (ebx the window).
uint32_t FindMenu(Ctx& c, uint32_t screen, uint32_t name) {
  const uint32_t ebx = c.ebx;
  c.ebx = screen;
  const uint32_t m = GuestCall(c, f_005A9910, {name});
  c.ebx = ebx;
  return m;
}

void Sound(Ctx& c, uint32_t screen, uint32_t name) {
  const uint32_t esi = c.esi, edi = c.edi;
  c.esi = Rd32(screen + 0x40), c.edi = 0x0F;
  GuestCall(c, f_005A86C0, {name, 0x3F800000u});
  c.esi = esi, c.edi = edi;
}
}  // namespace

void f_0048DFE0(Ctx& c) {
  const uint32_t car = c.edi, dyn = c.esi;
  f_0048DFE0_lifted(c);
  uint32_t first = Rd32(dyn + 0x12C);
  const int gears = int((Rd32(dyn + 0x130) - first) / 4);
  {
    std::lock_guard<std::mutex> l(g_gear_mu);
    g_stock_gears[car] = gears;
  }
  const uint8_t mod = g_proc->mem.read<uint8_t>(car + 0x17);
  const int extra = mod == 0x7F ? 1 : mod == 0x7E ? 2 : 0;
  if (!g_overdrive_mod || !extra || gears < 2 || gears + extra > 6) return;
  if (Rd32(dyn + 0x138) - first < 6 * 4) {
    const uint32_t room = GuestCall(c, f_004EEA60, {6 * 4});
    c.esp += 4;  // operator new is cdecl
    if (!room) return;
    std::memcpy(GPtr(room), GPtr(first), size_t(gears) * 4);
    first = room;
    g_proc->mem.write<uint32_t>(dyn + 0x12C, first);
    g_proc->mem.write<uint32_t>(dyn + 0x138, first + 6 * 4);
  }
  for (int n = gears; n < gears + extra; n++) {
    const float last = g_proc->mem.read<float>(first + (n - 1) * 4), before = g_proc->mem.read<float>(first + (n - 2) * 4);
    g_proc->mem.write<float>(first + n * 4, last * last / before);
  }
  g_proc->mem.write<uint32_t>(dyn + 0x130, first + uint32_t(gears + extra) * 4);
}

// tuningmenu's enter (this ecx, the window the argument): the car is rated (fitted) first, then its menu is reset; the
// overdrive item goes when it is not for sale.
void f_0043A890(Ctx& c) {
  static const uint32_t kItem = GuestString("overdrive");
  const uint32_t screen = Rd32(c.esp + 4);
  f_0043A890_lifted(c);
  const uint32_t menu = FindMenu(c, screen, 0x6EE37C /* "menu" */);
  if (!menu) return;
  if (g_overdrive_mod && OverdriveLevels(CurrentCar())) {
    // Its help line (the item's +0x14, UTF-16), else the menu's DETAILS.
    static const uint32_t kHelp = GuestText(u"ADD AN OVERDRIVE: TALLER TOP GEARS FOR MORE TOP SPEED");
    for (uint32_t p = Rd32(menu + 0x11C); p < Rd32(menu + 0x120); p += 4)
      if (strcasecmp(reinterpret_cast<const char*>(GPtr(Rd32(p) + 0x28)), "overdrive") == 0)
        g_proc->mem.write<uint32_t>(Rd32(p) + 0x14, kHelp);
    return;
  }
  const uint32_t ebx = c.ebx, edi = c.edi;
  c.ebx = kItem, c.edi = menu;
  GuestCall(c, f_005AF740, {});
  c.ebx = ebx, c.edi = edi;
}

namespace {
bool g_overdrive_screen = false;  // performtype shows the overdrive's levels

uint32_t SelectedItem(Ctx& c, uint32_t menu) {
  if (!menu) return 0;
  const uint32_t eax = c.eax;
  c.eax = menu;
  const uint32_t item = GuestCall(c, f_005AF7A0, {});
  c.eax = eax;
  return item;
}

const char* Command(uint32_t item) { return reinterpret_cast<const char*>(GPtr(item + 0x28)); }

// A window opened by the window manager (the window's +0x40): mode 5 over this one, 7 in its place.
void Request(uint32_t screen, uint32_t mode, const char* name) {
  const uint32_t host = Rd32(screen + 0x40);
  if (Rd32(host + 0xBC) != 0) return;
  g_proc->mem.write<uint32_t>(host + 0xBC, mode);
  std::strcpy(reinterpret_cast<char*>(GPtr(host + 0xC0)), name);
}

void Return(Ctx& c) {  // the windows' handlers: thiscall, the window the argument, ret 4
  c.eax = 1;
  c.esp += 8;
}
}  // namespace

// tuningmenu's update: SELECT (5) on the overdrive item opens performtype (in place of this window) with its levels.
void f_0043A9C0(Ctx& c) {
  const uint32_t screen = Rd32(c.esp + 4);
  bool overdrive = false;
  if (g_overdrive_mod && Rd32(screen + 0x88) == 5) {
    const uint32_t item = SelectedItem(c, FindMenu(c, screen, 0x6EEC5C /* "Menu" */));
    overdrive = item && strcasecmp(Command(item), "overdrive") == 0;
  }
  f_0043A9C0_lifted(c);  // plays CLICK for an item it does not know
  if (!overdrive || !OverdriveLevels(CurrentCar())) return;
  g_proc->mem.write<uint32_t>(0x714C2C, 9);  // the garage's part kind: the gearbox
  g_overdrive_screen = true;
  Request(screen, 7, "performtype");
}

// performtype's enter for the overdrive: STOCK (with one fitted), LEVEL 1 and LEVEL 2 (the game's texts), a level past
// six gears locked (value -1, the window's "locked" shown), the fitted one ticked (ui_yes), as the game's parts have them.
void f_00441670(Ctx& c) {
  if (!g_overdrive_screen) return f_00441670_lifted(c);
  const uint32_t screen = Rd32(c.esp + 4), car = CurrentCar();
  {
    const uint32_t ecx = c.ecx;
    GuestCall(c, f_005A5D80, {screen});  // the windows' base enter (this ecx)
    c.ecx = ecx;
  }
  const uint32_t menu = FindMenu(c, screen, 0x6EE37C /* "menu" */);
  if (!menu || !car) return Return(c);
  const uint32_t esi = c.esi, edi = c.edi, ebx = c.ebx;
  c.esi = menu;
  GuestCall(c, f_005AEB60, {});  // menu emptied
  const int levels = OverdriveLevels(car), fitted = FittedLevel(car);
  auto add = [&](uint32_t text, uint32_t command) {
    GuestCall(c, f_005AEEA0, {menu, text, 0x3F800000u, command, 0, 0xFFFFFFFFu});
    return Rd32(Rd32(menu + 0x120) - 4);
  };
  if (fitted) add(0x75EF2C /* STOCK */, 0x6EF764 /* "stock" */);
  for (int level = 1; level <= 2; level++) {
    const uint32_t item = add(0x760CF0 + uint32_t(level - 1) * 16 /* LEVEL n */, level == 1 ? 0x6EF75C : 0x6EF754);
    if (level > levels) g_proc->mem.write<float>(item + 0x24, -1.0f);
    if (level == fitted)
      for (uint32_t slot : {item + 0x48, item + 0x4C}) GuestCall(c, f_005A8920, {Rd32(screen + 0x40), 0x6EF734 /* "ui_yes" */, slot});
  }
  c.eax = menu, c.edx = 0;
  GuestCall(c, f_005AF510, {0});  // the first item highlighted
  if (const uint32_t locked = FindMenu(c, screen, 0x6EF960 /* "locked" */)) g_proc->mem.write<uint32_t>(locked + 0x58, 1);
  c.esi = esi, c.edi = edi, c.ebx = ebx;
  Return(c);
}

// performtype's update for the overdrive: SELECT buys a level through buymod ($1500 a gear) or takes the fitted one off
// on STOCK (half its price back); a fitted overdrive goes back to tuningmenu. BACK (12): tuningmenu.
void f_00441910(Ctx& c) {
  if (!g_overdrive_screen) return f_00441910_lifted(c);
  const uint32_t screen = Rd32(c.esp + 4), car = CurrentCar();
  g_proc->mem.write<uint32_t>(0x75E0E8, 2);  // the clouds' mode, as every garage window sets it
  if (g_pending_car) {
    const bool bought = Rd32(kGarageBought) == 1;
    if (bought) {
      g_proc->mem.write<uint8_t>(g_pending_car + 0x17, g_pending_mod);
      g_proc->mem.write<uint32_t>(kGarageBought, 0);
    }
    g_pending_car = 0;
    if (bought) {
      g_overdrive_screen = false;
      Request(screen, 7, "tuningmenu");
      return Return(c);
    }
  }
  const uint32_t item = SelectedItem(c, FindMenu(c, screen, 0x6EEC5C /* "Menu" */));
  const bool locked = item && g_proc->mem.read<float>(item + 0x24) == -1.0f;
  if (const uint32_t l = FindMenu(c, screen, 0x6EF960 /* "locked" */)) g_proc->mem.write<uint32_t>(l + 0x58, locked ? 0 : 1);
  const uint32_t event = Rd32(screen + 0x88);
  if (event == 5 && item && car) {
    const bool stock = strcasecmp(Command(item), "stock") == 0;
    const int level = stock ? 0 : strcasecmp(Command(item), "street") == 0 ? 1 : 2, fitted = FittedLevel(car);
    if (locked || level == fitted || Rd32(Rd32(screen + 0x40) + 0xBC) != 0) {
      Sound(c, screen, 0x6EE9CC /* "NO" */);
    } else {
      Sound(c, screen, 0x6EE084 /* "CLICK" */);
      // buymod pays the price less the refund: half of what the fitted one cost.
      g_proc->mem.write<uint32_t>(kGaragePrice, uint32_t(level * 1500));
      g_proc->mem.write<float>(kGarageRating, 0.0f);
      g_proc->mem.write<float>(kGarageRefund, float(fitted * 750));
      g_proc->mem.write<uint32_t>(kGarageBought, 0);
      g_pending_car = car;
      g_pending_mod = level ? uint8_t(0x80 - level) : g_proc->mem.read<float>(car + 0x5C) != 0.5f ? 1 : 0xFF;
      Request(screen, 5, "buymod");
    }
  } else if (event == 0x0C) {
    Sound(c, screen, 0x6EE084 /* "CLICK" */);
    g_overdrive_screen = false;
    Request(screen, 7, "tuningmenu");
  }
  Return(c);
}

// gearboxmenu's update: buying the gearing writes the gearbox part 1, which would take an overdrive back out.
void f_004427C0(Ctx& c) {
  const uint32_t car = CurrentCar();
  const uint8_t was = car ? g_proc->mem.read<uint8_t>(car + 0x17) : 0;
  f_004427C0_lifted(c);
  if ((was == 0x7E || was == 0x7F) && g_proc->mem.read<uint8_t>(car + 0x17) == 1)
    g_proc->mem.write<uint8_t>(car + 0x17, was);
}

}  // namespace recomp::gen
