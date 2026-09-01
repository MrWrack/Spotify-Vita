#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include <stdint.h>

#include "app_controller.h"

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    int stride;
} UiSurface;

void ui_renderer_draw(
    UiSurface *surface,
    const AppController *app
);

#endif
