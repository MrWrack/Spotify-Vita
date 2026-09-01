# v13 — VitaShell 0x8010113D PNG fix

The LiveArea resources were regenerated as **8-bit indexed/paletted PNG** files
(PNG bit depth 8, color type 3), instead of 32-bit RGBA PNG.

Files changed:
- sce_sys/icon0.png
- sce_sys/livearea/contents/bg.png
- sce_sys/livearea/contents/startup.png

All source code, CMake link fixes, and LiveArea packaging from the successful v12
build are retained.
