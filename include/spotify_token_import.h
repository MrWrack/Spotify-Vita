#ifndef SPOTIFY_TOKEN_IMPORT_H
#define SPOTIFY_TOKEN_IMPORT_H

#include "spotify_auth_pkce.h"

/*
 * Imports tokens produced by tools/spotify_vita_login_helper.py.
 * Returns:
 *   1 imported successfully
 *   0 no import file present
 *  <0 parse/I/O error
 */
int spotify_token_import_try(SpotifyAuthPkce *auth);

#endif
