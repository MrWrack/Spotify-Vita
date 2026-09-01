# v17 — Callback + Back fix

What changed:
- Callback listener now binds to `SCE_NET_INADDR_ANY` instead of loopback-only.
- Redirect URL remains `http://127.0.0.1:43891/callback`.
- O/X on the ERROR screen now truly resets/stops the callback worker before
  returning to LOGIN.
- `app_controller_update()` only reopens CALLBACK ERROR while a login attempt
  is actually active.
- A new login attempt clears stale callback errors first.
- Existing D-pad UP/DOWN + X selection remains.

This specifically addresses the on-device v16 result:
`STEG: CALLBACK SERVER`.
