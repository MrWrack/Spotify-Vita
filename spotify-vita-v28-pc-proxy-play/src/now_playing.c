#include "now_playing.h"

#include <string.h>

void now_playing_init(
    NowPlayingModel *model
)
{
    if (!model)
        return;

    memset(model, 0, sizeof(*model));
}

void now_playing_apply_state(
    NowPlayingModel *model,
    const SpotifyStateResult *state
)
{
    if (!model || !state)
        return;

    switch (state->type) {
        case SPOTIFY_STATE_PLAYBACK:
            model->track = state->track;
            model->state_received_ms = state->received_at_ms;
            model->has_track = state->track.valid;
            break;

        case SPOTIFY_STATE_QUEUE:
            model->queue = state->queue;
            model->has_queue = state->queue.valid;
            break;

        case SPOTIFY_STATE_ERROR:
            model->last_error = state->error;
            model->last_http_status = state->http_status;
            break;

        default:
            break;
    }
}

int now_playing_progress_ms(
    const NowPlayingModel *model,
    uint64_t now_ms
)
{
    if (!model || !model->has_track)
        return 0;

    int progress = model->track.progress_ms;

    if (model->track.is_playing && now_ms > model->state_received_ms) {
        uint64_t delta = now_ms - model->state_received_ms;

        if (delta > 0x7fffffffu)
            delta = 0x7fffffffu;

        progress += (int)delta;
    }

    if (model->track.duration_ms > 0 &&
        progress > model->track.duration_ms)
        progress = model->track.duration_ms;

    if (progress < 0)
        progress = 0;

    return progress;
}
