#include "gxm_display.h"

#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>

#include <string.h>

#define DISPLAY_BUFFER_COUNT 2
#define DISPLAY_MAX_PENDING_SWAPS 2
#define ALIGN_UP(v,a) (((v)+((a)-1))&~((a)-1))

typedef struct {
    void *address;
} DisplayCallbackData;

static void display_callback(const void *callback_data)
{
    const DisplayCallbackData *data =
        (const DisplayCallbackData *)callback_data;

    SceDisplayFrameBuf frame;
    memset(&frame, 0, sizeof(frame));

    frame.size = sizeof(frame);
    frame.base = data->address;
    frame.pitch = GXM_DISPLAY_STRIDE;
    frame.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    frame.width = GXM_DISPLAY_WIDTH;
    frame.height = GXM_DISPLAY_HEIGHT;

    sceDisplaySetFrameBuf(
        &frame,
        SCE_DISPLAY_SETBUF_NEXTFRAME
    );
}

static void *alloc_cdram(
    unsigned int size,
    SceUID *uid
)
{
    unsigned int alloc_size =
        ALIGN_UP(size, 256 * 1024);

    *uid = sceKernelAllocMemBlock(
        "SpotifyVitaDisplay",
        SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
        alloc_size,
        NULL
    );

    if (*uid < 0)
        return NULL;

    void *memory = NULL;

    if (sceKernelGetMemBlockBase(*uid, &memory) < 0) {
        sceKernelFreeMemBlock(*uid);
        *uid = -1;
        return NULL;
    }

    if (sceGxmMapMemory(
            memory,
            alloc_size,
            SCE_GXM_MEMORY_ATTRIB_READ |
            SCE_GXM_MEMORY_ATTRIB_WRITE) < 0) {
        sceKernelFreeMemBlock(*uid);
        *uid = -1;
        return NULL;
    }

    return memory;
}

int gxm_display_init(
    GxmDisplay *display
)
{
    if (!display)
        return -1;

    memset(display, 0, sizeof(*display));

    SceGxmInitializeParams init;
    memset(&init, 0, sizeof(init));

    init.flags = 0;
    init.displayQueueMaxPendingCount =
        DISPLAY_MAX_PENDING_SWAPS;
    init.displayQueueCallback =
        display_callback;
    init.displayQueueCallbackDataSize =
        sizeof(DisplayCallbackData);
    init.parameterBufferSize =
        SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

    int rc = sceGxmInitialize(&init);
    if (rc < 0)
        return rc;

    const unsigned int bytes =
        GXM_DISPLAY_STRIDE *
        GXM_DISPLAY_HEIGHT *
        4u;

    for (int i = 0; i < DISPLAY_BUFFER_COUNT; ++i) {
        display->buffers[i].uid = -1;

        display->buffers[i].data =
            alloc_cdram(
                bytes,
                &display->buffers[i].uid
            );

        if (!display->buffers[i].data) {
            gxm_display_shutdown(display);
            return -2;
        }

        memset(
            display->buffers[i].data,
            0,
            bytes
        );

        rc = sceGxmColorSurfaceInit(
            &display->buffers[i].surface,
            SCE_GXM_COLOR_FORMAT_A8B8G8R8,
            SCE_GXM_COLOR_SURFACE_LINEAR,
            SCE_GXM_COLOR_SURFACE_SCALE_NONE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
            GXM_DISPLAY_WIDTH,
            GXM_DISPLAY_HEIGHT,
            GXM_DISPLAY_STRIDE,
            display->buffers[i].data
        );

        if (rc < 0) {
            gxm_display_shutdown(display);
            return rc;
        }

        rc = sceGxmSyncObjectCreate(
            &display->buffers[i].sync
        );

        if (rc < 0) {
            gxm_display_shutdown(display);
            return rc;
        }
    }

    display->back_index = 0;
    display->front_index = 0;
    display->initialized = 1;

    return 0;
}

uint32_t *gxm_display_begin_frame(
    GxmDisplay *display
)
{
    if (!display || !display->initialized)
        return NULL;

    return (uint32_t *)
        display->buffers[
            display->back_index
        ].data;
}

void gxm_display_end_frame(
    GxmDisplay *display
)
{
    if (!display || !display->initialized)
        return;

    GxmDisplayBuffer *back =
        &display->buffers[display->back_index];

    GxmDisplayBuffer *front =
        &display->buffers[display->front_index];

    sceGxmPadHeartbeat(
        &back->surface,
        back->sync
    );

    DisplayCallbackData callback;
    callback.address = back->data;

    sceGxmDisplayQueueAddEntry(
        front->sync,
        back->sync,
        &callback
    );

    display->front_index =
        display->back_index;

    display->back_index =
        (display->back_index + 1) %
        DISPLAY_BUFFER_COUNT;

    sceDisplayWaitVblankStart();
}

void gxm_display_shutdown(
    GxmDisplay *display
)
{
    if (!display)
        return;

    if (display->initialized)
        sceGxmDisplayQueueFinish();

    for (int i = 0; i < DISPLAY_BUFFER_COUNT; ++i) {
        if (display->buffers[i].sync) {
            sceGxmSyncObjectDestroy(
                display->buffers[i].sync
            );
            display->buffers[i].sync = NULL;
        }

        if (display->buffers[i].data) {
            sceGxmUnmapMemory(
                display->buffers[i].data
            );
            display->buffers[i].data = NULL;
        }

        if (display->buffers[i].uid >= 0) {
            sceKernelFreeMemBlock(
                display->buffers[i].uid
            );
            display->buffers[i].uid = -1;
        }
    }

    if (display->initialized)
        sceGxmTerminate();

    memset(display, 0, sizeof(*display));
}

int gxm_display_width(void)  { return GXM_DISPLAY_WIDTH; }
int gxm_display_height(void) { return GXM_DISPLAY_HEIGHT; }
int gxm_display_stride(void) { return GXM_DISPLAY_STRIDE; }
