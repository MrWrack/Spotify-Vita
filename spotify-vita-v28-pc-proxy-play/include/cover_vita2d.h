#ifndef COVER_VITA2D_H
#define COVER_VITA2D_H

#include <stdint.h>
#include <vita2d.h>

typedef struct {
    vita2d_texture *texture;
    unsigned int width;
    unsigned int height;
    int ready;
} CoverVita2D;

int cover_vita2d_from_argb8888(
    const uint32_t *pixels,
    unsigned int width,
    unsigned int height,
    CoverVita2D *out
);

void cover_vita2d_destroy(
    CoverVita2D *cover
);

#endif
