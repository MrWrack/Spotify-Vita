#include "cover_pipeline.h"

#include "spotify_http.h"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include <stdlib.h>
#include <string.h>

#define COVER_WORKER_STACK    0x10000
#define COVER_WORKER_PRIORITY 0x10000100

typedef struct {
    int used;
    int handle;
    CoverState state;
    int priority;
    int refs;

    int http_status;
    int error;

    uint64_t last_used_ms;

    char url[1024];

    unsigned char *download;
    size_t download_size;

    vita2d_texture *texture;
    unsigned int width;
    unsigned int height;
    size_t gpu_bytes;
} CoverEntry;

static CoverEntry g_entries[COVER_PIPELINE_MAX_ITEMS];

static SceUID g_mutex = -1;
static SceUID g_event = -1;
static SceUID g_thread = -1;

static volatile int g_running = 0;
static int g_next_handle = 1;
static size_t g_gpu_bytes = 0;

static uint64_t now_ms(void)
{
    return sceKernelGetProcessTimeWide() / 1000u;
}

static void lock(void)
{
    if (g_mutex >= 0)
        sceKernelLockMutex(g_mutex, 1, NULL);
}

static void unlock(void)
{
    if (g_mutex >= 0)
        sceKernelUnlockMutex(g_mutex, 1);
}

static int find_by_url_locked(const char *url)
{
    for (int i = 0; i < COVER_PIPELINE_MAX_ITEMS; ++i) {
        if (g_entries[i].used &&
            strcmp(g_entries[i].url, url) == 0)
            return i;
    }

    return -1;
}

static int find_by_handle_locked(int handle)
{
    for (int i = 0; i < COVER_PIPELINE_MAX_ITEMS; ++i) {
        if (g_entries[i].used &&
            g_entries[i].handle == handle)
            return i;
    }

    return -1;
}

static int choose_slot_locked(void)
{
    for (int i = 0; i < COVER_PIPELINE_MAX_ITEMS; ++i) {
        if (!g_entries[i].used)
            return i;
    }

    /*
     * Prefer an unreferenced READY/ERROR entry with the oldest touch time.
     * Active queued/downloaded items are not overwritten.
     */
    int best = -1;
    uint64_t best_time = UINT64_MAX;

    for (int i = 0; i < COVER_PIPELINE_MAX_ITEMS; ++i) {
        CoverEntry *e = &g_entries[i];

        if (e->refs != 0)
            continue;

        if (e->state != COVER_STATE_READY &&
            e->state != COVER_STATE_ERROR)
            continue;

        if (e->last_used_ms < best_time) {
            best_time = e->last_used_ms;
            best = i;
        }
    }

    return best;
}

static int choose_download_locked(void)
{
    int best = -1;
    int best_priority = -2147483647;
    uint64_t oldest = UINT64_MAX;

    for (int i = 0; i < COVER_PIPELINE_MAX_ITEMS; ++i) {
        CoverEntry *e = &g_entries[i];

        if (!e->used ||
            e->state != COVER_STATE_QUEUED)
            continue;

        if (e->priority > best_priority ||
            (e->priority == best_priority &&
             e->last_used_ms < oldest)) {
            best = i;
            best_priority = e->priority;
            oldest = e->last_used_ms;
        }
    }

    return best;
}

static void destroy_entry_ui_locked(CoverEntry *e)
{
    if (!e)
        return;

    if (e->texture) {
        vita2d_free_texture(e->texture);

        if (g_gpu_bytes >= e->gpu_bytes)
            g_gpu_bytes -= e->gpu_bytes;
        else
            g_gpu_bytes = 0;
    }

    free(e->download);
    memset(e, 0, sizeof(*e));
}

static int image_kind(
    const unsigned char *data,
    size_t size
)
{
    if (!data || size < 4)
        return 0;

    if (size >= 8 &&
        data[0] == 0x89 &&
        data[1] == 0x50 &&
        data[2] == 0x4e &&
        data[3] == 0x47 &&
        data[4] == 0x0d &&
        data[5] == 0x0a &&
        data[6] == 0x1a &&
        data[7] == 0x0a)
        return 1; /* PNG */

    if (data[0] == 0xff &&
        data[1] == 0xd8 &&
        data[2] == 0xff)
        return 2; /* JPEG */

    return 0;
}

static vita2d_texture *decode_texture_ui(
    const unsigned char *data,
    size_t size
)
{
    int kind = image_kind(data, size);

    if (kind == 1) {
        /*
         * libvita2d's PNG buffer API does not accept a byte length.
         * The HTTP allocation remains alive for the complete call.
         */
        return vita2d_load_PNG_buffer(data);
    }

    if (kind == 2) {
        return vita2d_load_JPEG_buffer(
            data,
            (unsigned long)size
        );
    }

    return NULL;
}

