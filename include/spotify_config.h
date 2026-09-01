#ifndef SPOTIFY_CONFIG_H
#define SPOTIFY_CONFIG_H

/*
 * Spotify application Client ID.
 * DO NOT embed a client secret in a Vita homebrew application.
 */
#define SPOTIFY_CLIENT_ID "PUT_YOUR_SPOTIFY_CLIENT_ID_HERE"

/*
 * v19: use the Vita system app URI as the OAuth redirect.
 * Register this exact Redirect URI in Spotify Developer Dashboard:
 *
 *   psgm:play?titleid=SPVT00001
 *
 * Spotify returns code/state by appending query parameters to this URI.
 * The Vita app reads the launch URI with sceAppMgrGetAppParam().
 */
#define SPOTIFY_REDIRECT_URI "psgm:play?titleid=SPVT00001"

#define SPOTIFY_SCOPES \
    "user-read-playback-state " \
    "user-read-currently-playing " \
    "user-modify-playback-state"

#define SPOTIFY_DATA_DIR "ux0:data/spotify-vita"
#define SPOTIFY_TOKEN_STORE_PATH SPOTIFY_DATA_DIR "/session.bin"

#endif
