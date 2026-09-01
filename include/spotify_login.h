#ifndef SPOTIFY_LOGIN_H
#define SPOTIFY_LOGIN_H

#include <stddef.h>

#include "spotify_auth_pkce.h"

typedef enum {
    SPOTIFY_LOGIN_IDLE = 0,
    SPOTIFY_LOGIN_NEEDS_BROWSER,
    SPOTIFY_LOGIN_WAITING_CALLBACK,
    SPOTIFY_LOGIN_EXCHANGING,
    SPOTIFY_LOGIN_AUTHENTICATED,
    SPOTIFY_LOGIN_ERROR
} SpotifyLoginState;

typedef struct {
    SpotifyAuthPkce *auth;
    SpotifyLoginState state;

    char authorization_url[4096];
    char error_text[256];
    int last_error;
} SpotifyLogin;

int spotify_login_init(
    SpotifyLogin *login,
    SpotifyAuthPkce *auth
);

int spotify_login_begin(
    SpotifyLogin *login,
    const char *scopes
);

/*
 * Feed the exact callback URL obtained by the Vita callback transport.
 * Example:
 * http://127.0.0.1:43891/callback?code=...&state=...
 */
int spotify_login_accept_callback_url(
    SpotifyLogin *login,
    const char *callback_url
);

const char *spotify_login_authorization_url(
    const SpotifyLogin *login
);

#endif