static int worker_main(SceSize args, void *arg)
{
    (void)args;
    (void)arg;

    while (g_running) {
        int index = -1;
        char url[1024];

        lock();

        index = choose_download_locked();

        if (index >= 0) {
            CoverEntry *e = &g_entries[index];
            e->state = COVER_STATE_DOWNLOADING;

            strncpy(
                url,
                e->url,
                sizeof(url) - 1
            );
            url[sizeof(url) - 1] = '\0';
        }

        unlock();

        if (index < 0) {
            unsigned int bits = 0;

            sceKernelWaitEventFlag(
                g_event,
                1,
                SCE_EVENT_WAITOR |
                SCE_EVENT_WAITCLEAR_PAT,
                &bits,
                NULL
            );

            continue;
        }

        SpotifyHttpResponse response;
        memset(&response, 0, sizeof(response));

        int rc = spotify_http_request_absolute(
            SPOTIFY_HTTP_GET,
            url,
            NULL,
            NULL,
            NULL,
            &response
        );

        lock();

        CoverEntry *e = &g_entries[index];

        /*
         * The slot cannot be repurposed while DOWNLOADING, but still verify
         * its URL before applying the result.
         */
        if (e->used &&
            e->state == COVER_STATE_DOWNLOADING &&
            strcmp(e->url, url) == 0) {

            e->http_status = response.status_code;

            if (rc == 0 &&
                response.status_code >= 200 &&
                response.status_code < 300 &&
                response.body &&
                response.body_size > 0) {

                e->download =
                    (unsigned char *)response.body;

                e->download_size =
                    response.body_size;

                response.body = NULL;
                response.body_size = 0;

                e->state =
                    COVER_STATE_DOWNLOADED;

                e->error = 0;
            } else {
                e->state =
                    COVER_STATE_ERROR;

                e->error =
                    rc != 0
                    ? rc
                    : -response.status_code;
            }
        }

        unlock();

        spotify_http_response_free(
            &response
        );
    }

    return 0;
}

int cover_pipeline_init(void)
{
    memset(
        g_entries,
        0,
        sizeof(g_entries)
    );

    g_gpu_bytes = 0;
    g_next_handle = 1;

    g_mutex = sceKernelCreateMutex(
        "CoverPipelineMutex",
        0,
        0,
        NULL
    );

    if (g_mutex < 0)
        return g_mutex;

    g_event = sceKernelCreateEventFlag(
        "CoverPipelineEvent",
        0,
        0,
        NULL
    );

    if (g_event < 0) {
        sceKernelDeleteMutex(g_mutex);
        g_mutex = -1;
        return g_event;
    }

    return 0;
}

int cover_pipeline_start(void)
{
    if (g_thread >= 0)
        return 0;

    g_running = 1;

    g_thread = sceKernelCreateThread(
        "CoverPipelineWorker",
        worker_main,
        COVER_WORKER_PRIORITY,
        COVER_WORKER_STACK,
        0,
        0,
        NULL
    );

    if (g_thread < 0) {
        g_running = 0;
        return g_thread;
    }

    int rc = sceKernelStartThread(
        g_thread,
        0,
        NULL
    );

    if (rc < 0) {
        sceKernelDeleteThread(g_thread);
        g_thread = -1;
        g_running = 0;
        return rc;
    }

    return 0;
}

int cover_request(
    const char *url,
    int priority
)
{
    if (!url || !url[0])
        return -1;

    lock();

    int index =
        find_by_url_locked(url);

    if (index >= 0) {
        CoverEntry *e =
            &g_entries[index];

        if (priority > e->priority)
            e->priority = priority;

        e->last_used_ms = now_ms();

        int handle = e->handle;
        unlock();

        sceKernelSetEventFlag(g_event, 1);
        return handle;
    }

    index = choose_slot_locked();

    if (index < 0) {
        unlock();
        return -2;
    }

    CoverEntry *slot =
        &g_entries[index];

    /*
     * Slot eviction is safe here only if it contains no GPU texture. READY
     * textures are evicted by update_ui(), which is the GXM thread.
     */
    if (slot->texture) {
        unlock();
        return -3;
    }

    free(slot->download);
    memset(slot, 0, sizeof(*slot));

    slot->used = 1;
    slot->handle = g_next_handle++;
    slot->state = COVER_STATE_QUEUED;
    slot->priority = priority;
    slot->last_used_ms = now_ms();

    strncpy(
        slot->url,
        url,
        sizeof(slot->url) - 1
    );

    int handle = slot->handle;

    unlock();

    sceKernelSetEventFlag(
        g_event,
        1
    );

    return handle;
}

void cover_acquire(int handle)
{
    lock();

    int index =
        find_by_handle_locked(handle);

    if (index >= 0) {
        ++g_entries[index].refs;
        g_entries[index].last_used_ms =
            now_ms();
    }

    unlock();
}

