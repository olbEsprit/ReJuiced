# Third-party notices

- **SwARc**, formerly x86recomp (https://github.com/JTM-rootstorm/SwARc), MIT. Its `juiced` branch
  (https://github.com/olbEsprit/SwARc) is fetched into `vendor/`, not included.
- **SDL3** (https://libsdl.org), zlib license. Linked from Homebrew.
- **FFmpeg** (https://ffmpeg.org), LGPL 2.1+. Linked dynamically from Homebrew by x86recomp's Bink replacement.
- **Xelu's Free Controller & Key Prompts** (https://thoseawesomeguys.com/prompts/), CC0. The originals are in
  `tools/pad_prompts_src/`; `assets/ui/textures/` is made from them by `tools/pad_prompts.py`.
- **JuicedWidescreenFix** by 4D-Eclipse (https://github.com/4D-Eclipse/JuicedWidescreenFix), based on TFP-dev's
  Juiced widescreen fix (MIT). The port sets the same four values the fix's plugin writes (`docs/widescreen.md`);
  its widescreen UI files are downloaded by `tools/widescreen-ui.sh`, not included.
