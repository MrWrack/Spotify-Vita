#ifndef VITA2D_RENDERER_H
#define VITA2D_RENDERER_H

#include "app_controller.h"
#include "cover_vita2d.h"

typedef struct {
    CoverVita2D current_cover;
    vita2d_texture *pipeline_cover;
    int pipeline_cover_handle;
    char current_cover_url[1024];
    vita2d_pgf *font;
    int initialized;
} Vita2DRenderer;

int vita2d_renderer_init(
    Vita2DRenderer *renderer
);

void vita2d_renderer_shutdown(
    Vita2DRenderer *renderer
);

void vita2d_renderer_set_cover(
    Vita2DRenderer *renderer,
    const char *url,
    const uint32_t *argb_pixels,
    unsigned int width,
    unsigned int height
);


void vita2d_renderer_set_pipeline_cover(
    Vita2DRenderer *renderer,
    int handle,
    const char *url,
    vita2d_texture *texture
);

void vita2d_renderer_draw(
    Vita2DRenderer *renderer,
    const AppController *app
);

#endif
