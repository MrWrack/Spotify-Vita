#ifndef APP_CONTROLLER_H
#define APP_CONTROLLER_H

#include "now_playing.h"
#include "spotify_auth_pkce.h"
#include "spotify_login.h"
#include "spotify_json.h"

#define APP_SEARCH_MAX_RESULTS 8

typedef enum {
    APP_SCREEN_LOGIN = 0,
    APP_SCREEN_HOME,
    APP_SCREEN_SEARCH,
    APP_SCREEN_LIBRARY,
    APP_SCREEN_SETTINGS,
    APP_SCREEN_NOW_PLAYING,
    APP_SCREEN_ERROR
} AppScreen;


typedef enum {
    APP_ERROR_STAGE_NONE = 0,
    APP_ERROR_STAGE_LOGIN,
    APP_ERROR_STAGE_APP_URI_CALLBACK,
    APP_ERROR_STAGE_BROWSER,
    APP_ERROR_STAGE_NETWORK,
    APP_ERROR_STAGE_SPOTIFY_HTTP
} AppErrorStage;

typedef struct {
    SpotifyAuthPkce auth;
    SpotifyLogin login;
    NowPlayingModel now_playing;

    AppScreen screen;
    int selected_nav;
    int login_focus;

    int initialized;
    int login_started;
    int session_saved;

    int last_error;
    AppErrorStage last_error_stage;
    int last_http_status;

    int network_connected;
    int network_state;

    char search_query[64];
    SpotifyTrack search_results[APP_SEARCH_MAX_RESULTS];
    int search_result_count;
    int search_selected;
    int search_keyboard_index;
    int search_focus_results;
    int search_last_error;
    int search_last_http_status;
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

void app_controller_return_to_login(
    AppController *app
);

void app_controller_logout(
    AppController *app
);


int app_controller_search(
    AppController *app
);

int app_controller_search_play_selected(
    AppController *app
);

void app_controller_shutdown(
    AppController *app
);

#endif
