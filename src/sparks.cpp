// SPARKS effect Update (0x509720, vtable 0x6F6408 +0x14), natively: see manifest/juiced.manifest. The calls into
// the rest of the game and the D3D9 calls are the original's, in its order; the per-particle loop and the RNG
// (0x5095E0, table 0x75A2B8) are C++ with the same integer behaviour. JUICED_NATIVE_SPARKS=0 runs the lifted one.
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "x86/runtime.h"

namespace recomp {
extern uint8_t* g_guest_base;
void Dispatch(Ctx& c, uint32_t target, uint32_t va);
}

namespace recomp::gen {

void f_00509720_lifted(Ctx& c);

namespace {

uint8_t* B() { return g_guest_base; }

uint32_t U32(uint32_t va) {
  uint32_t v;
  std::memcpy(&v, B() + va, 4);
  return v;
}

float F32(uint32_t va) {
  float v;
  std::memcpy(&v, B() + va, 4);
  return v;
}

void W32(uint32_t va, uint32_t v) { std::memcpy(B() + va, &v, 4); }

// Calls guest code from the stack top `sp` (the callee pops the arguments).
void Call(Ctx& c, uint32_t sp, uint32_t target, uint32_t from, std::initializer_list<uint32_t> args) {
  uint32_t at = sp + 4;
  for (uint32_t a : args) W32(at, a), at += 4;
  W32(sp, 0);
  c.esp = sp;
  Dispatch(c, target, from);
}

// _ftol under RC=chop: fistp's out-of-range result is 0x80000000.
inline int32_t Ftol(float x) { return x >= -2147483648.0f && x < 2147483648.0f ? int32_t(x) : INT32_MIN; }

// The CRT's modf (0x5CAC80) of a float: the fraction.
inline float ModfFrac(float x) {
  double ip;
  return float(std::modf(double(x), &ip));
}

inline uint32_t Rol(uint32_t v, int n) { return v << n | v >> (32 - n); }

void SparksUpdate(Ctx& c) {
  const uint32_t entry_esp = c.esp, arg0 = U32(c.esp + 4), self = c.ecx;
  const uint32_t ebx = c.ebx, esi = c.esi, edi = c.edi, ebp = c.ebp;
  // The original's frame: the 16-byte buffer 0x48F640 fills, then the calls' stacks below it.
  const uint32_t buf = (entry_esp - 0x100) & ~15u, sp = buf - 0x20;

  c.ebx = self, c.esi = buf, c.edi = self;
  Call(c, sp, 0x48F640, 0x50973A, {});
  c.esi = arg0, c.ecx = self;
  Call(c, sp, 0x5065B0, 0x509747, {buf});
  if (c.eax != 0) {
    c.esi = U32(self + 0x9C), c.edi = 0;
    Call(c, sp, 0x523A10, 0x50975C, {});

    // 0x509650 with ecx = 0: reseed, then one value drawn and dropped (the loop below draws from this state).
    uint32_t tab[17];
    int32_t ia = 0, ib = 10;
    for (uint32_t k = 0, x = 0; k < 17; k++) x = 1 - x * 0x53A9B4FBu, tab[k] = x;
    const float k1 = F32(0x6FE638);
    auto rng = [&]() -> float {
      uint32_t v = Rol(tab[ia], 9) + Rol(tab[ib], 13);
      tab[ia] = v;
      if (--ia < 0) ia = 0x10;
      if (--ib < 0) ib = 0x10;
      const uint32_t bits = v >> 9 | 0x3F800000u;
      float f;
      std::memcpy(&f, &bits, 4);
      return f - k1;
    };
    rng();

    const uint32_t cam48 = U32(self + 0x48), cam40 = U32(self + 0x40);
    const float in70 = F32(cam48 + 0xF0), in74 = F32(cam48 + 0xF4), in78 = F32(cam48 + 0xF8);
    const float in80 = F32(cam40 + 0xC8), in94 = F32(cam40 + 0xCC), in3c = F32(cam40 + 0xD0), in38 = F32(cam40 + 0xD4);
    const float inbc = F32(cam40 + 0x104), ine0 = F32(cam40 + 0xF4), ine4 = F32(cam40 + 0xF8), ine8 = F32(cam40 + 0xFC);
    const float ina8 = F32(self + 0x70), in30 = F32(self + 0x3C);
    const float in84 = F32(self + 0x7C), in88 = F32(self + 0x80), in8c = F32(self + 0x84), in90 = F32(self + 0x88);
    const float in98 = F32(self + 0x8C), in9c = F32(self + 0x90), ina0 = F32(self + 0x94), ina4 = F32(self + 0x98);
    const float th60 = F32(self + 0x60), th64 = F32(self + 0x64), th68 = F32(self + 0x68);
    const float inc0 = F32(buf), inc4 = F32(buf + 4), inc8 = F32(buf + 8);
    const float K6fe638 = k1, K6fe6e8 = F32(0x6FE6E8), K6fe8cc = F32(0x6FE8CC), K6feb14 = F32(0x6FEB14), K6c9e1c = F32(0x6C9E1C);
    const uint32_t K718210 = U32(0x718210), K718214 = U32(0x718214), K718218 = U32(0x718218), K71821c = U32(0x71821C);
    const uint32_t K718220 = U32(0x718220), K718224 = U32(0x718224), K718228 = U32(0x718228), K71822c = U32(0x71822C);

    const uint32_t vbw = U32(self + 0xA4);
    W32(vbw + 0x24, 1);
    c.esi = vbw, c.edi = U32(self + 0xA0);
    Call(c, sp, 0x51BF30, 0x50987B, {0});
    const uint32_t vb = c.eax, n = U32(self + 0x5C);
    int32_t last_ftol = 0;
    for (uint32_t i = 0; i < n; i++) {
      uint8_t* const q = B() + vb + i * 0x60;
      auto VF = [q](int o, float x) { std::memcpy(q + o, &x, 4); };
      auto V32 = [q](int o, uint32_t x) { std::memcpy(q + o, &x, 4); };
      float t1 = rng();
      float t2 = t1 + K6fe638;
      float t3 = t2 * inbc;
      float t4 = t3;
      float t5 = t4 * in80;
      float t6 = ina8 / t5;
      float t7 = ModfFrac(t6);
      float t8 = t7;
      float t9 = K6fe638 - t8;
      float t10 = in90 * t9;
      float t11 = ina4 * t8;
      float t12 = t10 + t11;
      int32_t t13 = Ftol(t12);
      float t14 = in84 * t9;
      float t15 = in98 * t8;
      float t16 = t14 + t15;
      int32_t t17 = Ftol(t16);
      float t18 = in88 * t9;
      float t19 = in9c * t8;
      float t20 = t18 + t19;
      int32_t t21 = Ftol(t20);
      float t22 = in8c * t9;
      float t23 = ina0 * t8;
      float t24 = t22 + t23;
      int32_t t25 = Ftol(t24);
      const uint32_t col = uint32_t(uint8_t(t25)) | uint32_t(uint8_t(t21)) << 8 | uint32_t(uint8_t(t17)) << 16 | uint32_t(uint8_t(t13)) << 24;
      last_ftol = t25;
      float t26 = rng();
      float t27 = t26 - K6fe6e8;
      float t28 = t27 * ine8;
      float t29 = t28 + t28;
      float t30 = t29;
      float t31 = rng();
      float t32 = t31 * ine4;
      float t33 = t32;
      float t34 = rng();
      float t35 = t34 - K6fe6e8;
      float t36 = t35 * ine0;
      float t37 = t36 + t36;
      float t38 = t37 * in30;
      float t39 = t33 * in30;
      float t40 = t30 * in30;
      float t41 = t40;
      float t42 = t38 + th60;
      float t43 = t42;
      float t44 = t39 + th64;
      float t45 = t44;
      float t46 = t41 + th68;
      float t47 = t46;
      float t48 = rng();
      float t49 = t48 + K6fe638;
      float t50 = t49 * in30;
      float t51 = t50 * in94;
      float t52 = t51;
      float t53 = t8 + in3c;
      float t54 = t53;
      float t55 = rng();
      float t56 = t55 + K6feb14;
      float t57 = t56 * in38;
      float t58 = t57;
      float t59 = t8 * K6c9e1c;
      float t60 = t58 * t8;
      float t61 = t60;
      float t62 = t59;
      float t63 = t59 * t8;
      float t64 = t63;
      float t65 = t61 * t8;
      float t66 = t65;
      float t67 = t62 * t8;
      float t68 = t67;
      float t69 = t43 * t4;
      float t70 = t69;
      float t71 = t45 * t4;
      float t72 = t47 * t4;
      float t73 = t72;
      float t74 = t70 * t8;
      float t75 = t74;
      float t76 = t71 * t8;
      float t77 = t76;
      float t78 = t72 * t8;
      float t79 = inc0 + t75;
      float t80 = inc4 + t77;
      float t81 = t80;
      float t82 = inc8 + t78;
      float t83 = t82;
      float t84 = t79 + t64;
      float t85 = t84;
      float t86 = t81 + t66;
      float t87 = t86;
      float t88 = t83 + t68;
      float t89 = t88;
      float t90 = t54 * K6c9e1c;
      float t91 = t58 * t54;
      float t92 = t91;
      float t93 = t90;
      float t94 = t90 * t54;
      float t95 = t94;
      float t96 = t92 * t54;
      float t97 = t96;
      float t98 = t93 * t54;
      float t99 = t98;
      float t100 = t69;
      float t101 = t100 * t54;
      float t102 = t101;
      float t103 = t71 * t54;
      float t104 = t103;
      float t105 = t73 * t54;
      float t106 = t102 + inc0;
      float t107 = t104 + inc4;
      float t108 = t107;
      float t109 = t105 + inc8;
      float t110 = t109;
      float t111 = t106 + t95;
      float t112 = t111;
      float t113 = t108 + t97;
      float t114 = t113;
      float t115 = t110 + t99;
      float t116 = t115;
      float t117 = t112 - t85;
      float t118 = t117;
      float t119 = t114 - t87;
      float t120 = t119;
      float t121 = t116 - t89;
      float t122 = t121;
      float t123 = t118 * t118;
      float t124 = t122 * t122;
      float t125 = t123 + t124;
      float t126 = t120 * t120;
      float t127 = t125 + t126;
      float t128 = t127;
      float t129 = std::sqrt(t128);
      float t130 = t129 + K6fe8cc;
      float t131 = K6fe638 / t130;
      float t132 = t118 * t131;
      float t133 = t120 * t131;
      float t134 = t122 * t131;
      float t135 = in74 * t134;
      float t136 = t133 * in78;
      float t137 = t135 - t136;
      float t138 = t137;
      float t139 = in78 * t132;
      float t140 = t134 * in70;
      float t141 = t139 - t140;
      float t142 = t141;
      float t143 = t133 * in70;
      float t144 = in74 * t132;
      float t145 = t143 - t144;
      float t146 = t138 * t52;
      float t147 = t142 * t52;
      float t148 = t147;
      float t149 = t145 * t52;
      float t150 = t149;
      float t151 = t85 - t118;
      float t152 = t151 + t146;
      VF(0, t152);
      float t153 = t87 - t120;
      float t154 = t153 + t148;
      VF(4, t154);
      float t155 = t89 - t122;
      float t156 = t155 + t150;
      VF(8, t156);
      V32(16, K718220);
      float t157 = t118 + t146;
      V32(20, K718224);
      float t158 = t157 + t85;
      V32(12, col);
      VF(24, t158);
      float t159 = t148 + t120;
      float t160 = t159 + t87;
      VF(28, t160);
      float t161 = t150 + t122;
      float t162 = t161 + t89;
      VF(32, t162);
      V32(40, K718228);
      float t163 = t112 - t118;
      V32(44, K71822c);
      V32(36, col);
      float t164 = t163 - t146;
      VF(48, t164);
      float t165 = t114 - t120;
      float t166 = t165 - t148;
      VF(52, t166);
      float t167 = t116 - t122;
      float t168 = t167 - t150;
      VF(56, t168);
      V32(64, K718210);
      float t169 = t118 + t112;
      V32(68, K718214);
      V32(60, col);
      float t170 = t169 - t146;
      VF(72, t170);
      float t171 = t120 + t114;
      float t172 = t171 - t148;
      VF(76, t172);
      float t173 = t122 + t116;
      float t174 = t173 - t150;
      VF(80, t174);
      V32(88, K718218);
      V32(92, K71821c);
      V32(84, col);
    }
    if (n) W32(0x70302F, uint32_t(last_ftol));
    std::memcpy(B() + 0x75A2B8, tab, sizeof tab);
    W32(0x75A2FC, uint32_t(ia));
    W32(0x75A300, uint32_t(ib));

    // Unlock, SetStreamSource, SetIndices, DrawIndexedPrimitive (TRIANGLELIST, N * 4 vertices, N * 2 primitives).
    c.ebx = self;
    const uint32_t vbo = U32(self + 0xA4);
    if (U32(vbo + 0x20) != 0) {
      if (const uint32_t o = U32(vbo)) {
        Call(c, sp, U32(U32(o) + 0x30), 0x509DF5, {o});
        W32(vbo + 0x20, 0);
      }
    }
    const uint32_t ib_obj = U32(U32(self + 0xA8)), dev = U32(U32(self + 0x44) + 0x7D0);
    Call(c, sp, U32(U32(dev) + 0x190), 0x509E37, {dev, 0, U32(U32(self + 0xA4)), 0, 0x18});
    Call(c, sp, U32(U32(dev) + 0x1A0), 0x509E41, {dev, ib_obj});
    Call(c, sp, U32(U32(dev) + 0x148), 0x509E5C, {dev, 4, 0, 0, n * 4, 0, n * 2});
  }
  c.ebx = ebx, c.esi = esi, c.edi = edi, c.ebp = ebp;
  c.eax = 1;
  c.esp = entry_esp + 8;  // ret 4
}

}  // namespace

void f_00509720(Ctx& c) {
  static const bool native = [] {
    const char* e = std::getenv("JUICED_NATIVE_SPARKS");
    return !e || std::strcmp(e, "0") != 0;
  }();
  if (native) return SparksUpdate(c);
  f_00509720_lifted(c);
}

}  // namespace recomp::gen
