#include "spotify_state_worker.h"

#include "spotify_http.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <string.h>

#define STATE_QUEUE_CAPACITY 8
#define STATE_EVENT_WAKE     1u
#define STATE_WORKER_STACK   0x10000
#define STATE_WORKER_PRIORITY 0x10000100

static SceUID g_mutex = -1;
static SceUID g_event = -1;
static SceUID g_thread = -1;
static volatile int g_running = 0;

static SpotifyAuthPkce *g_auth = NULL;

static SpotifyStateResult g_queue[STATE_QUEUE_CAPACITY];
static int g_read = 0;
static int g_write = 0;
static int g_count = 0;

static uint64_t worker_now_ms(void)
{
    return sceKernelGetProcessTimeWide() / 1000u;
}

static void push_result(const SpotifyStateResult *result)
{
    SpotifyStateResult stamped = *result;
    stamped.received_at_ms = worker_now_ms();
    sceKernelLockMutex(g_mutex, 1, NULL);

    /*
     * Latest state is more useful than stale state.
     */
    if (g_count == STATE_QUEUE_CAPACITY) {
        g_read = (g_read + 1) % STATE_QUEUE_CAPACITY;
        --g_count;
    }

    g_queue[g_write] = stamped;
    g_write = (g_write + 1) % STATE_QUEUE_CAPACITY;
    ++g_count;

    sceKernelUnlockMutex(g_mutex, 1);
}

static void publish_http_error(int status, int error)
{
    SpotifyStateResult result;
    memset(&result, 0, sizeof(result));
    result.type = SPOTIFY_STATE_ERROR;
    result.http_status = status;
    result.error = error;
    push_result(&result);
}

static int fetch_playback(void)
{
    SpotifyHttpResponse response;
    int rc = spotify_http_request_api(
        SPOTIFY_HTTP_GET,
        "/v1/me/player",
        NULL,
        NULL,
        &response
    );

    if (rc < 0) {
        publish_http_error(0, rc);
        return rc;
    }

    if (response.status_code == 204) {
        spotify_http_response_free(&response);
        return 0;
    }

    if (response.status_code == 401 && g_auth) {
        spotify_http_response_free(&response);

        rc = spotify_auth_pkce_refresh(g_auth);
        if (rc < 0) {
            publish_http_error(401, rc);
            return rc;
        }

        /*
         * Retry exactly once with the refreshed token.
         */
        rc = spotify_http_request_api(
            SPOTIFY_HTTP_GET,
            "/v1/me/player",
            NULL,
            NULL,
            &response
        );

        if (rc < 0) {
            publish_http_error(0, rc);
            return rc;
        }
    }

    if (response.status_code == 429) {
        int retry_after = response.retry_after > 0 ? response.retry_after : 1;
        spotify_http_response_free(&response);
        sceKernelDelayThread((unsigned int)retry_after * 1000u * 1000u);
        return 0;
    }

    if (response.status_code != 200) {
        int status = response.status_code;
        spotify_http_response_free(&response);
        publish_http_error(status, 0);
        return -status;
    }

    SpotifyStateResult result;
    memset(&result, 0, sizeof(result));

    rc = spotify_json_parse_player(
        response.body,
        response.body_size,
        &result.track
    );

    spotify_http_response_free(&response);

    if (rc == 0) {
        result.type = SPOTIFY_STATE_PLAYBACK;
        push_result(&result);
        return 0;
    }

    if (rc == 1)
        return 0;

    publish_http_error(200, rc);
    return rc;
}

