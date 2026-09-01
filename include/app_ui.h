#ifndef APP_UI_H
#define APP_UI_H

#include "app_controller.h"
#include "app_input.h"

typedef enum {
    UI_ACTION_NONE = 0,
    UI_ACTION_LOGIN,
    UI_ACTION_FOCUS_LOGIN,
    UI_ACTION_PLAY_PAUSE,
    UI_ACTION_PREVIOUS,
    UI_ACTION_NEXT,
    UI_ACTION_OPEN_NOW_PLAYING,
    UI_ACTION_NAV_UP,
    UI_ACTION_NAV_DOWN,
    UI_ACTION_SELECT_NAV,
    UI_ACTION_BACK,
    UI_ACTION_LOGOUT
} UiAction;

UiAction app_ui_action_from_input(
    const AppController *app,
    const AppInput *input
);

int app_ui_execute_action(
    AppController *app,
    UiAction action
);

#endif
