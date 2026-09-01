# Spotify Vita v21 — App URI callback fix

Callback transport:
- No local callback server.
- No `sceNetBind()`.
- Uses `sceAppMgrGetAppParam()`.

Redirect URI:
`psgm:play?titleid=SPVT00001`

The parser now handles both forms Vita may expose:
1. `uri=psgm:play?titleid=SPVT00001&code=...&state=...`
2. `uri=psgm:play?titleid=SPVT00001` with `code` and `state` exposed as
   separate AppMgr parameters.

The complete AppMgr string is no longer URL-decoded as one blob before parsing,
which avoids turning percent-encoded OAuth data into separators.

UI:
- Version marker is `v21` at bottom-right of the login screen.
- D-pad UP/DOWN, X and O/Back remain.
- NET ONLINE / NET OFFLINE remains.

Spotify Dashboard must contain EXACTLY:
`psgm:play?titleid=SPVT00001`
