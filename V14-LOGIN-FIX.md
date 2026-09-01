# Spotify Vita v14 LOGIN-FIX

Changes:
- Uses Vita's `webmodal: https://...` URI handler when opening Spotify authorization.
- Keeps the documented `sceAppMgrLaunchAppByUri(0x20000, ...)` flag.
- Login button is explicitly focused with D-pad.
- X activates the focused login button.
- Touch hitbox is restricted to the login button.
- Existing v13 8-bit LiveArea fix and v14 UI are retained.

IMPORTANT:
`include/spotify_config.h` still needs a real Spotify application Client ID.
The redirect URI registered in Spotify Dashboard must exactly match:
`http://127.0.0.1:43891/callback`
