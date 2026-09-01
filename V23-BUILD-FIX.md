# Spotify Vita v23 — build fix

Fixes GitHub Actions compile error:

`assignment of member 'login_focus' in read-only object`

Cause:
`app_ui_action_from_input()` receives a const AppController pointer, so
the input layer must not modify `app->login_focus`.

Fix:
- Removed the illegal assignment from the D-pad branch.
- Login screen still has only Spotify Login visible.
- X = login.
- O = back.
- No visible Back text or symbol.
- v21/v22 callback changes are retained.
- Visible version marker is now v23.
