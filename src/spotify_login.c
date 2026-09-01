#include "spotify_login.h"

#include <stdio.h>
#include <string.h>

int spotify_login_init(
    SpotifyLogin *login,
    SpotifyAuthPkce *auth
)
{
    if (!login || !auth)
        return -1;

    memset(login, 0, sizeof(*login));
    login->auth = auth;
    login->state = auth->authenticated
        ? SPOTIFY_LOGIN_AUTHENTICATED
        : SPOTIFY_LOGIN_IDLE;

    return 0;
}

int spotify_login_begin(
    SpotifyLogin *login,
    const char *scopes
)
{
    if (!SPOTIFY_CLIENT_ID[0] ||
        strcmp(SPOTIFY_CLIENT_ID, "PUT_YOUR_SPOTIFY_CLIENT_ID_HERE") == 0)
        return -3001;


    if (!login || !login->auth || !scopes)
        return -1;

    login->last_error = 0;
    login->error_text[0] = '\0';

    int rc = spotify_auth_pkce_begin(
        login->auth,
        scopes,
        login->authorization_url,
        sizeof(login->authorization_url)
    );

    if (rc < 0) {
        login->state = SPOTIFY_LOGIN_ERROR;
        login->last_error = rc;
        snprintf(login->error_text, sizeof(login->error_text),
                 "Could not create Spotify authorization URL (%d)", rc);
        return rc;
    }

    /*
     * UI/platform code should now open this URL.
     * The auth core remains independent of the browser implementation.
     */
    login->state = SPOTIFY_LOGIN_NEEDS_BROWSER;
    return 0;
}

int spotify_login_accept_callback_url(
    SpotifyLogin *login,
    const char *callback_url
)
{
    if (!login || !login->auth || !callback_url)
        return -1;

    char code[2048];

    int rc = spotify_auth_pkce_parse_callback(
        login->auth,
        callback_url,
        code,
        sizeof(code)
    );

    if (rc < 0) {
        login->state = SPOTIFY_LOGIN_ERROR;
        login->last_error = rc;
        snprintf(login->error_text, sizeof(login->error_text),
                 "Invalid Spotify callback (%d)", rc);
        return rc;
    }

    login->state = SPOTIFY_LOGIN_EXCHANGING;

    rc = spotify_auth_pkce_exchange_code(login->auth, code);

    memset(code, 0, sizeof(code));

    if (rc < 0) {
        login->state = SPOTIFY_LOGIN_ERROR;
        login->last_error = rc;
        snprintf(login->error_text, sizeof(login->error_text),
                 "Token exchange failed (%d)", rc);
        return rc;
    }

    login->state = SPOTIFY_LOGIN_AUTHENTICATED;
    return 0;
}

const char *spotify_login_authorization_url(
    const SpotifyLogin *login
)
{
    return login ? login->authorization_url : NULL;
}
