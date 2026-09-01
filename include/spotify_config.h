#ifndef SPOTIFY_CONFIG_H
#define SPOTIFY_CONFIG_H

/*
 * Spotify application Client ID.
 * DO NOT embed a client secret in a Vita homebrew application.
 */
#define SPOTIFY_CLIENT_ID "PUT_YOUR_SPOTIFY_CLIENT_ID_HERE"

/*
 * Spotify currently requires HTTPS redirects, except explicit loopback IP
 * literals. Register this exact URI in the Spotify Developer Dashboard.
 *
 * The port may be changed, but the auth request and registered redirect must
 * match Spotify's redirect rules.
 */
#define SPOTIFY_REDIRECT_URI "http://127.0.0.1:43891/callback"
#define SPOTIFY_CALLBACK_PORT 43891

#define SPOTIFY_SCOPES \
    "user-read-playback-state " \
    "user-read-currently-playing " \
    "user-modify-playback-state"

#define SPOTIFY_DATA_DIR "ux0:data/spotify-vita"
#define SPOTIFY_TOKEN_STORE_PATH SPOTIFY_DATA_DIR "/session.bin"

#endif
