#ifndef SPOTIFY_STATE_WORKER_H
#define SPOTIFY_STATE_WORKER_H

#include "spotify_auth_pkce.h"
#include "spotify_json.h"
#include <stdint.h>

typedef enum {
    SPOTIFY_STATE_NONE = 0,
    SPOTIFY_STATE_PLAYBACK,
    SPOTIFY_STATE_QUEUE,
    SPOTIFY_STATE_ERROR
} SpotifyStateType;

typedef struct {
    SpotifyStateType type;
    SpotifyTrack track;
    SpotifyQueue queue;
    int http_status;
    int error;
    uint64_t received_at_ms;
} SpotifyStateResult;

int spotify_state_worker_init(
    SpotifyAuthPkce *auth
);

int spotify_state_worker_start(void);
void spotify_state_worker_wake(void);
void spotify_state_worker_stop(void);
void spotify_state_worker_shutdown(void);

int spotify_state_worker_poll(
    SpotifyStateResult *out
);

#endif
