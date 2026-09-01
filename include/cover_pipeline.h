#ifndef COVER_PIPELINE_H
#define COVER_PIPELINE_H

#include <stddef.h>
#include <stdint.h>
#include <vita2d.h>

#define COVER_PIPELINE_MAX_ITEMS 16
#define COVER_PIPELINE_MEMORY_BUDGET (16u * 1024u * 1024u)

typedef enum {
    COVER_STATE_EMPTY = 0,
    COVER_STATE_QUEUED,
    COVER_STATE_DOWNLOADING,
    COVER_STATE_DOWNLOADED,
    COVER_STATE_READY,
    COVER_STATE_ERROR
} CoverState;

typedef struct {
    int handle;
    CoverState state;
    int priority;
    int http_status;
    int error;

    char url[1024];

    vita2d_texture *texture;
    unsigned int width;
    unsigned int height;
} CoverInfo;

/*
 * Worker lifecycle.
 * HTTP downloads happen on the worker; texture decoding/upload happens only
 * when cover_pipeline_update_ui() is called from the render/UI thread.
 */
int cover_pipeline_init(void);
int cover_pipeline_start(void);
void cover_pipeline_stop(void);
void cover_pipeline_shutdown(void);

/*
 * Deduplicated request. Re-requesting the same URL returns the same handle and
 * may raise its priority.
 */
int cover_request(
    const char *url,
    int priority
);

void cover_acquire(int handle);
void cover_release(int handle);
void cover_touch(int handle);

int cover_get(
    int handle,
    CoverInfo *out
);

/*
 * UI/GXM thread only.
 * Converts downloaded JPEG/PNG bytes into vita2d textures and applies LRU
 * eviction to stay under COVER_PIPELINE_MEMORY_BUDGET.
 */
void cover_pipeline_update_ui(void);

size_t cover_pipeline_gpu_bytes(void);

#endif
