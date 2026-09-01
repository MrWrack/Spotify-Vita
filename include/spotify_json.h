#ifndef SPOTIFY_JSON_H
#define SPOTIFY_JSON_H

#include <stddef.h>

#define SPOTIFY_MAX_QUEUE 16

typedef struct {
    char id[128];
    char uri[256];
    char title[256];
    char artist[256];
    char album[256];
    char cover_url[1024];

    int duration_ms;
    int progress_ms;
    int is_playing;
    int valid;
} SpotifyTrack;

typedef struct {
    SpotifyTrack current;
    SpotifyTrack items[SPOTIFY_MAX_QUEUE];
    int count;
    int valid;
} SpotifyQueue;

typedef struct {
    char access_token[1024];
    char refresh_token[2048];
    int expires_in;
    int has_refresh_token;
} SpotifyTokenResponse;

int spotify_json_parse_player(
    const char *json,
    size_t size,
    SpotifyTrack *out
);

int spotify_json_parse_queue(
    const char *json,
    size_t size,
    SpotifyQueue *out
);

int spotify_json_parse_token(
    const char *json,
    size_t size,
    SpotifyTokenResponse *out
);

int spotify_json_get_error(
    const char *json,
    size_t size,
    char *out,
    size_t out_size
);

#endif