void cover_release(int handle)
{
    lock();

    int index =
        find_by_handle_locked(handle);

    if (index >= 0) {
        if (g_entries[index].refs > 0)
            --g_entries[index].refs;

        g_entries[index].last_used_ms =
            now_ms();
    }

    unlock();
}

void cover_touch(int handle)
{
    lock();

    int index =
        find_by_handle_locked(handle);

    if (index >= 0)
        g_entries[index].last_used_ms =
            now_ms();

    unlock();
}

int cover_get(
    int handle,
    CoverInfo *out
)
{
    if (!out)
        return -1;

    memset(out, 0, sizeof(*out));

    lock();

    int index =
        find_by_handle_locked(handle);

    if (index < 0) {
        unlock();
        return -2;
    }

    CoverEntry *e =
        &g_entries[index];

    out->handle = e->handle;
    out->state = e->state;
    out->priority = e->priority;
    out->http_status = e->http_status;
    out->error = e->error;
    out->texture = e->texture;
    out->width = e->width;
    out->height = e->height;

    strncpy(
        out->url,
        e->url,
        sizeof(out->url) - 1
    );

    unlock();
    return 0;
}

static void enforce_budget_ui_locked(void)
{
    while (g_gpu_bytes >
           COVER_PIPELINE_MEMORY_BUDGET) {

        int victim = -1;
        uint64_t oldest = UINT64_MAX;

        for (int i = 0; i < COVER_PIPELINE_MAX_ITEMS; ++i) {
            CoverEntry *e = &g_entries[i];

            if (!e->used ||
                e->state != COVER_STATE_READY ||
                !e->texture ||
                e->refs != 0)
                continue;

            if (e->last_used_ms < oldest) {
                oldest = e->last_used_ms;
                victim = i;
            }
        }

        if (victim < 0)
            break;

        destroy_entry_ui_locked(
            &g_entries[victim]
        );
    }
}

void cover_pipeline_update_ui(void)
{
    /*
     * This function MUST run on the render/UI thread. It is the only place
     * where downloaded bytes become or destroy vita2d/GXM textures.
     */
    lock();

    for (int i = 0; i < COVER_PIPELINE_MAX_ITEMS; ++i) {
        CoverEntry *e =
            &g_entries[i];

        if (!e->used ||
            e->state != COVER_STATE_DOWNLOADED ||
            !e->download ||
            e->download_size == 0)
            continue;

        unsigned char *bytes =
            e->download;

        size_t size =
            e->download_size;

        /*
         * Temporarily unlock while the decoder performs CPU/GPU allocations.
         * State remains DOWNLOADED, so the worker will not touch this entry.
         */
        unlock();

        vita2d_texture *texture =
            decode_texture_ui(
                bytes,
                size
            );

        lock();

        if (!e->used ||
            e->state != COVER_STATE_DOWNLOADED ||
            e->download != bytes) {
            if (texture)
                vita2d_free_texture(texture);
            continue;
        }

        free(e->download);
        e->download = NULL;
        e->download_size = 0;

        if (!texture) {
            e->state = COVER_STATE_ERROR;
            e->error = -2001;
            continue;
        }

        e->texture = texture;
        e->width =
            vita2d_texture_get_width(texture);
        e->height =
            vita2d_texture_get_height(texture);

        /*
         * Conservative GPU memory accounting. Exact allocator overhead is
         * intentionally not assumed.
         */
        e->gpu_bytes =
            (size_t)e->width *
            (size_t)e->height *
            4u;

        g_gpu_bytes +=
            e->gpu_bytes;

        e->state = COVER_STATE_READY;
        e->last_used_ms = now_ms();
    }

    enforce_budget_ui_locked();

    unlock();
}

size_t cover_pipeline_gpu_bytes(void)
{
    lock();
    size_t value = g_gpu_bytes;
    unlock();
    return value;
}

void cover_pipeline_stop(void)
{
    if (g_thread < 0)
        return;

    g_running = 0;

    sceKernelSetEventFlag(
        g_event,
        1
    );

    sceKernelWaitThreadEnd(
        g_thread,
        NULL,
        NULL
    );

    sceKernelDeleteThread(
        g_thread
    );

    g_thread = -1;
}

void cover_pipeline_shutdown(void)
{
    cover_pipeline_stop();

    /*
     * Must be called from the UI/render thread because READY entries own
     * vita2d textures.
     */
    lock();

    for (int i = 0; i < COVER_PIPELINE_MAX_ITEMS; ++i)
        destroy_entry_ui_locked(
            &g_entries[i]
        );

    unlock();

    if (g_event >= 0) {
        sceKernelDeleteEventFlag(g_event);
        g_event = -1;
    }

    if (g_mutex >= 0) {
        sceKernelDeleteMutex(g_mutex);
        g_mutex = -1;
    }
}
