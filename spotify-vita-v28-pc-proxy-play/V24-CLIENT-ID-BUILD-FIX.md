# Spotify Vita v24 — Client ID compile fix

Fixes GitHub Actions error:

`spotify_login.c: error: 'SPOTIFY_CLIENT_ID' undeclared`

Cause:
`spotify_login.c` uses `SPOTIFY_CLIENT_ID`, but did not include
`spotify_config.h`, where that macro is defined.

Fix:
- Added `#include "spotify_config.h"` to `src/spotify_login.c`.
- Previous callback, navigation, network, and no-visible-Back fixes remain.
- Visible version marker is now `v24`.

Note:
The project will compile with the placeholder Client ID, but on-device login
will intentionally report that the real Spotify Client ID is missing until
`include/spotify_config.h` is configured.
