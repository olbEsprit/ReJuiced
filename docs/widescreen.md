# JuicedWidescreenFix.asi — static audit (game v1.00)

Source: `JuicedWidescreenFix.asi` from the 4D-Eclipse release V1.0 (PE32 DLL, MSVC 14.51, 226 KB, built 2026-07-07, closed source). Nothing was run.

## Summary
The whole plugin is one init routine `0x10003020` (DLL image base 0x10000000), called from the DllMain wrapper `0x100010d0` on DLL_PROCESS_ATTACH. No code caves, no hooks, no jmp/call injection. It only writes **4 floats (4 bytes each)**, all in the game's own data (3 in `.rdata`, 1 in `.data`). A search of the DLL for every immediate/absolute operand in 0x400000..0x7FFFFF found exactly these addresses: 0x72477C, 0x72778C (v1.01 only), 0x6FEA14, 0x701BE0 (v1.01 only), 0x6FE670, 0x6FEAF8.

## Patch table (v1.00)
Variables: W, H = HKCU\Software\THQ\Juiced RezWidth/RezHeight (DWORD); r = (float)W / (float)H (float division); ini values as in Widescreen.ini.

| address | size | original (clean Juiced.exe) | new value | purpose (guess marked) |
|---|---|---|---|---|
| 0x72477C (.data) | 4 f32 | 1.15 (3333933F) | `aspect - ExtraWide`; aspect per section 5. Written if AspectRatioMode is 1 or 2 | Horizontal-FOV / aspect multiplier. Read at 0x55222E `fmul [0x72477C]` into camera field `[ecx+0xB4]`, only when game flag `[0x75E0DC]` (the game's own widescreen option) is non-zero |
| 0x6FEA14 (.rdata) | 4 f32 | 1.333 (BE9FAA3F) | FOV value, section 5. Written if FOVMode is 1 or 2 and version=1.00 | Base FOV multiplier. Read at 0x552198 `fmul [0x6FEA14]` into `[ecx+0xA8]`, same `[0x75E0DC]` flag |
| 0x6FE670 (.rdata) | 4 f32 | 0.0015625 = 1/640 (CDCCCC3A) | kerning value, section 5. Written if DigitalFontKerningMode is 1 or 2 and version=1.00 | 2D HUD horizontal scale (guess, "digital font kerning"). Read in 5 places (0x4E01DA, 0x4E01F4, 0x4F820A, 0x5B17FE (`W_screen * ... * k`), 0x5B1854) |
| 0x6FEAF8 (.rdata) | 4 f32 | 0.975 (9A99793F) | acceleration value, section 5, clamped to [0, 0.97]. Written if AccelerationEffectMode is 1 or 2 and version=1.00 | Camera acceleration effect (guess). Read at 0x40621C: `x = (x - [0x6FE638]) * k + [0x6FE638]` on `[ecx+0xB8]` |
| 0x72778C (.data) | 4 f32 | 0.0 in v1.00 (only a version probe) | same aspect value, **only if the v1.01 branch is taken** | v1.01 twin of 0x72477C |
| 0x701BE0 (.data) | 4 f32 | v1.00: pointer-like 0x006BC9B0 (not a float, unrelated) | same FOV value, **only in v1.01** | v1.01 twin of 0x6FEA14 |

Kerning (0x6FE670) and acceleration (0x6FEAF8) are patched only on v1.00 — the plugin has no v1.01 addresses for them.
Note: the base source (TFP-dev) uses 0x6FEAF0 as the v1.00 FOV address; this ASI uses 0x6FEA14 instead. At 0x6FEAF0 the exe holds 1.1624, at 0x6FEA14 it holds 1.333, which matches the ini default CustomFOV=1.333. So the build differs from the base source here.

## 1. Memory access mechanism
In `0x10003020`:
1. `GetModuleHandleW(NULL)` ([0x10028014]) -> game image base. Walk the section table; for sections named `.rdata` (8-byte memcmp with "`.rdata`"):
   for each address range in the section (`VirtualQuery` [0x10028018]): if State==MEM_COMMIT(0x1000) and Type has MEM_IMAGE(0x1000000) and Protect has none of the write bits (`& 0xCC == 0`), call `VirtualProtect` ([0x10028024]) with PAGE_EXECUTE_READWRITE (0x40) if Protect had exec bits (0x30), else PAGE_READWRITE (4). The old protection, size and base are saved in a linked list.
2. After all patches the list is walked and `VirtualProtect` restores the original protections (0x10003C40 loop).
   The `.data` addresses are writable already, so only `.rdata` needs this.
   Note: the .asi tests the section by name only, so it unprotects every `.rdata` section found; there is just one in this exe (0x6C9000, size 0x3710A, covers 0x6FE670, 0x6FEA14, 0x6FEAF8).

## 2. Inputs
- Registry: `RegOpenKeyExW(HKEY_CURRENT_USER (0x80000001), L"Software\THQ\Juiced\", 0, KEY_READ (0x20119))`, then `RegGetValueW(..., L"RezWidth"/L"RezHeight", RRF_RT_REG_DWORD (0x10), ...)` -> W (`[ebp-0x1C]`), H (`[ebp-0x18]`) (0x10003254-0x100032B7). If key is missing or W==0 or H==0 -> jump to cleanup, **nothing is patched**.
- INI: `"Widescreen.ini"` is opened by a relative name (no module-path code in the init routine; `GetModuleFileNameW` is used only in CRT code), section `[WIDESCREEN]`. Relative resolution means current directory at load time; UNCERTAIN whether the ASI loader sets cwd to `scripts\` (the ini ships there). Parsing is a custom ini reader; integers via `0x10012C0A` (atoi-like), floats via `0x10011C4A` (atof-like).
- `Enable`: if 0 (or missing -> 0) -> exit, nothing patched (0x10003346).
- Key -> variable map: AspectRatioMode -> `[0x100368C0]`; CustomAspectRatio -> `[ebp-0xA8]`; FOVMode -> `[0x100368BC]`; CustomFOV -> `[ebp-0xA0]`; AccelerationEffectMode -> `[0x100368B8]`; CustomAccelerationEffect -> `[ebp-0x14]`; DigitalFontKerningMode -> `[0x100368B4]`; CustomDigitalFontKerning -> `[ebp-0xA4]`; PortraitMode -> `edi`/`[ebp-0x98]`; ExtraWide -> `[ebp-0x9C]`.

## 3. Version detection
In block 0x10003B0D, **only if AspectRatioMode is 1 or 2**:
- `float [0x72477C] == 1.15f` (exact compare, `ucomiss`) -> version 1.00 (internal code 0x64); writes aspect there.
- else `float [0x72778C] == 1.15f` -> version 1.01 (code 0x65); writes aspect there.
- else `MessageBoxA(NULL, "Unsupported version detected!", "Juiced Widescreen Fix", MB_ICONINFORMATION|0x40)`, no further patches.
Every later write (FOV, kerning, acceleration) is conditioned on the code being 0x64 or 0x65. **Consequence (quirk): with AspectRatioMode=0 nothing at all is patched**, including FOV/kerning/accel, because version detection lives in the aspect block. The exact-1.15 check also means a second load / already patched exe is treated as unsupported.
Original v1.00 exe: 0x72477C = 1.15, 0x72778C = 0.0 -> detected as 1.00.

## 4. Code caves / injected code
None. No jmp/call patching, no trampolines. Only data floats are overwritten.

## 5. Value formulas (all floats, mode 1 = Auto)
Constants from DLL .rdata (0x10032264..0x100322D4): 1.15, 1.3, 1.3333333, 1.5625, 1.6, 1.6666666, 1.7777778, 2.3703704, 9.0, 16.0.
Matching is **exact float equality** on r = (float)W/(float)H (only exact 4:3, 25:16, 16:10, 15:9, 16:9, 64:27 hit the table; 1280x1024 etc. fall to the formula).

### Aspect (AspectRatioMode)
- mode 2: `aspect = CustomAspectRatio`; mode 0 or other: nothing written (see quirk).
- mode 1, non-portrait:
  | r | aspect |
  |---|---|
  | == 1.3333333 (4:3) | 1.15 |
  | == 1.5625 (25:16) | 1.1175 |
  | == 1.6 (16:10) | 1.0915 |
  | == 1.6666666 (15:9) | 1.0915 |
  | == 1.7777778 (16:9) | 1.0 |
  | == 2.3703704 (64:27) | 0.795 |
  | else | 16*H / (9*W) (code: `16 / ((float)W / ((float)H/9))`) |
  Note: 16:10 gives 1.0915 here, not 1.15 as in the base source (TFP-dev).
- mode 1, PortraitMode=1: skip the table, formula with W and H swapped: `16*W/(9*H)`.
- Final: `aspect_written = aspect - ExtraWide'` where `ExtraWide' = ExtraWide` normally, or `ExtraWide - 1.3` if PortraitMode==1.

### FOV (FOVMode)
- mode 2: `CustomFOV`. mode 1 (computed from r before the portrait swap):
  | r | FOV |
  |---|---|
  | 4:3 (1.3333333) | 1.333 |
  | 25:16 | 1.3615 |
  | 16:10 (1.6) | 1.385 |
  | 15:9 | 1.4185 |
  | 16:9 | 1.43 |
  | 64:27 | 1.675 |
  | else | min(1.75, 0.388*r + 0.88) |
- Written to 0x6FEA14 (v1.00) / 0x701BE0 (v1.01). Always computed also when mode is 0, but only stored for modes 1/2.

### Digital font kerning (DigitalFontKerningMode) -> 0x6FE670, v1.00 only
- mode 2: `CustomDigitalFontKerning`. mode 1 (r after possible portrait swap, i.e. H/W in portrait), using strict `<`:
  | r | value |
  |---|---|
  | < 1.5625 | 0.0015625 (original) |
  | < 1.6666666 | 0.0013333 |
  | < 1.7777778 | 0.0013021 |
  | < 2.3703704 | 0.0011710 |
  | else | 0.00087873 |

### Acceleration effect (AccelerationEffectMode) -> 0x6FEAF8, v1.00 only
- mode 2: `CustomAccelerationEffect`. mode 1: `k = (PortraitMode==1 ? 1.15 : 1.3) / FOV` where FOV is the value computed/selected above (so 4:3: 1.3/1.333 = 0.975 -> clamped 0.97).
- Both modes clamped: `k = min(0.97, max(0, k))`.

## 6. Other observations / uncertainty
- All "purpose" labels are guesses from the consumer code in the exe (listed read sites), not confirmed by running.
- The ASI never touches the game's flag `[0x75E0DC]`; the ini comment "Widescreen must be enabled in config" refers to the game's own setting, which gates reading 0x72477C and 0x6FEA14 in the game code.
- INI path resolution (cwd) not verified.
- DLL also imports MessageBoxA (user32) and Reg* (advapi32) only; no other imports used for patching.
