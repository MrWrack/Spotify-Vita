#include "app_ui.h"

#include "spotify_playback.h"
#include "spotify_state_worker.h"

#include <psp2/ctrl.h>

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
        if (input->buttons_down & SCE_CTRL_UP)
            return UI_ACTION_NAV_UP;

        if (input->buttons_down & SCE_CTRL_DOWN)
            return UI_ACTION_NAV_DOWN;

        if (input->buttons_down & SCE_CTRL_CIRCLE)
            return UI_ACTION_BACK;

        if (input->buttons_down & SCE_CTRL_CROSS) {
            if (app->login_focus == 0)
                return UI_ACTION_LOGIN;
            return UI_ACTION_BACK;
        }

        if (input->touch_active) {
            if (input->touch_x >= 290 &&
                input->touch_x <= 670 &&
                input->touch_y >= 250 &&
                input->touch_y <= 365)
                return UI_ACTION_LOGIN;

            if (input->touch_x >= 390 &&
                input->touch_x <= 570 &&
                input->touch_y >= 385 &&
                input->touch_y <= 440)
                return UI_ACTION_BACK;
        }

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

    if (app->screen == APP_SCREEN_SEARCH ||
        app->screen == APP_SCREEN_LIBRARY ||
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
                app->screen = APP_SCREEN_LOGIN;
                app->login_focus = 0;
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
