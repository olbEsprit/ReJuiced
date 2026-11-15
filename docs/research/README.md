# Research notes

Audits made while bringing Juiced up on x86recomp (November 2026), kept as they were written, in Russian. Paths
(`ref/x86recomp-wt-*`, `work/...`, `build-*`) are those of the working tree of the time; the code they name has since
moved to SwARc's new layout, so read them for the findings, not the file names.

- `d3d9-gaps.md` — what the demo would hit in the HLE between the menu and a race (imports, COM calls).
- `lifter.md` — lifter coverage and oracle tests against Unicorn.
- `shaders.md` — all 263 shaders of the game through the translator, glslang, SPIRV-Cross and Metal.
- `perf.md` — why the demo stuttered (shader compiles on the guest thread) and what fixed it.
- `widescreen-asi.md` — static audit of 4D-Eclipse's JuicedWidescreenFix.asi, the source of the widescreen patches.
