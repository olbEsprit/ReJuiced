#include <cstring>

#include "runtime/hle/kernel32_state.h"
#include "runtime/hle/xinput.h"
#include "runtime/runtime.h"

namespace recomp::gen {

void f_00510090_lifted(Ctx& c);

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

}  // namespace recomp::gen
