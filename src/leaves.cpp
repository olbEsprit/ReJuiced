// LEAVES effect's particle update (0x5089E0, called by the worker's job 0x509040) and its wind query (0x505760), natively:
// see manifest/juiced.manifest. The calls into the rest of the game are the original's, in its order; the arithmetic and
// the trig are C++ floats. JUICED_NATIVE_LEAVES=0 runs the lifted ones.
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

void f_005089E0_lifted(Ctx& c);
void f_00505760_lifted(Ctx& c);

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

void WF(uint32_t va, float v) { std::memcpy(B() + va, &v, 4); }

// Calls guest code from the stack top `sp` (the callee pops the arguments).
void Call(Ctx& c, uint32_t sp, uint32_t target, uint32_t from, std::initializer_list<uint32_t> args) {
  uint32_t at = sp + 4;
  for (uint32_t a : args) W32(at, a), at += 4;
  W32(sp, 0);
  c.esp = sp;
  Dispatch(c, target, from);
}

// A guest function's ST0 result (a float).
float PopSt0(Ctx& c) {
  const uint32_t bits = x87::StoreF32(c.fpu);
  x87::Pop(c.fpu);
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}

// 0x505760 without its three output pointers (every caller passes 0): the wind at the particle `p` from the group `g`'s
// sources, the vector written to `out` (arg4). `ref` is arg3, a point subtracted from the sources' positions.
void Wind(Ctx& c, uint32_t sp, uint32_t g, uint32_t p, uint32_t ref, uint32_t out) {
  const uint32_t qb = sp - 0x40, vb = sp - 0x30, csp = sp - 0x80;
  const float zero = F32(0x6C9E1C), one = F32(0x6FE638);
  const float K1 = F32(0x7246E4), K2 = F32(0x6FE8BC), K3 = F32(0x6FE718), K4 = F32(0x6FE8B0), K5 = F32(0x7246E0);
  const float K6 = F32(0x6FE9CC), K7 = F32(0x6FE7CC), K8 = F32(0x6FEC3C);
  const float px = F32(p), py = F32(p + 4), pz = F32(p + 8);
  float ax = F32(g + 8), ay = F32(g + 0xC), az = F32(g + 0x10);
  const uint32_t aw = U32(g + 0x14);

  auto count = [&] { return uint32_t(int32_t(U32(g + 0x20) - U32(g + 0x1C)) >> 2); };
  if (count() != 0) {
    for (uint32_t i = 0; i < count(); i++) {
      const uint32_t obj = U32(U32(g + 0x1C) + i * 4), s = U32(obj) + 4;
      if (U32(s + 0x88) != 0) {
        c.ecx = s;
        Call(c, csp, 0x5341C0, 0x5057DB, {});
      }
      const float dx = px - F32(s + 0x40), dy = py - F32(s + 0x50), dz = pz - F32(s + 0x60);
      const float d2 = (dx * dx + dz * dz) + dy * dy;
      const float d2k = d2 * K3;
      const float r2 = K1 * K1;
      const float R = K2 * r2;
      if (!(d2k < R)) continue;
      float T = d2k - r2;
      if (T < one) T = one;
      const float ox = F32(obj + 0x14), oy = F32(obj + 0x18), oz = F32(obj + 0x1C);
      const float opx = ox - F32(ref), opy = oy - F32(ref + 4), opz = oz - F32(ref + 8);
      const float s2 = (opx * opx + opz * opz) + opy * opy;
      const float dist = std::sqrt(s2);
      float wt = dist / T;
      wt = wt * K4;
      if (wt > K5) wt = K5;
      const float dxw = dx * wt, dyw = dy * wt, dzw = wt * dz;
      ax = dxw + ax;
      ay = dyw + ay;
      az = dzw + az;
      if (!(F32(p + 0x20) < zero)) continue;

      c.ecx = s;
      Call(c, csp, 0x5341C0, 0x5059BF, {});
      const float n3c = -F32(s + 0x3C), n4c = -F32(s + 0x4C), n5c = -F32(s + 0x5C);
      const float y38 = F32(s + 0x38) * K6, y48 = F32(s + 0x48) * K6, y58 = F32(s + 0x58) * K6;
      const float vx = y38 + n3c, vy = y48 + n4c, vz = y58 + n5c;
      c.ecx = s;
      Call(c, csp, 0x5341C0, 0x505A43, {});
      const float tx = F32(s + 0x40), ty = F32(s + 0x50), tz = F32(s + 0x60);
      const float vxk = vx * K1, vyk = vy * K1, vzk = vz * K1;
      float qx = vxk + tx, qy = vyk + ty, qz = vzk + tz;
      const float ex = px - qx, ey = py - qy, ez = pz - qz;
      const float dot = (ex * vx + ez * vz) + ey * vy;
      if (!(dot > zero)) continue;

      const float x34 = F32(s + 0x34), x44 = F32(s + 0x44), x54 = F32(s + 0x54);
      const float side = (ex * x34 + x54 * ez) + x44 * ey;
      const float sgn = side < zero ? one : -one;
      const float c38 = F32(s + 0x38) * K7, c48 = F32(s + 0x48) * K7, c58 = F32(s + 0x58) * K7;
      qx = c38 + qx;
      qy = c48 + qy;
      qz = c58 + qz;
      const float ux = (x34 * K8) * sgn, uy = (x44 * K8) * sgn, uz = (x54 * K8) * sgn;
      qx = ux + qx;
      qy = uy + qy;
      qz = uz + qz;
      WF(qb, qx), WF(qb + 4, qy), WF(qb + 8, qz);
      WF(vb, vx), WF(vb + 4, vy), WF(vb + 8, vz);
      const float vox = F32(obj + 0x14), voy = F32(obj + 0x18), voz = F32(obj + 0x1C);
      const float n2 = (vox * vox + voy * voy) + voz * voz;
      const float n = std::sqrt(n2);
      uint32_t nb, sb;
      std::memcpy(&nb, &n, 4), std::memcpy(&sb, &sgn, 4);
      c.ebx = qb, c.esi = vb, c.edi = p;
      Call(c, csp, 0x505420, 0x505C31, {nb, sb});
      PopSt0(c);
    }
  }

  const uint32_t g2c = U32(g + 0x2C);
  const float f = F32(g2c + 0xC0);
  const float arg1 = f * F32(0x7246D8) + px * F32(0x6FEC40);
  const float c1 = std::cos(arg1);
  const float arg2 = f * F32(0x7246D4) + pz * F32(0x6FEC38);
  const float s1 = std::sin(arg2);
  const float amp = F32(g + 0x18);
  const float c1amp = c1 * amp;
  const float zamp = zero * amp;
  const float samp = s1 * amp;
  ax = c1amp + ax;
  ay = zamp + ay;
  const float azo = samp + az;
  if (out != 0) {
    WF(out, ax), WF(out + 4, ay), WF(out + 8, azo);
    W32(out + 0xC, aw);
  }
}

