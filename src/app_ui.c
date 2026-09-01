#include "app_ui.h"

#include "spotify_playback.h"
#include "spotify_state_worker.h"

#include <psp2/ctrl.h>
#include <string.h>

static const char g_search_keys[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";

#define SEARCH_KEY_COUNT 37
#define SEARCH_KEY_COLS 8

static void search_key_move(
    AppController *app,
    int delta
)
{
    int idx = app->search_keyboard_index;
    int next = idx + delta;

    while (next < 0)
        next += SEARCH_KEY_COUNT;
    while (next >= SEARCH_KEY_COUNT)
        next -= SEARCH_KEY_COUNT;

    app->search_keyboard_index = next;
}

static void search_append_selected_key(
    AppController *app
)
{
    size_t len = strlen(app->search_query);

    if (len + 1 >= sizeof(app->search_query))
        return;

    char c = g_search_keys[app->search_keyboard_index];
    app->search_query[len] = c;
    app->search_query[len + 1] = '\0';
}

static void search_backspace(
    AppController *app
)
{
    size_t len = strlen(app->search_query);
    if (len > 0)
        app->search_query[len - 1] = '\0';
}


static int touch_nav_index(const AppInput *input)
{
    if (!input || !input->touch_active)
        return -1;

    if (input->touch_x < 20 || input->touch_x > 300)
        return -1;

    const int top = 108;
    const int row_h = 58;
    int idx = (input->touch_y - top) / row_h;

    if (idx < 0 || idx > 3)
        return -1;

    return idx;
}

UiAction app_ui_action_from_input(
    const AppController *app,
    const AppInput *input
)
{
    if (!app || !input)
        return UI_ACTION_NONE;

    if (app->screen == APP_SCREEN_LOGIN) {
        /*
         * Only Spotify Login is visible/selectable.
         * D-pad keeps focus on it.
         * X activates login.
         * O works as Back with NO on-screen Back text/symbol.
         */
        if (input->buttons_down &
            (SCE_CTRL_UP |
             SCE_CTRL_DOWN |
             SCE_CTRL_LEFT |
             SCE_CTRL_RIGHT)) {
            return UI_ACTION_NONE;
        }

        if (input->buttons_down & SCE_CTRL_CIRCLE)
            return UI_ACTION_BACK;

        if (input->buttons_down & SCE_CTRL_CROSS)
            return UI_ACTION_LOGIN;

        if (input->touch_active &&
            input->touch_x >= 290 &&
            input->touch_x <= 670 &&
            input->touch_y >= 215 &&
            input->touch_y <= 310)
            return UI_ACTION_LOGIN;

        return UI_ACTION_NONE;
    }

    if (app->screen == APP_SCREEN_NOW_PLAYING) {
        if (input->buttons_down & SCE_CTRL_CIRCLE)
            return UI_ACTION_BACK;

        if (input->buttons_down & SCE_CTRL_LEFT)
            return UI_ACTION_PREVIOUS;

        if (input->buttons_down & SCE_CTRL_RIGHT)
            return UI_ACTION_NEXT;

        if (input->buttons_down & SCE_CTRL_CROSS)
            return UI_ACTION_PLAY_PAUSE;

        if (input->touch_active) {
            if (input->touch_y >= 330 && input->touch_y <= 430) {
                if (input->touch_x < 560)
                    return UI_ACTION_PREVIOUS;
                if (input->touch_x > 735)
                    return UI_ACTION_NEXT;
                return UI_ACTION_PLAY_PAUSE;
            }
        }

        return UI_ACTION_NONE;
    }

    if (app->screen == APP_SCREEN_SEARCH) {
        if (input->buttons_down & SCE_CTRL_CIRCLE)
            return UI_ACTION_BACK;

        if (input->buttons_down & SCE_CTRL_TRIANGLE)
            return UI_ACTION_SEARCH_EXECUTE;

        if (input->buttons_down & SCE_CTRL_SQUARE)
            return UI_ACTION_SEARCH_BACKSPACE;

        if (input->buttons_down & SCE_CTRL_LTRIGGER)
            return UI_ACTION_SEARCH_FOCUS_KEYBOARD;

        if (input->buttons_down & SCE_CTRL_RTRIGGER)
            return UI_ACTION_SEARCH_FOCUS_RESULTS;

        if (app->search_focus_results) {
            if (input->buttons_down & SCE_CTRL_UP)
                return UI_ACTION_SEARCH_RESULT_UP;
            if (input->buttons_down & SCE_CTRL_DOWN)
                return UI_ACTION_SEARCH_RESULT_DOWN;
            if (input->buttons_down & SCE_CTRL_CROSS)
                return UI_ACTION_SEARCH_PLAY_SELECTED;
        } else {
            if (input->buttons_down & SCE_CTRL_LEFT)
                return UI_ACTION_SEARCH_KEY_LEFT;
            if (input->buttons_down & SCE_CTRL_RIGHT)
                return UI_ACTION_SEARCH_KEY_RIGHT;
            if (input->buttons_down & SCE_CTRL_UP)
                return UI_ACTION_SEARCH_KEY_UP;
            if (input->buttons_down & SCE_CTRL_DOWN)
                return UI_ACTION_SEARCH_KEY_DOWN;
            if (input->buttons_down & SCE_CTRL_CROSS)
                return UI_ACTION_SEARCH_KEY_SELECT;
        }

        if (input->touch_active &&
            input->touch_y >= 458)
            return UI_ACTION_OPEN_NOW_PLAYING;

        return UI_ACTION_NONE;
    }

    if (app->screen == APP_SCREEN_LIBRARY ||
        app->screen == APP_SCREEN_SETTINGS) {
        if (input->buttons_down & SCE_CTRL_CIRCLE)
            return UI_ACTION_BACK;

        if (input->touch_active &&
            input->touch_y >= 458)
            return UI_ACTION_OPEN_NOW_PLAYING;

        return UI_ACTION_NONE;
    }

    if (app->screen == APP_SCREEN_ERROR) {
        if (input->buttons_down & SCE_CTRL_CIRCLE)
            return UI_ACTION_BACK;

        if (input->buttons_down & SCE_CTRL_CROSS)
            return UI_ACTION_BACK;

        return UI_ACTION_NONE;
    }

    if (app->screen == APP_SCREEN_HOME) {
        if (input->buttons_down & SCE_CTRL_UP)
            return UI_ACTION_NAV_UP;

        if (input->buttons_down & SCE_CTRL_DOWN)
            return UI_ACTION_NAV_DOWN;

        if (input->buttons_down & SCE_CTRL_CROSS)
            return UI_ACTION_SELECT_NAV;

        if (input->buttons_down & SCE_CTRL_LTRIGGER)
            return UI_ACTION_PREVIOUS;

        if (input->buttons_down & SCE_CTRL_RTRIGGER)
            return UI_ACTION_NEXT;

        if (input->touch_active && input->touch_y >= 458)
            return UI_ACTION_OPEN_NOW_PLAYING;

        if (touch_nav_index(input) >= 0)
            return UI_ACTION_SELECT_NAV;

        return UI_ACTION_NONE;
    }

    return UI_ACTION_NONE;
}

int app_ui_execute_action(
    AppController *app,
    UiAction action
)
{
    if (!app)
        return -1;

    int rc = 0;

    switch (action) {
        case UI_ACTION_LOGIN:
            return app_controller_begin_login(app);

        case UI_ACTION_FOCUS_LOGIN:
            app->login_focus = 0;
            return 0;

        case UI_ACTION_OPEN_NOW_PLAYING:
            app->screen = APP_SCREEN_NOW_PLAYING;
            return 0;

        case UI_ACTION_NAV_UP:
            if (app->screen == APP_SCREEN_LOGIN) {
                app->login_focus =
                    (app->login_focus + 1) % 2;
            } else {
                app->selected_nav =
                    (app->selected_nav + 3) % 4;
            }
            return 0;

        case UI_ACTION_NAV_DOWN:
            if (app->screen == APP_SCREEN_LOGIN) {
                app->login_focus =
                    (app->login_focus + 1) % 2;
            } else {
                app->selected_nav =
                    (app->selected_nav + 1) % 4;
            }
            return 0;

        case UI_ACTION_SELECT_NAV:
            switch (app->selected_nav) {
                case 0:
                    app->screen = APP_SCREEN_HOME;
                    break;
                case 1:
                    app->screen = APP_SCREEN_SEARCH;
                    break;
                case 2:
                    app->screen = APP_SCREEN_LIBRARY;
                    break;
                case 3:
                    app->screen = APP_SCREEN_SETTINGS;
                    break;
                default:
                    app->screen = APP_SCREEN_HOME;
                    break;
            }
            return 0;

        case UI_ACTION_BACK:
            if (app->screen == APP_SCREEN_ERROR) {
                app_controller_return_to_login(app);
            } else if (app->screen == APP_SCREEN_LOGIN) {
                app->login_focus = 0;
            } else {
                app->screen = APP_SCREEN_HOME;
            }
            return 0;

        case UI_ACTION_PREVIOUS:
            rc = spotify_playback_previous();
            break;

        case UI_ACTION_NEXT:
            rc = spotify_playback_next();
            break;

        case UI_ACTION_PLAY_PAUSE:
            if (app->now_playing.has_track &&
                app->now_playing.track.is_playing) {
                rc = spotify_playback_pause();
            } else {
                rc = spotify_playback_play();
            }
            break;


        case UI_ACTION_SEARCH_KEY_LEFT:
            app->search_focus_results = 0;
            search_key_move(app, -1);
            return 0;

        case UI_ACTION_SEARCH_KEY_RIGHT:
            app->search_focus_results = 0;
            search_key_move(app, 1);
            return 0;

        case UI_ACTION_SEARCH_KEY_UP:
            app->search_focus_results = 0;
            search_key_move(app, -SEARCH_KEY_COLS);
            return 0;

        case UI_ACTION_SEARCH_KEY_DOWN:
            app->search_focus_results = 0;
            search_key_move(app, SEARCH_KEY_COLS);
            return 0;

        case UI_ACTION_SEARCH_KEY_SELECT:
            app->search_focus_results = 0;
            search_append_selected_key(app);
            return 0;

        case UI_ACTION_SEARCH_BACKSPACE:
            search_backspace(app);
            app->search_focus_results = 0;
            return 0;

        case UI_ACTION_SEARCH_EXECUTE:
            return app_controller_search(app);

        case UI_ACTION_SEARCH_RESULT_UP:
            if (app->search_result_count > 0) {
                app->search_selected =
                    (app->search_selected +
                     app->search_result_count - 1) %
                    app->search_result_count;
            }
            return 0;

        case UI_ACTION_SEARCH_RESULT_DOWN:
            if (app->search_result_count > 0) {
                app->search_selected =
                    (app->search_selected + 1) %
                    app->search_result_count;
            }
            return 0;

        case UI_ACTION_SEARCH_PLAY_SELECTED:
            return app_controller_search_play_selected(app);

        case UI_ACTION_SEARCH_FOCUS_KEYBOARD:
            app->search_focus_results = 0;
            return 0;

        case UI_ACTION_SEARCH_FOCUS_RESULTS:
            if (app->search_result_count > 0)
                app->search_focus_results = 1;
            return 0;

        case UI_ACTION_LOGOUT:
            app_controller_logout(app);
            return 0;

        default:
            return 0;
    }

    if (rc == 0)
        spotify_state_worker_wake();

    return rc;
}
