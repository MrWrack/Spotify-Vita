#ifndef SPOTIFY_AUTH_PKCE_H
#define SPOTIFY_AUTH_PKCE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char client_id[128];
    char redirect_uri[512];

    char access_token[1024];
    char refresh_token[2048];

    uint64_t expires_at_ms;

    char code_verifier[129];
    char code_challenge[128];
    char state[129];

    int authenticated;
} SpotifyAuthPkce;

int spotify_auth_pkce_init(
    SpotifyAuthPkce *auth,
    const char *client_id,
    const char *redirect_uri
);

int spotify_auth_pkce_begin(
    SpotifyAuthPkce *auth,
    const char *scopes,
    char *authorization_url,
    size_t authorization_url_size
);

int spotify_auth_pkce_parse_callback(
    SpotifyAuthPkce *auth,
    const char *callback_url,
    char *code,
    size_t code_size
);

int spotify_auth_pkce_exchange_code(
    SpotifyAuthPkce *auth,
    const char *authorization_code
);

int spotify_auth_pkce_refresh(
    SpotifyAuthPkce *auth
);

int spotify_auth_pkce_ensure_valid(
    SpotifyAuthPkce *auth
);

void spotify_auth_pkce_clear(
    SpotifyAuthPkce *auth
);

const char *spotify_auth_pkce_access_token(
    const SpotifyAuthPkce *auth
);

#endif
