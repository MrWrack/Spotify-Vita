#include "cover_vita2d.h"

#include <string.h>

int cover_vita2d_from_argb8888(
    const uint32_t *pixels,
    unsigned int width,
    unsigned int height,
    CoverVita2D *out
)
{
    if (!pixels || !out || width == 0 || height == 0)
        return -1;

    memset(out, 0, sizeof(*out));

    /*
     * libvita2d's RGBA8 macro packs bytes as Vita display/GXM expects:
     * low byte R, then G, B, high byte A.
     */
    vita2d_texture *texture =
        vita2d_create_empty_texture_format(
            width,
            height,
            SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR
        );

    if (!texture)
        return -2;

    uint32_t *dst =
        (uint32_t *)vita2d_texture_get_datap(
            texture
        );

    if (!dst) {
        vita2d_free_texture(texture);
        return -3;
    }

    unsigned int stride_bytes =
        vita2d_texture_get_stride(texture);

    unsigned int stride_pixels =
        stride_bytes / 4u;

    /*
     * Input convention is 0xAARRGGBB.
     * Convert each pixel to the byte layout expected by
     * U8U8U8U8_ABGR: little-endian memory [R,G,B,A].
     */
    for (unsigned int y = 0; y < height; ++y) {
        for (unsigned int x = 0; x < width; ++x) {
            uint32_t p = pixels[y * width + x];

            uint32_t a = (p >> 24) & 0xffu;
            uint32_t r = (p >> 16) & 0xffu;
            uint32_t g = (p >> 8)  & 0xffu;
            uint32_t b = p & 0xffu;

            dst[y * stride_pixels + x] =
                (a << 24) |
                (b << 16) |
                (g << 8) |
                r;
        }
    }

    vita2d_texture_set_filters(
        texture,
        SCE_GXM_TEXTURE_FILTER_LINEAR,
        SCE_GXM_TEXTURE_FILTER_LINEAR
    );

    out->texture = texture;
    out->width = width;
    out->height = height;
    out->ready = 1;

    return 0;
}

void cover_vita2d_destroy(
    CoverVita2D *cover
)
{
    if (!cover)
        return;

    if (cover->texture) {
        vita2d_free_texture(
            cover->texture
        );
    }

    memset(
        cover,
        0,
        sizeof(*cover)
    );
}
