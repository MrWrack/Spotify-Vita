#ifndef SPOTIFY_TOKEN_STORE_H
#define SPOTIFY_TOKEN_STORE_H

#include "spotify_auth_pkce.h"

/*
 * Persistence only.
 *
 * This module deliberately does NOT call itself encrypted or secure storage.
 * It stores the refresh/access session locally with a version header and
 * checksum to detect accidental corruption.
 *
 * A future device-bound encryption implementation can replace this module
 * without changing SpotifyAuthPkce.
 */
int spotify_token_store_save(
    const char *path,
    const SpotifyAuthPkce *auth
);

int spotify_token_store_load(
    const char *path,
    SpotifyAuthPkce *auth
);

int spotify_token_store_delete(
    const char *path
);

#endif
