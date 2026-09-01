#include "app_cover.h"

#include "cover_pipeline.h"

#include <string.h>

static void replace_handle(
    int *slot,
    int next
)
{
    if (*slot == next)
        return;

    if (*slot > 0)
        cover_release(*slot);

    *slot = next;

    if (*slot > 0)
        cover_acquire(*slot);
}

void app_cover_init(
    AppCoverState *state
)
{
    if (!state)
        return;

    memset(state, 0, sizeof(*state));
}

void app_cover_update(
    AppCoverState *state,
    const AppController *app,
    Vita2DRenderer *renderer
)
{
    if (!state || !app || !renderer)
        return;

    /*
     * Current track is highest priority.
     */
    int current = 0;

    if (app->now_playing.has_track &&
        app->now_playing.track.cover_url[0]) {

        current = cover_request(
            app->now_playing.track.cover_url,
            100
        );
    }

    replace_handle(
        &state->current_handle,
        current
    );

    /*
     * Preload next and next+1 from Spotify queue.
     */
    for (int i = 0; i < 2; ++i) {
        int next = 0;

        if (app->now_playing.has_queue &&
            i < app->now_playing.queue.count &&
            app->now_playing.queue.items[i].cover_url[0]) {

            next = cover_request(
                app->now_playing.queue.items[i].cover_url,
                i == 0 ? 90 : 80
            );
        }

        replace_handle(
            &state->next_handles[i],
            next
        );
    }

    if (state->current_handle > 0) {
        CoverInfo info;

        if (cover_get(
                state->current_handle,
                &info) == 0 &&
            info.state == COVER_STATE_READY &&
            info.texture) {

            cover_touch(
                state->current_handle
            );

            vita2d_renderer_set_pipeline_cover(
                renderer,
                info.handle,
                info.url,
                info.texture
            );

            return;
        }
    }

    /*
     * Do not retain a stale pipeline texture when the song has no ready cover.
     * The renderer falls back to its startup test texture / placeholder.
     */
    vita2d_renderer_set_pipeline_cover(
        renderer,
        0,
        NULL,
        NULL
    );
}

void app_cover_shutdown(
    AppCoverState *state,
    Vita2DRenderer *renderer
)
{
    if (!state)
        return;

    if (renderer) {
        vita2d_renderer_set_pipeline_cover(
            renderer,
            0,
            NULL,
            NULL
        );
    }

    replace_handle(
        &state->current_handle,
        0
    );

    replace_handle(
        &state->next_handles[0],
        0
    );

    replace_handle(
        &state->next_handles[1],
        0
    );
}
