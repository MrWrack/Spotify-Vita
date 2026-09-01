#include "app_controller.h"

#include "spotify_config.h"
#include "spotify_http.h"
#include "spotify_search.h"
#include "spotify_playback.h"
#include "spotify_state_worker.h"
#include "spotify_token_store.h"
#include "spotify_token_import.h"
#include "vita_browser.h"
#include "vita_network.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/appmgr.h>

#include <string.h>
#include <stdio.h>


static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_decode_in_place(char *text)
{
    if (!text)
        return;

    char *src = text;
    char *dst = text;

    while (*src) {
        if (src[0] == '%' && src[1] && src[2]) {
            int hi = hex_value(src[1]);
            int lo = hex_value(src[2]);

            if (hi >= 0 && lo >= 0) {
                *dst++ = (char)((hi << 4) | lo);
                src += 3;
                continue;
            }
        }

        /*
         * For an encoded query parameter '+' commonly represents a space.
         * A Spotify authorization code/state does not need literal spaces.
         */
        if (*src == '+')
            *dst++ = ' ';
        else
            *dst++ = *src;

        ++src;
    }

    *dst = '\0';
}

/*
 * Read a URI that launched/resumed this Vita application.
 *
 * VitaSDK documents app params such as:
 *   type=LAUNCH_APP_BY_URI&uri=psgm:play?titleid=...
 *
 * Return:
 *   1 = Spotify callback URI found
 *   0 = no callback present
 *  <0 = AppMgr error
 */
static int copy_param_value(
    const char *text,
    const char *name,
    char *out,
    size_t out_size
)
{
    if (!text || !name || !out || out_size == 0)
        return -1;

    size_t name_len = strlen(name);
    const char *p = text;

    while ((p = strstr(p, name)) != NULL) {
        /*
         * Parameter must begin at start or after ?, &, or whitespace.
         */
        if (p != text &&
            p[-1] != '?' &&
            p[-1] != '&' &&
            p[-1] != ' ' &&
            p[-1] != '\n' &&
            p[-1] != '\r') {
            p += name_len;
            continue;
        }

        const char *value = p + name_len;
        const char *end = value;

        while (*end &&
               *end != '&' &&
               *end != '\n' &&
               *end != '\r')
            ++end;

        size_t len = (size_t)(end - value);
        if (len == 0 || len >= out_size)
            return -2;

        memcpy(out, value, len);
        out[len] = '\0';
        return 0;
    }

    return 1;
}

static int app_controller_read_spotify_callback_uri(
    char *callback_uri,
    size_t callback_uri_size
)
{
    if (!callback_uri || callback_uri_size == 0)
        return -1;

    char param[1024];
    memset(param, 0, sizeof(param));

    int rc = sceAppMgrGetAppParam(param);
    if (rc < 0)
        return rc;

    /*
     * Vita can expose LAUNCH_APP_BY_URI in slightly different layouts:
     *
     *   type=LAUNCH_APP_BY_URI&uri=psgm:play?titleid=SPVT00001&code=...&state=...
     *
     * or the uri field itself can be percent encoded.
     *
     * Instead of assuming everything after "uri=" belongs to the URI, first
     * extract URI/code/state separately. This also avoids corrupting encoded
     * OAuth values by URL-decoding the complete AppMgr parameter string.
     */
    char uri[768];
    char code[256];
    char state[192];
    char error[192];

    memset(uri, 0, sizeof(uri));
    memset(code, 0, sizeof(code));
    memset(state, 0, sizeof(state));
    memset(error, 0, sizeof(error));

    rc = copy_param_value(param, "uri=", uri, sizeof(uri));
    if (rc != 0)
        return 0;

    url_decode_in_place(uri);

    if (strncmp(
            uri,
            "psgm:play?titleid=SPVT00001",
            strlen("psgm:play?titleid=SPVT00001")) != 0)
        return 0;

    /*
     * First try callback values already embedded in the URI.
     * spotify_auth_pkce_parse_callback() will decode them later.
     */
    const char *embedded_code = strstr(uri, "&code=");
    const char *embedded_error = strstr(uri, "&error=");

    if (embedded_code || embedded_error) {
        size_t len = strlen(uri);
        if (len >= callback_uri_size)
            return -2;

        memcpy(callback_uri, uri, len + 1);
        return 1;
    }

    /*
     * Some AppMgr launch strings expose appended query fields as top-level
     * app parameters after the uri field. Reconstruct a canonical callback
     * URL for the existing PKCE parser.
     */
    int have_code =
        copy_param_value(param, "code=", code, sizeof(code)) == 0;
    int have_state =
        copy_param_value(param, "state=", state, sizeof(state)) == 0;
    int have_error =
        copy_param_value(param, "error=", error, sizeof(error)) == 0;

    if (!have_state || (!have_code && !have_error))
        return 0;

    int n;

    if (have_code) {
        n = snprintf(
            callback_uri,
            callback_uri_size,
            "psgm:play?titleid=SPVT00001&code=%s&state=%s",
            code,
            state
        );
    } else {
        n = snprintf(
            callback_uri,
            callback_uri_size,
            "psgm:play?titleid=SPVT00001&error=%s&state=%s",
            error,
            state
        );
    }

    if (n < 0 || (size_t)n >= callback_uri_size)
        return -3;

    return 1;
}