// The effect's per-particle update (this = eax, the float step the argument, ret 4).
void LeavesUpdate(Ctx& c) {
  const uint32_t entry_esp = c.esp, self = c.eax;
  const float dt = F32(entry_esp + 4);
  const uint32_t ebx = c.ebx, esi = c.esi, edi = c.edi, ebp = c.ebp;
  if (!(dt <= 0.0f)) {
    // The original's frame (m = its esp after `sub esp, 0x110`): the vectors the callees fill or read.
    const uint32_t m = entry_esp - 0x110;
    const uint32_t buf1 = m + 0x90, zero3 = m + 0xA0, out4 = m + 0x40, buf2 = m + 0x10, stale = m + 0x8C;
    const uint32_t sp = (entry_esp - 0x300) & ~15u;
    const uint32_t sys = U32(self + 0x48);
    const float zero = F32(0x6C9E1C), one = F32(0x6FE638), half = F32(0x6FE6E8), quarter = F32(0x6FEB14);
    const float K17 = F32(0x6FE71C), K18 = F32(0x6FEAB0), two = F32(0x6FE7C8);

    c.edi = self, c.esi = buf1;
    Call(c, sp, 0x48F640, 0x508A06, {});
    c.eax = self + 0xD4, c.ecx = buf1;
    Call(c, sp, 0x50CAC0, 0x508A18, {0x43480000});

    auto rand_f = [&](float k) {
      Call(c, sp, 0x5C9ECC, 0x508D51, {});
      return float(int32_t(c.eax)) * k;
    };
    auto hit = [&](uint32_t p, uint32_t from) {
      c.eax = p, c.ecx = sys;
      Call(c, sp, 0x504370, from, {p + 0x48, 0});
      return c.eax != 0;
    };
    auto far_away = [&](uint32_t p) {
      W32(p, 0x49742400), W32(p + 4, 0x49742400), W32(p + 8, 0x49742400);
      W32(p + 0xC, U32(stale));
    };
    auto respawn = [&](uint32_t p) {
      const float a = rand_f(K17);
      const float b = rand_f(K18);
      const float sc = F32(self + 0x60);
      const float bx = (b - quarter) * sc;
      const float a2 = a * half;
      const float a3 = a2 - quarter;
      const float ay = a3 * sc;
      const float hz = sc * half;
      W32(p + 0xC, U32(buf2 + 0xC));
      W32(p + 0x24, 0);
      W32(p + 0x20, 0xBF800000);
      const float nx = ay + F32(buf2), ny = hz + F32(buf2 + 4), nz = F32(buf2 + 8) + bx;
      WF(p, nx), WF(p + 4, ny), WF(p + 8, nz);
    };

    for (uint32_t i = 0, off = 0; U32(self + 0x5C) != 0; i++, off += 0xB4) {
      const uint32_t p = U32(self + 0xCC) + off;
      auto Pf = [p](uint32_t o) { return F32(p + o); };
      auto Pw = [p](uint32_t o, float v) { WF(p + o, v); };
      if (Pf(0x10) >= zero) {
        W32(out4, 0), W32(out4 + 4, 0), W32(out4 + 8, 0);
        W32(zero3, 0), W32(zero3 + 4, 0), W32(zero3 + 8, 0);
        Wind(c, sp, U32(sys + 0x7C), p, zero3, out4);

        const float k = one - F32(self + 0xA4);
        float v4c = Pf(0x4C) * k, v50 = Pf(0x50) * k, v54 = Pf(0x54) * k;
        Pw(0x4C, v4c), Pw(0x50, v50), Pw(0x54, v54);
        float s0 = zero;
        if (Pf(4) > Pf(0x48)) {
          const float x = F32(self + 0xA4);
          const float oxx = F32(out4) * x, oyx = F32(out4 + 4) * x, ozx = F32(out4 + 8) * x;
          v4c = oxx + v4c, v50 = oyx + v50, v54 = ozx + v54;
          Pw(0x4C, v4c), Pw(0x50, v50), Pw(0x54, v54);
          const float vy = v50 - F32(self + 0xAC);
          const float tab = F32(0x756CC0 + i * 4);
          const float t1 = F32(0x6FEAA0) - tab;
          const float t2 = t1 * F32(0x724700);
          Pw(0x50, vy - t2);
          s0 = tab;
        }
        const float a1 = Pf(0x5C) * s0, a2 = Pf(0x60) * s0, a3 = s0 * Pf(0x64);
        const float wx = Pf(0x4C) + a1, wy = Pf(0x50) + a2, wz = Pf(0x54) + a3;
        const float dx3 = wx * dt, dy3 = wy * dt, dz3 = wz * dt;
        Pw(0, dx3 + Pf(0)), Pw(4, dy3 + Pf(4)), Pw(8, dz3 + Pf(8));

        if (Pf(0x20) >= zero) {
          const float ang = (dt * Pf(0x24)) * F32(0x6FE8D4);
          const float a0 = ang * Pf(0x1C);
          const float sn = std::sin(Pf(0x20));
          const float s_a = sn * a0;
          const float r1x = Pf(0x28) * s_a, r1y = Pf(0x2C) * s_a, r1z = Pf(0x30) * s_a;
          const float cs = std::cos(Pf(0x20));
          const float c_a = cs * a0;
          const float rx = Pf(0x38) * c_a + r1x, ry = Pf(0x3C) * c_a + r1y, rz = Pf(0x40) * c_a + r1z;
          Pw(0x20, ang + Pf(0x20));
          const float p24 = Pf(0x24) * F32(0x6FE708);
          Pw(0x24, p24);
          if (p24 < F32(0x6FE898)) W32(p + 0x20, 0xBF800000);
          Pw(0, rx + Pf(0)), Pw(4, ry + Pf(4)), Pw(8, rz + Pf(8));
        }

        c.edi = self, c.esi = buf2;
        Call(c, sp, 0x48F640, 0x508D15, {});
        const float dxs = Pf(0) - F32(buf2), dzs = Pf(8) - F32(buf2 + 8);
        const float near2 = dxs * dxs + dzs * dzs;
        bool spawn = near2 > F32(self + 0xB4);
        if (!spawn) spawn = rand_f(K17) < F32(U32(self + 0x40) + 0xC4);
        if (spawn) {
          if (U32(self + 4) != 0) {
            W32(p + 0x10, 0xBF800000);
          } else {
            respawn(p);
          }
        }
        // ... down to the sphere's growth below, as the original's tail.
        {
          const float mx = F32(p + 0x8C) - Pf(0), mz = F32(p + 0x94) - Pf(8);
          const float moved = mz * mz + mx * mx;
          if (moved > F32(0x7246FC)) {
            if (!hit(p, 0x508E7A)) {
              respawn(p);
              if (!hit(p, 0x508F38)) far_away(p);
            }
            Pw(0x48, Pf(0x48) + two);
            if (const uint32_t hm = U32(sys + 0x140)) {
              c.ecx = hm, c.edx = p;
              Call(c, sp, 0x505100, 0x508F73, {});
              Pw(0xB0, PopSt0(c));
            }
            W32(p + 0x8C, U32(p)), W32(p + 0x90, U32(p + 4)), W32(p + 0x94, U32(p + 8)), W32(p + 0x98, U32(p + 0xC));
          }
        }
        if (Pf(4) < Pf(0x48)) {
          if (!hit(p, 0x508FB5)) far_away(p);
          const float ny = Pf(0x48) + two;
          Pw(0x48, ny), Pw(4, ny);
          W32(p + 0x4C, 0), W32(p + 0x50, 0), W32(p + 0x54, 0);
          W32(p + 0x58, U32(stale));
        }
        W32(sp - 4, 0);
        c.ecx = self + 0xD4, c.edx = p;
        Call(c, sp, 0x50CB40, 0x508FFF, {});
      }
      if (!(i + 1 < U32(self + 0x5C))) break;
    }
    WF(self + 0xE4, F32(self + 0x64) + F32(self + 0xE4));
  }
  c.ebx = ebx, c.esi = esi, c.edi = edi, c.ebp = ebp;
  c.esp = entry_esp + 8;  // ret 4
}

