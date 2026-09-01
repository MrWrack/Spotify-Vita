# v19 — Vita App-URI callback

Main fix:
- Removes the local HTTP callback listener from the login flow.
- No `sceNetBind()` is used for Spotify OAuth callback.
- Spotify Redirect URI is now:
  `psgm:play?titleid=SPVT00001`
- The Vita receives the launch/resume URI using `sceAppMgrGetAppParam()`.
- `code` and `state` are passed into the existing PKCE validation/token exchange.

Spotify Developer Dashboard:
Register EXACTLY this Redirect URI:
`psgm:play?titleid=SPVT00001`

Login UI cleanup:
- Removed `Logga in med ditt Spotify-konto`.
- Removed the decorative green line below that text.
- Moved LOGIN/BACK controls upward.
- D-pad UP/DOWN, X, O/Back and touch remain.

Network:
- NET ONLINE / NET OFFLINE remains based on Vita NetCtl.

Why:
The on-device v18 diagnostic proved that `sceNetBind()` failed with
`0x8041010D` (`SCE_NET_ERROR_EACCES`). v19 therefore avoids the local
listening socket entirely.
