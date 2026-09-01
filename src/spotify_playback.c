#include "spotify_playback.h"
#include "spotify_http.h"

#include <stdio.h>

static int command(
    SpotifyHttpMethod method,
    const char *path
)
{
    SpotifyHttpResponse response;
    int rc = spotify_http_request_api(method, path, NULL, NULL, &response);

    if (rc < 0)
        return rc;

    int status = response.status_code;
    spotify_http_response_free(&response);

    return status == 204 ? 0 : -status;
}

int spotify_playback_play(void)
{
    return command(SPOTIFY_HTTP_PUT, "/v1/me/player/play");
}

int spotify_playback_pause(void)
{
    return command(SPOTIFY_HTTP_PUT, "/v1/me/player/pause");
}

int spotify_playback_next(void)
{
    return command(SPOTIFY_HTTP_POST, "/v1/me/player/next");
}

int spotify_playback_previous(void)
{
    return command(SPOTIFY_HTTP_POST, "/v1/me/player/previous");
}

int spotify_playback_seek(int position_ms)
{
    if (position_ms < 0)
        position_ms = 0;

    char path[128];
    snprintf(
        path,
        sizeof(path),
        "/v1/me/player/seek?position_ms=%d",
        position_ms
    );

    return command(SPOTIFY_HTTP_PUT, path);
}

int spotify_playback_volume(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    char path[128];
    snprintf(
        path,
        sizeof(path),
        "/v1/me/player/volume?volume_percent=%d",
        percent
    );

    return command(SPOTIFY_HTTP_PUT, path);
}
