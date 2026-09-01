#include "app_controller.h"

#include "spotify_config.h"
#include "spotify_http.h"
#include "spotify_state_worker.h"
#include "spotify_token_store.h"
#include "vita_browser.h"
#include "vita_network.h"

#include <psp2/kernel/threadmgr.h>

#include <string.h>

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

    spotify_login_init(
        &app->login,
        &app->auth
    );

    rc = spotify_callback_server_init(
        &app->callback,
        &app->login,
        SPOTIFY_CALLBACK_PORT
    );

    if (rc < 0)
        return rc;

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

    /*
     * Clear stale callback state from a previous failed attempt.
     */
    spotify_callback_server_stop(&app->callback);
    app->callback.state = SPOTIFY_CALLBACK_STOPPED;
    app->callback.last_error = 0;
    app->last_error = 0;
    app->last_http_status = 0;
    app->last_error_stage = APP_ERROR_STAGE_NONE;

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

    rc = spotify_callback_server_start(
        &app->callback
    );

    if (rc < 0) {
        app->last_error = rc;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_CALLBACK;
        app->screen = APP_SCREEN_ERROR;
        return rc;
    }

    /*
     * Wait briefly until the callback listener has bound its socket before
     * opening the authorization URL.
     */
    int listening = 0;

    for (int i = 0; i < 200; ++i) {
        SpotifyCallbackServerState state =
            spotify_callback_server_state(
                &app->callback
            );

        if (state == SPOTIFY_CALLBACK_LISTENING) {
            listening = 1;
            break;
        }

        if (state == SPOTIFY_CALLBACK_ERROR) {
            app->last_error = app->callback.last_error;
            app->last_http_status = 0;
            app->last_error_stage = APP_ERROR_STAGE_CALLBACK;
            app->screen = APP_SCREEN_ERROR;
            return app->last_error;
        }

        sceKernelDelayThread(5 * 1000);
    }

    if (!listening) {
        app->last_error = -1001;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_CALLBACK;
        app->screen = APP_SCREEN_ERROR;
        return app->last_error;
    }

    rc = vita_browser_open_url(
        spotify_login_authorization_url(
            &app->login
        )
    );

    if (rc < 0) {
        spotify_callback_server_stop(
            &app->callback
        );

        app->last_error = rc;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_BROWSER;
        app->screen = APP_SCREEN_ERROR;
        return rc;
    }

    app->last_error = 0;
    app->last_http_status = 0;
    app->last_error_stage = APP_ERROR_STAGE_NONE;
    app->last_error_stage = APP_ERROR_STAGE_NONE;
    app->login_started = 1;
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

    SpotifyCallbackServerState callback_state =
        spotify_callback_server_state(
            &app->callback
        );

    if (callback_state == SPOTIFY_CALLBACK_RECEIVED &&
        app->auth.authenticated) {

        if (!app->session_saved) {
            spotify_token_store_save(
                SPOTIFY_TOKEN_STORE_PATH,
                &app->auth
            );

            app->session_saved = 1;
        }

        app->login_started = 0;
        app->screen = APP_SCREEN_HOME;

        spotify_state_worker_wake();
    }

    if (app->login_started &&
        callback_state == SPOTIFY_CALLBACK_ERROR) {
        app->last_error = app->callback.last_error;
        app->last_http_status = 0;
        app->last_error_stage = APP_ERROR_STAGE_CALLBACK;
        app->screen = APP_SCREEN_ERROR;
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

    /*
     * Stop/reset the failed callback worker BEFORE changing screen.
     * Otherwise app_controller_update() sees CALLBACK_ERROR on the next
     * frame and immediately sends the UI back to APP_SCREEN_ERROR.
     */
    spotify_callback_server_stop(&app->callback);
    app->callback.state = SPOTIFY_CALLBACK_STOPPED;
    app->callback.last_error = 0;

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

    spotify_callback_server_stop(
        &app->callback
    );

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

void app_controller_shutdown(
    AppController *app
)
{
    if (!app || !app->initialized)
        return;

    spotify_state_worker_shutdown();

    spotify_callback_server_shutdown(
        &app->callback
    );

    if (app->auth.authenticated) {
        spotify_token_store_save(
            SPOTIFY_TOKEN_STORE_PATH,
            &app->auth
        );
    }

    app->initialized = 0;
}
