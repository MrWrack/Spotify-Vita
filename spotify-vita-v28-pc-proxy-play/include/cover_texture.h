#ifndef COVER_TEXTURE_H
#define COVER_TEXTURE_H

#include <stdint.h>
#include <stddef.h>
#include <psp2/gxm.h>
#include <psp2/types.h>

/*
 * Canonical CPU representation used by the cover pipeline:
 *
 *     uint32_t pixel = 0xAARRGGBB
 *
 * This avoids depending on decoder byte order outside this module.
 */
typedef struct {
    const uint32_t *rgba;
    unsigned int width;
    unsigned int height;
} CoverRGBA;

typedef struct {
    SceGxmTexture texture;

    SceUID memblock;
    void *gpu_memory;
    size_t gpu_size;

    unsigned int width;
    unsigned int height;

    int mapped;
    int initialized;
} CoverGxmTexture;

typedef enum {
    COVER_FORMAT_RGBA8888_ABGR = 0
} CoverTextureFormat;

/*
 * Convert canonical 0xAARRGGBB pixels into the byte representation used by
 * SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR and create a linear GXM texture.
 *
 * IMPORTANT:
 * Call from the thread that owns GXM resources in the application.
 */
int cover_rgba_to_gxm(
    const CoverRGBA *src,
    CoverGxmTexture *out
);

/*
 * Create a GXM texture from already-converted bytes.
 */
int cover_gxm_create(
    const void *pixels,
    unsigned int width,
    unsigned int height,
    CoverTextureFormat format,
    CoverGxmTexture *out
);

/*
 * Destroy the GXM texture backing memory.
 * Call from the same GXM-owning thread used for creation.
 */
void cover_gxm_destroy(
    CoverGxmTexture *texture
);

/*
 * Create a 2x2 verification texture:
 *
 *   RED        GREEN
 *   BLUE       WHITE/50% alpha
 *
 * Render it with nearest filtering after the renderer/GXM context is ready.
 */
int cover_gxm_create_test_texture(
    CoverGxmTexture *out
);

#endif
