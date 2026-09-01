# LIVEAREA-FINAL — v12

Added install/LiveArea resources:

- `sce_sys/icon0.png` — 128×128 Spotify Vita bubble icon
- `sce_sys/livearea/contents/bg.png` — 840×500 LiveArea background
- `sce_sys/livearea/contents/startup.png` — 280×158 gate/start image
- `sce_sys/livearea/contents/template.xml` — LiveArea template

The CMake VPK target packages all four resources.

v12 also preserves the two fixes made after the first CI build:
- `#include <psp2/io/stat.h>` for `sceIoMkdir`
- static library link order with `vita2d` before freetype/png/jpeg/z/m/c

Build it with the same GitHub Actions `VitaSDK Build` workflow.
