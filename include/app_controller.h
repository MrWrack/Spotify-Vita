#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "now_playing.h"
#include "spotify_auth_pkce.h"
#include "spotify_callback_server.h"
#include "spotify_login.h"

typedef enum {
    APP_SCREEN_LOGIN = 0,
    APP_SCREEN_HOME,
    APP_SCREEN_SEARCH,
    APP_SCREEN_LIBRARY,
    APP_SCREEN_SETTINGS,
    APP_SCREEN_NOW_PLAYING,
    APP_SCREEN_ERROR
} AppScreen;

typedef struct {
    SpotifyAuthPkce auth;
    SpotifyLogin login;
    SpotifyCallbackServer callback;
    NowPlayingModel now_playing;

    AppScreen screen;
    int selected_nav;

    int initialized;
    int login_started;
    int session_saved;

    int last_error;
    int last_http_status;
} AppController;

int app_controller_init(
    AppController *app
);

void app_controller_update(
    AppController *app
);

int app_controller_begin_login(
    AppController *app
);

void app_controller_logout(
    AppController *app
);

void app_controller_shutdown(
    AppController *app
);

#endif
