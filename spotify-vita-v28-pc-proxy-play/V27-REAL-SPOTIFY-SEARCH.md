# Spotify Vita v27 — Real Spotify Search

Search screen now talks to Spotify Web API.

Controls:
- D-pad: select letters on the on-screen keyboard.
- X: enter selected character.
- Square: delete last character.
- Triangle: send `/v1/search?type=track` request to Spotify.
- R: focus search results.
- L: return focus to keyboard.
- Up/Down in results: choose a track.
- X on a result: start that Spotify track on the active Spotify Connect device.
- O: back.

The app returns up to 8 tracks and displays title + artist.
Search uses the existing PKCE access/refresh token.
No Client Secret is embedded.

Note:
Spotify playback commands require an active Spotify playback device/account
that is allowed to use the Web API playback endpoints.
