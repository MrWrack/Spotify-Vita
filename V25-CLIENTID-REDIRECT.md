# Spotify Vita v25

Configured:
- Spotify Client ID: fc8dbfc6eecf4635b948af24981384ba
- Redirect URI: http://127.0.0.1:8000/callback
- Visible version marker: v25

No Client Secret is embedded.

Important:
The redirect URI now matches the value accepted in Spotify Developer Dashboard.
The Vita-side loopback callback still needs hardware validation, because an
earlier Vita test returned EACCES when binding a local callback socket.