static int fetch_queue(void)
{
    SpotifyHttpResponse response;
    int rc = spotify_http_request_api(
        SPOTIFY_HTTP_GET,
        "/v1/me/player/queue",
        NULL,
        NULL,
        &response
    );

    if (rc < 0) {
        publish_http_error(0, rc);
        return rc;
    }

    if (response.status_code == 401 && g_auth) {
        spotify_http_response_free(&response);

        rc = spotify_auth_pkce_refresh(g_auth);
        if (rc < 0) {
            publish_http_error(401, rc);
            return rc;
        }

        rc = spotify_http_request_api(
            SPOTIFY_HTTP_GET,
            "/v1/me/player/queue",
            NULL,
            NULL,
            &response
        );

        if (rc < 0) {
            publish_http_error(0, rc);
            return rc;
        }
    }

    if (response.status_code == 429) {
        int retry_after = response.retry_after > 0 ? response.retry_after : 1;
        spotify_http_response_free(&response);
        sceKernelDelayThread((unsigned int)retry_after * 1000u * 1000u);
        return 0;
    }

    if (response.status_code != 200) {
        int status = response.status_code;
        spotify_http_response_free(&response);
        publish_http_error(status, 0);
        return -status;
    }

    SpotifyStateResult result;
    memset(&result, 0, sizeof(result));

    rc = spotify_json_parse_queue(
        response.body,
        response.body_size,
        &result.queue
    );

    spotify_http_response_free(&response);

    if (rc == 0) {
        result.type = SPOTIFY_STATE_QUEUE;
        push_result(&result);
        return 0;
    }

    publish_http_error(200, rc);
    return rc;
}

static int state_worker_main(SceSize args, void *arg)
{
    (void)args;
    (void)arg;

    unsigned cycle = 0;

    while (g_running) {
        if (g_auth && g_auth->authenticated) {
            if (spotify_auth_pkce_ensure_valid(g_auth) == 0) {
                fetch_playback();

                /*
                 * Queue changes less frequently than progress.
                 */
                if ((cycle % 3u) == 0)
                    fetch_queue();

                ++cycle;
            }
        }

        /*
         * Wait up to ~2 seconds, but allow explicit wakeups on track changes.
         */
        unsigned int timeout = 2u * 1000u * 1000u;

        sceKernelWaitEventFlag(
            g_event,
            STATE_EVENT_WAKE,
            SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR,
            NULL,
            &timeout
        );
    }

    return 0;
}

int spotify_state_worker_init(
    SpotifyAuthPkce *auth
)
{
    if (!auth)
        return -1;

    g_auth = auth;

    memset(g_queue, 0, sizeof(g_queue));
    g_read = g_write = g_count = 0;

    g_mutex = sceKernelCreateMutex("SpotifyStateMutex", 0, 0, NULL);
    if (g_mutex < 0)
        return g_mutex;

    g_event = sceKernelCreateEventFlag("SpotifyStateEvent", 0, 0, NULL);
    if (g_event < 0) {
        sceKernelDeleteMutex(g_mutex);
        g_mutex = -1;
        return g_event;
    }

    return 0;
}

int spotify_state_worker_start(void)
{
    if (g_thread >= 0)
        return 0;

    g_running = 1;

    g_thread = sceKernelCreateThread(
        "SpotifyStateWorker",
        state_worker_main,
        STATE_WORKER_PRIORITY,
        STATE_WORKER_STACK,
        0,
        0,
        NULL
    );

    if (g_thread < 0) {
        g_running = 0;
        return g_thread;
    }

    int rc = sceKernelStartThread(g_thread, 0, NULL);
    if (rc < 0) {
        sceKernelDeleteThread(g_thread);
        g_thread = -1;
        g_running = 0;
        return rc;
    }

    return 0;
}

void spotify_state_worker_wake(void)
{
    if (g_event >= 0)
        sceKernelSetEventFlag(g_event, STATE_EVENT_WAKE);
}

void spotify_state_worker_stop(void)
{
    if (g_thread < 0)
        return;

    g_running = 0;
    spotify_state_worker_wake();

    sceKernelWaitThreadEnd(g_thread, NULL, NULL);
    sceKernelDeleteThread(g_thread);
    g_thread = -1;
}

void spotify_state_worker_shutdown(void)
{
    spotify_state_worker_stop();

    if (g_event >= 0) {
        sceKernelDeleteEventFlag(g_event);
        g_event = -1;
    }

    if (g_mutex >= 0) {
        sceKernelDeleteMutex(g_mutex);
        g_mutex = -1;
    }

    g_auth = NULL;
}

int spotify_state_worker_poll(
    SpotifyStateResult *out
)
{
    if (!out || g_mutex < 0)
        return 0;

    int found = 0;

    sceKernelLockMutex(g_mutex, 1, NULL);

    if (g_count > 0) {
        *out = g_queue[g_read];
        g_read = (g_read + 1) % STATE_QUEUE_CAPACITY;
        --g_count;
        found = 1;
    }

    sceKernelUnlockMutex(g_mutex, 1);
    return found;
}
