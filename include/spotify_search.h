#ifndef SPOTIFY_SEARCH_H
#define SPOTIFY_SEARCH_H

#include "spotify_auth_pkce.h"
#include "spotify_json.h"

int spotify_search_tracks(
    SpotifyAuthPkce *auth,
    const char *query,
    SpotifyTrack *results,
    int max_results,
    int *out_count,
    int *out_http_status
);

#endif
