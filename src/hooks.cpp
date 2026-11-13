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

// The level a car's overdrive would be (1 or 2), or 0.
int OverdriveLevel(uint32_t car) {
  std::lock_guard<std::mutex> l(g_gear_mu);
  auto it = g_stock_gears.find(car);
  return it == g_stock_gears.end() || it->second < 4 || it->second > 5 ? 0 : 6 - it->second;
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
  if (!g_overdrive_mod || !extra || gears + extra != 6) return;
  if (Rd32(dyn + 0x138) - first < 6 * 4) {
    const uint32_t room = GuestCall(c, f_004EEA60, {6 * 4});
    c.esp += 4;  // operator new is cdecl
    if (!room) return;
    std::memcpy(GPtr(room), GPtr(first), size_t(gears) * 4);
    first = room;
    g_proc->mem.write<uint32_t>(dyn + 0x12C, first);
    g_proc->mem.write<uint32_t>(dyn + 0x138, first + 6 * 4);
  }
  for (int n = gears; n < 6; n++) {
    const float last = g_proc->mem.read<float>(first + (n - 1) * 4), before = g_proc->mem.read<float>(first + (n - 2) * 4);
    g_proc->mem.write<float>(first + n * 4, last * last / before);
  }
  g_proc->mem.write<uint32_t>(dyn + 0x130, first + 6 * 4);
}

// tuningmenu's enter (this ecx, the window the argument): the car is rated (fitted) first, then its menu is reset; the
// overdrive item goes when it is not for sale.
void f_0043A890(Ctx& c) {
  static const uint32_t kItem = GuestString("overdrive");
  const uint32_t screen = Rd32(c.esp + 4);
  f_0043A890_lifted(c);
  const uint32_t menu = FindMenu(c, screen, 0x6EE37C /* "menu" */);
  if (!menu) return;
  if (g_overdrive_mod && OverdriveLevel(CurrentCar())) {
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

// tuningmenu's update: SELECT (5) on the overdrive item puts its price in the garage and opens buymod over the window;
// the next update fits it if bought.
void f_0043A9C0(Ctx& c) {
  const uint32_t screen = Rd32(c.esp + 4);
  if (g_pending_car) {
    if (Rd32(kGarageBought) == 1) {
      g_proc->mem.write<uint8_t>(g_pending_car + 0x17, g_pending_mod);
      g_proc->mem.write<uint32_t>(kGarageBought, 0);
    }
    g_pending_car = 0;
  }
  bool overdrive = false;
  if (g_overdrive_mod && Rd32(screen + 0x88) == 5) {
    const uint32_t menu = FindMenu(c, screen, 0x6EEC5C /* "Menu" */);
    uint32_t item = 0;
    if (menu) {
      const uint32_t eax = c.eax;
      c.eax = menu;
      item = GuestCall(c, f_005AF7A0, {});
      c.eax = eax;
    }
    overdrive = item && strcasecmp(reinterpret_cast<const char*>(GPtr(item + 0x28)), "overdrive") == 0;
  }
  f_0043A9C0_lifted(c);  // plays CLICK for an item it does not know
  if (!overdrive) return;
  const uint32_t car = CurrentCar(), host = Rd32(screen + 0x40);
  const int level = car ? OverdriveLevel(car) : 0;
  const uint8_t mod = uint8_t(0x80 - level);
  if (!level || Rd32(host + 0xBC) != 0) {
    Sound(c, screen, 0x6EE9CC /* "NO" */);
    return;
  }
  // A fitted overdrive comes off for half its price back (buymod pays the price less the refund); the gearbox part
  // goes back to a tuned gearbox (1) if the gearing was moved, else to none.
  const int price = level == 1 ? 1500 : 3000;
  const bool fitted = g_proc->mem.read<uint8_t>(car + 0x17) == mod;
  g_proc->mem.write<uint32_t>(kGaragePrice, fitted ? 0 : uint32_t(price));
  g_proc->mem.write<float>(kGarageRating, 0.0f);
  g_proc->mem.write<float>(kGarageRefund, fitted ? float(price / 2) : 0.0f);
  g_proc->mem.write<uint32_t>(kGarageBought, 0);
  g_proc->mem.write<uint32_t>(host + 0xBC, 5);  // over this window
  std::strcpy(reinterpret_cast<char*>(GPtr(host + 0xC0)), "buymod");
  g_pending_mod = !fitted ? mod : g_proc->mem.read<float>(car + 0x5C) != 0.5f ? 1 : 0xFF;
  g_pending_car = car;
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
