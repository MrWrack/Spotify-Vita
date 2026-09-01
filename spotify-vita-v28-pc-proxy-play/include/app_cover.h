#ifndef APP_COVER_H
#define APP_COVER_H

#include "app_controller.h"
#include "vita2d_renderer.h"

typedef struct {
    int current_handle;
    int next_handles[2];
} AppCoverState;

void app_cover_init(
    AppCoverState *state
);

void app_cover_update(
    AppCoverState *state,
    const AppController *app,
    Vita2DRenderer *renderer
);

void app_cover_shutdown(
    AppCoverState *state,
    Vita2DRenderer *renderer
);

#endif
