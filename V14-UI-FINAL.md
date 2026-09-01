# v14 — UI-FINAL

Replaces the rectangle-only test UI with a real text-based Spotify Vita interface.

Implemented:
- System PGF text rendering through libvita2d
- Spotify Vita black/green header and online/offline indicator
- Login page with visible Spotify login button
- Home navigation: Home / Search / Library / Settings
- D-pad up/down navigation and X selection
- Search, Library and Settings screens
- Persistent bottom mini-player
- Real track title / artist / album text
- Cover art rendering
- Now Playing screen with progress and previous/play/next controls
- O returns to Home
- L/R previous/next from Home
- Touch login, menu selection, mini-player and Now Playing controls
- Existing v13 8-bit LiveArea PNG fix retained
- Existing successful VitaSDK build/link fixes retained

Build with the same GitHub Actions workflow, then install the VPK on Vita.
