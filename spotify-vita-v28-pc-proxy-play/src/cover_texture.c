#include "cover_texture.h"

#include <psp2/kernel/sysmem.h>
#include <stdlib.h>
#include <string.h>

#define COVER_GPU_ALIGNMENT (256u * 1024u)

static size_t align_up_size(size_t value, size_t alignment)
{
    return (value + alignment - 1u) & ~(alignment - 1u);
}

static void cover_reset(CoverGxmTexture *texture)
{
    memset(texture, 0, sizeof(*texture));
    texture->memblock = -1;
}

/*
 * Canonical pixel: 0xAARRGGBB
 *
 * Destination bytes in little-endian memory become:
 *   R G B A
 *
 * packed as 0xAABBGGRR.
 *
 * This is intentionally isolated here so the 2x2 hardware verification can
 * confirm the selected GXM format. If the renderer shows swapped channels,
 * only this conversion layer needs to change.
 */
static uint32_t canonical_to_gxm_abgr(uint32_t pixel)
{
    const uint32_t a = (pixel >> 24) & 0xffu;
    const uint32_t r = (pixel >> 16) & 0xffu;
    const uint32_t g = (pixel >> 8)  & 0xffu;
    const uint32_t b = pixel & 0xffu;

    return (a << 24) | (b << 16) | (g << 8) | r;
}

static int gpu_alloc(size_t requested_size, CoverGxmTexture *out)
{
    const size_t mapped_size = align_up_size(requested_size, COVER_GPU_ALIGNMENT);

    out->memblock = sceKernelAllocMemBlock(
        "SpotifyVitaCover",
        SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
        mapped_size,
        NULL
    );

    if (out->memblock < 0)
        return out->memblock;

    int rc = sceKernelGetMemBlockBase(out->memblock, &out->gpu_memory);
    if (rc < 0) {
        sceKernelFreeMemBlock(out->memblock);
        cover_reset(out);
        return rc;
    }

    rc = sceGxmMapMemory(
        out->gpu_memory,
        mapped_size,
        SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE
    );

    if (rc < 0) {
        sceKernelFreeMemBlock(out->memblock);
        cover_reset(out);
        return rc;
    }

    out->gpu_size = mapped_size;
    out->mapped = 1;
    return 0;
}

int cover_gxm_create(
    const void *pixels,
    unsigned int width,
    unsigned int height,
    CoverTextureFormat format,
    CoverGxmTexture *out
)
{
    if (!pixels || !out || width == 0 || height == 0)
        return -1;

    if (format != COVER_FORMAT_RGBA8888_ABGR)
        return -2;

    cover_reset(out);

    const size_t pixel_bytes = (size_t)width * (size_t)height * 4u;
    int rc = gpu_alloc(pixel_bytes, out);
    if (rc < 0)
        return rc;

    memcpy(out->gpu_memory, pixels, pixel_bytes);

    rc = sceGxmTextureInitLinear(
        &out->texture,
        out->gpu_memory,
        SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
        width,
        height,
        0
    );

    if (rc < 0) {
        cover_gxm_destroy(out);
        return rc;
    }

    sceGxmTextureSetMinFilter(
        &out->texture,
        SCE_GXM_TEXTURE_FILTER_LINEAR
    );

    sceGxmTextureSetMagFilter(
        &out->texture,
        SCE_GXM_TEXTURE_FILTER_LINEAR
    );

    sceGxmTextureSetUAddrMode(
        &out->texture,
        SCE_GXM_TEXTURE_ADDR_CLAMP
    );

    sceGxmTextureSetVAddrMode(
        &out->texture,
        SCE_GXM_TEXTURE_ADDR_CLAMP
    );

    out->width = width;
    out->height = height;
    out->initialized = 1;
    return 0;
}

int cover_rgba_to_gxm(
    const CoverRGBA *src,
    CoverGxmTexture *out
)
{
    if (!src || !src->rgba || !out || src->width == 0 || src->height == 0)
        return -1;

    const size_t count = (size_t)src->width * (size_t)src->height;
    uint32_t *converted = (uint32_t *)malloc(count * sizeof(uint32_t));
    if (!converted)
        return -2;

    for (size_t i = 0; i < count; ++i)
        converted[i] = canonical_to_gxm_abgr(src->rgba[i]);

    const int rc = cover_gxm_create(
        converted,
        src->width,
        src->height,
        COVER_FORMAT_RGBA8888_ABGR,
        out
    );

    free(converted);
    return rc;
}

int cover_gxm_create_test_texture(
    CoverGxmTexture *out
)
{
    /*
     * Canonical 0xAARRGGBB values.
     */
    static const uint32_t test_pixels[4] = {
        0xFFFF0000u, /* RED */
        0xFF00FF00u, /* GREEN */
        0xFF0000FFu, /* BLUE */
        0x80FFFFFFu  /* WHITE, 50% alpha */
    };

    const CoverRGBA image = {
        test_pixels,
        2,
        2
    };

    int rc = cover_rgba_to_gxm(&image, out);
    if (rc < 0)
        return rc;

    /*
     * Nearest is easier to visually validate for a 2x2 test texture.
     */
    sceGxmTextureSetMinFilter(
        &out->texture,
        SCE_GXM_TEXTURE_FILTER_POINT
    );

    sceGxmTextureSetMagFilter(
        &out->texture,
        SCE_GXM_TEXTURE_FILTER_POINT
    );

    return 0;
}

void cover_gxm_destroy(
    CoverGxmTexture *texture
)
{
    if (!texture)
        return;

    if (texture->mapped && texture->gpu_memory) {
        sceGxmUnmapMemory(texture->gpu_memory);
        texture->mapped = 0;
    }

    if (texture->memblock >= 0) {
        sceKernelFreeMemBlock(texture->memblock);
        texture->memblock = -1;
    }

    texture->gpu_memory = NULL;
    texture->gpu_size = 0;
    texture->width = 0;
    texture->height = 0;
    texture->initialized = 0;

    memset(&texture->texture, 0, sizeof(texture->texture));
}
