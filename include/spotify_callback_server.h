#ifndef SPOTIFY_CALLBACK_SERVER_H
#define SPOTIFY_CALLBACK_SERVER_H

#include "spotify_login.h"

typedef enum {
    SPOTIFY_CALLBACK_STOPPED = 0,
    SPOTIFY_CALLBACK_LISTENING,
    SPOTIFY_CALLBACK_RECEIVED,
    SPOTIFY_CALLBACK_ERROR
} SpotifyCallbackServerState;

typedef struct {
    SpotifyLogin *login;

    int port;
    int listen_socket;
    int client_socket;

    int thread_id;
    volatile int running;

    SpotifyCallbackServerState state;
    int last_error;
} SpotifyCallbackServer;

int spotify_callback_server_init(
    SpotifyCallbackServer *server,
    SpotifyLogin *login,
    int port
);

int spotify_callback_server_start(
    SpotifyCallbackServer *server
);

void spotify_callback_server_stop(
    SpotifyCallbackServer *server
);

void spotify_callback_server_shutdown(
    SpotifyCallbackServer *server
);

SpotifyCallbackServerState spotify_callback_server_state(
    const SpotifyCallbackServer *server
);

#endif