static int app_controller_process_app_uri_callback(
    AppController *app
)
{
    if (!app)
        return -1;

    char callback_uri[1024];
    int found = app_controller_read_spotify_callback_uri(
        callback_uri,
        sizeof(callback_uri)
    );

    if (found <= 0)
        return found;

    int rc = spotify_login_accept_callback_url(
        &app->login,
        callback_uri
    );

    /*
     * Do not keep OAuth material in this stack buffer longer than needed.
     */
    memset(callback_uri, 0, sizeof(callback_uri));

    if (rc < 0) {
        app->last_error = rc;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_APP_URI_CALLBACK;
        app->screen = APP_SCREEN_ERROR;
        app->login_started = 0;
        return rc;
    }

    if (app->auth.authenticated) {
        spotify_token_store_save(
            SPOTIFY_TOKEN_STORE_PATH,
            &app->auth
        );

        app->session_saved = 1;
        app->login_started = 0;
        app->last_error = 0;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_NONE;
        app->screen = APP_SCREEN_HOME;

        spotify_state_worker_wake();
    }

    return 1;
}

int app_controller_init(
    AppController *app
)
{
    if (!app)
        return -1;

    memset(app, 0, sizeof(*app));

    int rc = spotify_auth_pkce_init(
        &app->auth,
        SPOTIFY_CLIENT_ID,
        SPOTIFY_REDIRECT_URI
    );

    if (rc < 0)
        return rc;

    int stored = spotify_token_store_load(
        SPOTIFY_TOKEN_STORE_PATH,
        &app->auth
    );
    /* v26: import tokens created by the PC login helper. */
    if (spotify_token_import_try(&app->auth) > 0) {
        spotify_token_store_save(
            SPOTIFY_TOKEN_STORE_PATH,
            &app->auth
        );
        stored = 0;
        app->session_saved = 1;
    }

    spotify_login_init(
        &app->login,
        &app->auth
    );

    now_playing_init(
        &app->now_playing
    );

    rc = spotify_state_worker_init(
        &app->auth
    );

    if (rc < 0)
        return rc;

    rc = spotify_state_worker_start();
    if (rc < 0) {
        spotify_state_worker_shutdown();
        return rc;
    }

    app->screen =
        (stored == 0 && app->auth.authenticated)
        ? APP_SCREEN_HOME
        : APP_SCREEN_LOGIN;

    app->selected_nav = 0;
    app->login_focus = 0;
    app->search_query[0] = '\0';
    app->search_result_count = 0;
    app->search_selected = 0;
    app->search_keyboard_index = 0;
    app->search_focus_results = 0;
    app->search_last_error = 0;
    app->search_last_http_status = 0;

    app->network_state = 0;
    app->network_connected =
        vita_network_is_connected();
    vita_network_get_state(&app->network_state);

    app->initialized = 1;

    /*
     * A restored access token may be stale. Wake the worker immediately;
     * ensure_valid() will refresh using the stored refresh token if needed.
     */
    if (app->auth.authenticated)
        spotify_state_worker_wake();

    return 0;
}

int app_controller_begin_login(
    AppController *app
)
{
    if (!app || !app->initialized)
        return -1;

    if (app->login_started)
        return 0;

    app->network_state = 0;
    app->network_connected =
        vita_network_is_connected();
    vita_network_get_state(&app->network_state);

    if (!app->network_connected) {
        app->last_error = -2001;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_NETWORK;
        app->screen = APP_SCREEN_ERROR;
        return app->last_error;
    }

    app->last_error = 0;
    app->last_http_status = 0;
    app->last_error_stage = APP_ERROR_STAGE_NONE;

    /*
     * PKCE creates verifier/challenge/state and the Spotify authorize URL.
     * v19 does NOT open a local listening socket.
     */
    int rc = spotify_login_begin(
        &app->login,
        SPOTIFY_SCOPES
    );

    if (rc < 0) {
        app->last_error = rc;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_LOGIN;
        app->screen = APP_SCREEN_ERROR;
        return rc;
    }

    /*
     * Mark login active before the browser opens because the Vita can
     * suspend this app while the system browser is in front.
     */
    app->login_started = 1;

    rc = vita_browser_open_url(
        spotify_login_authorization_url(
            &app->login
        )
    );

    if (rc < 0) {
        app->login_started = 0;
        app->last_error = rc;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_BROWSER;
        app->screen = APP_SCREEN_ERROR;
        return rc;
    }

    return 0;
}

