#ifndef GXM_DISPLAY_H
#define GXM_DISPLAY_H

#include <stdint.h>
#include <psp2/gxm.h>
#include <psp2/types.h>

#define GXM_DISPLAY_WIDTH  960
#define GXM_DISPLAY_HEIGHT 544
#define GXM_DISPLAY_STRIDE 1024
#define GXM_DISPLAY_BUFFER_COUNT 2

typedef struct {
    void *data;
    SceUID uid;
    SceGxmColorSurface surface;
    SceGxmSyncObject *sync;
} GxmDisplayBuffer;

typedef struct GxmDisplay {
    GxmDisplayBuffer buffers[GXM_DISPLAY_BUFFER_COUNT];
    unsigned int back_index;
    unsigned int front_index;
    int initialized;
} GxmDisplay;

int gxm_display_init(GxmDisplay *display);
void gxm_display_shutdown(GxmDisplay *display);

uint32_t *gxm_display_begin_frame(GxmDisplay *display);
void gxm_display_end_frame(GxmDisplay *display);

int gxm_display_width(void);
int gxm_display_height(void);
int gxm_display_stride(void);

#endif
