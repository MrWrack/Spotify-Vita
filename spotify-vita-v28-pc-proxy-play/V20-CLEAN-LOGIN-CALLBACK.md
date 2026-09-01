# Spotify Vita v20 — Clean Login + App URI Callback

This build intentionally removes the old local callback server.

UI:
- Removed `Logga in med ditt Spotify-konto`.
- Removed the green divider line below it.
- Added a visible `v20` marker in the header.
- D-pad UP/DOWN, X select and O Back remain.

OAuth callback:
- No local socket.
- No `sceNetBind()`.
- No `spotify_callback_server.c` in the project/build.
- Redirect URI:
  `psgm:play?titleid=SPVT00001`
- Callback is read with `sceAppMgrGetAppParam()`.

Spotify Dashboard must contain exactly:
  `psgm:play?titleid=SPVT00001`

Client ID:
- `include/spotify_config.h` must contain the real Spotify Client ID.
- If the placeholder is still present, v20 shows a specific Client ID error instead
  of pretending it is a callback/network failure.

Important:
If the Vita still displays `STEG: CALLBACK SERVER`, then v20 is NOT the installed VPK.
v20 displays `v20` in the header and never uses the callback server.
