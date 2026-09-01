# Spotify Vita v28 — PC Proxy Search + Play

Why:
The Vita can render the UI but modern Spotify HTTPS requests fail on hardware.
v28 moves Spotify HTTPS to the PC companion.

Setup:
1. Spotify Developer Dashboard Redirect URI:
   http://127.0.0.1:8000/callback
2. Install Python 3 on the PC.
3. Run:
   tools/START-COMPANION.bat
4. Log in to Spotify in the PC browser if asked.
5. The companion prints your PC IPv4 address, for example:
   192.168.1.42
6. Edit include/spotify_config.h before building the VPK:
   #define SPOTIFY_PC_PROXY_HOST "192.168.1.42"
7. PC and Vita must be on the same Wi-Fi.
8. Keep START-COMPANION.bat open while using the Vita app.

Search:
- Search requests go Vita -> PC over local HTTP.
- PC -> Spotify uses modern HTTPS.
- Results return to Vita.

Playback:
- Selecting a track sends /play to the PC companion.
- PC calls Spotify Web API /v1/me/player/play.

Important:
Spotify Web API playback controls an active Spotify Connect playback device.
It does not send raw Spotify audio to the Vita.