void LeavesWind(Ctx& c) {
  const uint32_t entry_esp = c.esp;
  const uint32_t ebx = c.ebx, esi = c.esi, edi = c.edi, ebp = c.ebp;
  Wind(c, (entry_esp - 0x300) & ~15u, U32(entry_esp + 4), U32(entry_esp + 8), U32(entry_esp + 0xC), U32(entry_esp + 0x10));
  c.ebx = ebx, c.esi = esi, c.edi = edi, c.ebp = ebp;
  c.esp = entry_esp + 0x20;  // ret 0x1c
}

bool Native() {
  static const bool native = [] {
    const char* e = std::getenv("JUICED_NATIVE_LEAVES");
    return !e || std::strcmp(e, "0") != 0;
  }();
  return native;
}

}  // namespace

void f_005089E0(Ctx& c) {
  if (Native()) return LeavesUpdate(c);
  f_005089E0_lifted(c);
}

void f_00505760(Ctx& c) {
  // The three optional outputs (args 5..7) no caller uses: those go to the lifted one.
  if (Native() && U32(c.esp + 0x14) == 0 && U32(c.esp + 0x18) == 0 && U32(c.esp + 0x1C) == 0) return LeavesWind(c);
  f_00505760_lifted(c);
}

}  // namespace recomp::gen
