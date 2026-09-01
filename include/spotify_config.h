#ifndef SPOTIFY_CONFIG_H
#define SPOTIFY_CONFIG_H

/*
 * Spotify application Client ID.
 * DO NOT embed a client secret in a Vita homebrew application.
 */
#define SPOTIFY_CLIENT_ID "fc8dbfc6eecf4635b948af24981384ba"

/*
 * v25: Spotify Dashboard redirect URI.
 * Register this exact Redirect URI in Spotify Developer Dashboard:
 *
 *   http://127.0.0.1:8000/callback
 *
 * Note: Vita callback transport still requires hardware validation.
 */
#define SPOTIFY_REDIRECT_URI "http://127.0.0.1:8000/callback"

#define SPOTIFY_SCOPES \
    "user-read-playback-state " \
    "user-read-currently-playing " \
    "user-modify-playback-state"

#define SPOTIFY_DATA_DIR "ux0:data/spotify-vita"
#define SPOTIFY_TOKEN_STORE_PATH SPOTIFY_DATA_DIR "/session.bin"

#endif
