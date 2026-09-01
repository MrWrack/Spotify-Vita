# v18 — Vita network detection + callback diagnostics

Changes:
- Header now reflects the PS Vita's real NetCtl internet state:
  - NET ONLINE
  - NET OFFLINE
- Login is blocked with `VITA NETWORK` if NetCtl is not connected.
- Callback errors identify the exact operation:
  - THREAD CREATE
  - THREAD START
  - SOCKET
  - SETSOCKOPT
  - BIND
  - LISTEN
  - ACCEPT
  - RECV
  - PARSE
- Existing Vita network initialization in `vita_network.c` is retained.
- D-pad UP/DOWN, X and O/Back remain enabled.
- Callback still listens on all local IPv4 interfaces and the redirect remains
  `http://127.0.0.1:43891/callback`.

The previous device error was `-2143223539` (`0x8041010D`).
This build is designed to identify exactly which Vita socket call returns it.
