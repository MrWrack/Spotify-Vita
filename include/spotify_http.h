#ifndef SPOTIFY_HTTP_H
#define SPOTIFY_HTTP_H

#include <stddef.h>

typedef enum {
    SPOTIFY_HTTP_GET = 0,
    SPOTIFY_HTTP_POST,
    SPOTIFY_HTTP_PUT
} SpotifyHttpMethod;

typedef struct {
    int status_code;
    int retry_after;
    char *body;
    size_t body_size;
} SpotifyHttpResponse;

int spotify_http_init(void);
void spotify_http_shutdown(void);

void spotify_http_set_access_token(
    const char *access_token
);

int spotify_http_request_api(
    SpotifyHttpMethod method,
    const char *path,
    const char *body,
    const char *content_type,
    SpotifyHttpResponse *out
);

int spotify_http_request_absolute(
    SpotifyHttpMethod method,
    const char *url,
    const char *body,
    const char *content_type,
    const char *authorization,
    SpotifyHttpResponse *out
);

void spotify_http_response_free(
    SpotifyHttpResponse *response
);

#endif
