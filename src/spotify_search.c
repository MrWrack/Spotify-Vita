#include "spotify_search.h"

#include "spotify_http.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int search_url_encode(
    const char *input,
    char *out,
    size_t out_size
)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t w = 0;

    if (!input || !out || out_size == 0)
        return -1;

    for (const unsigned char *p = (const unsigned char *)input; *p; ++p) {
        unsigned char c = *p;

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            if (w + 1 >= out_size)
                return -2;
            out[w++] = (char)c;
        } else {
            if (w + 3 >= out_size)
                return -2;
            out[w++] = '%';
            out[w++] = hex[(c >> 4) & 0x0F];
            out[w++] = hex[c & 0x0F];
        }
    }

    out[w] = '\0';
    return 0;
}

int spotify_search_tracks(
    SpotifyAuthPkce *auth,
    const char *query,
    SpotifyTrack *results,
    int max_results,
    int *out_count,
    int *out_http_status
)
{
    if (!auth || !query || !results || max_results <= 0 || !out_count)
        return -1;

    *out_count = 0;
    if (out_http_status)
        *out_http_status = 0;

    if (!query[0])
        return -2;

    int rc = spotify_auth_pkce_ensure_valid(auth);
    if (rc < 0)
        return rc;

    char encoded[384];
    rc = search_url_encode(query, encoded, sizeof(encoded));
    if (rc < 0)
        return rc;

    char path[512];
    int n = snprintf(
        path,
        sizeof(path),
        "/v1/search?q=%s&type=track&limit=%d",
        encoded,
        max_results
    );

    if (n < 0 || (size_t)n >= sizeof(path))
        return -3;

    SpotifyHttpResponse response;
    rc = spotify_http_request_api(
        SPOTIFY_HTTP_GET,
        path,
        NULL,
        NULL,
        &response
    );

    if (rc < 0)
        return rc;

    if (response.status_code == 401) {
        spotify_http_response_free(&response);

        rc = spotify_auth_pkce_refresh(auth);
        if (rc < 0)
            return rc;

        rc = spotify_http_request_api(
            SPOTIFY_HTTP_GET,
            path,
            NULL,
            NULL,
            &response
        );

        if (rc < 0)
            return rc;
    }

    if (out_http_status)
        *out_http_status = response.status_code;

    if (response.status_code != 200) {
        int status = response.status_code;
        spotify_http_response_free(&response);
        return -status;
    }

    rc = spotify_json_parse_search_tracks(
        response.body,
        response.body_size,
        results,
        max_results,
        out_count
    );

    spotify_http_response_free(&response);
    return rc;
}