void app_controller_update(
    AppController *app
)
{
    if (!app || !app->initialized)
        return;

    app->network_state = 0;
    app->network_connected =
        vita_network_is_connected();
    vita_network_get_state(&app->network_state);


    /*
     * When Spotify redirects to:
     *   psgm:play?titleid=SPVT00001&code=...&state=...
     * the Vita resumes/launches this title. AppMgr exposes that URI here.
     */
    if (app->login_started ||
        app->login.state == SPOTIFY_LOGIN_NEEDS_BROWSER ||
        app->login.state == SPOTIFY_LOGIN_WAITING_CALLBACK) {
        app_controller_process_app_uri_callback(app);
    }

    SpotifyStateResult state;

    while (spotify_state_worker_poll(&state)) {
        now_playing_apply_state(
            &app->now_playing,
            &state
        );

        if (state.type == SPOTIFY_STATE_PLAYBACK &&
            state.track.valid) {
            /*
             * Once cover_pipeline.c is present:
             *
             * cover_request(state.track.cover_url, 100);
             */
        }

        if (state.type == SPOTIFY_STATE_ERROR) {
            app->last_error = state.error;
            app->last_http_status =
                state.http_status;
            app->last_error_stage = APP_ERROR_STAGE_SPOTIFY_HTTP;
        }
    }
}


void app_controller_return_to_login(
    AppController *app
)
{
    if (!app)
        return;

    app->login_started = 0;
    app->last_error = 0;
    app->last_http_status = 0;
    app->last_error_stage = APP_ERROR_STAGE_NONE;
    app->login_focus = 0;
    app->screen = APP_SCREEN_LOGIN;
}

void app_controller_logout(
    AppController *app
)
{
    if (!app)
        return;
    spotify_auth_pkce_clear(
        &app->auth
    );

    spotify_token_store_delete(
        SPOTIFY_TOKEN_STORE_PATH
    );

    app->login_started = 0;
    app->session_saved = 0;
    app->last_error = 0;
    app->last_http_status = 0;
    app->last_error_stage = APP_ERROR_STAGE_NONE;

    now_playing_init(
        &app->now_playing
    );

    spotify_login_init(
        &app->login,
        &app->auth
    );

    app->screen = APP_SCREEN_LOGIN;
}


int app_controller_search(
    AppController *app
)
{
    if (!app || !app->initialized)
        return -1;

    if (!app->search_query[0]) {
        app->search_last_error = -4001;
        app->search_last_http_status = 0;
        return -4001;
    }

    memset(
        app->search_results,
        0,
        sizeof(app->search_results)
    );

    app->search_result_count = 0;
    app->search_selected = 0;
    app->search_last_error = 0;
    app->search_last_http_status = 0;

    int rc = spotify_search_tracks(
        &app->auth,
        app->search_query,
        app->search_results,
        APP_SEARCH_MAX_RESULTS,
        &app->search_result_count,
        &app->search_last_http_status
    );

    if (rc < 0) {
        app->search_last_error = rc;
        return rc;
    }

    if (app->search_result_count > 0)
        app->search_focus_results = 1;

    return 0;
}

int app_controller_search_play_selected(
    AppController *app
)
{
    if (!app || app->search_result_count <= 0)
        return -1;

    if (app->search_selected < 0 ||
        app->search_selected >= app->search_result_count)
        return -2;

    SpotifyTrack *track =
        &app->search_results[app->search_selected];

    if (!track->valid || !track->uri[0])
        return -3;

    char encoded_uri[256];
    size_t w = 0;
    static const char hex[] = "0123456789ABCDEF";

    for (const unsigned char *p = (const unsigned char *)track->uri;
         *p && w + 3 < sizeof(encoded_uri);
         ++p) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            encoded_uri[w++] = (char)c;
        } else {
            encoded_uri[w++] = '%';
            encoded_uri[w++] = hex[(c >> 4) & 0x0F];
            encoded_uri[w++] = hex[c & 0x0F];
        }
    }
    encoded_uri[w] = '\0';

    char path[320];
    snprintf(path, sizeof(path), "/play?uri=%s", encoded_uri);

    int http_status = 0;
    int rc = spotify_proxy_simple_get(path, &http_status);

    if (rc == 0)
        spotify_state_worker_wake();

    return rc;
}


void app_controller_shutdown(
    AppController *app
)
{
    if (!app || !app->initialized)
        return;

    spotify_state_worker_shutdown();
    if (app->auth.authenticated) {
        spotify_token_store_save(
            SPOTIFY_TOKEN_STORE_PATH,
            &app->auth
        );
    }

    app->initialized = 0;
}
