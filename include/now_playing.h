#ifndef NOW_PLAYING_H
#define NOW_PLAYING_H

#include <stdint.h>

#include "spotify_state_worker.h"

typedef struct {
    SpotifyTrack track;
    SpotifyQueue queue;

    uint64_t state_received_ms;
    int has_track;
    int has_queue;
    int last_error;
    int last_http_status;
} NowPlayingModel;

void now_playing_init(
    NowPlayingModel *model
);

void now_playing_apply_state(
    NowPlayingModel *model,
    const SpotifyStateResult *state
);

/*
 * Returns estimated progress between API polls.
 */
int now_playing_progress_ms(
    const NowPlayingModel *model,
    uint64_t now_ms
);

#endif
