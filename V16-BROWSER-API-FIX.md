# v16 Browser API fix

- Replaces `sceAppMgrLaunchAppByUri()` / `webmodal:` with VitaSDK's
  `sceAppUtilLaunchWebBrowser()`.
- Loads `SCE_SYSMODULE_APPUTIL` and initializes AppUtil before opening URL.
- Adds `SceAppUtil_stub` to CMake link libraries.
- Error screen now identifies the failing stage:
  - LOGIN / PKCE
  - CALLBACK SERVER
  - VITA BROWSER
  - SPOTIFY HTTP
- HTTP status is only shown for actual Spotify HTTP failures.
- Keeps v15 D-pad UP/DOWN, X select and O back navigation.

Spotify Client ID and redirect URI still must be valid for authentication to finish.
