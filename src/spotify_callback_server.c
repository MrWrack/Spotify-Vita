#include "spotify_callback_server.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/net/net.h>

#include <stdio.h>
#include <string.h>

#define CALLBACK_THREAD_STACK    0x10000
#define CALLBACK_THREAD_PRIORITY 0x10000100
#define CALLBACK_REQUEST_MAX     4096

static int send_all(int socket, const char *data, unsigned int size)
{
    unsigned int sent = 0;

    while (sent < size) {
        int rc = sceNetSend(
            socket,
            data + sent,
            size - sent,
            0
        );

        if (rc <= 0)
            return rc < 0 ? rc : -1;

        sent += (unsigned int)rc;
    }

    return 0;
}

static void send_browser_response(
    int socket,
    int success
)
{
    const char *body_ok =
        "<!doctype html>"
        "<html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Spotify Vita</title></head>"
        "<body style=\"background:#000;color:#1ed760;font-family:sans-serif;"
        "text-align:center;padding-top:48px\">"
        "<h2>Spotify Vita</h2>"
        "<p>Login complete.</p>"
        "<p>You can return to the Vita app.</p>"
        "</body></html>";

    const char *body_error =
        "<!doctype html>"
        "<html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Spotify Vita</title></head>"
        "<body style=\"background:#000;color:#ff6666;font-family:sans-serif;"
        "text-align:center;padding-top:48px\">"
        "<h2>Spotify Vita</h2>"
        "<p>Login failed.</p>"
        "<p>Return to the Vita app and try again.</p>"
        "</body></html>";

    const char *body = success ? body_ok : body_error;

    char header[512];
    int n = snprintf(
        header,
        sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "\r\n",
        (unsigned int)strlen(body)
    );

    if (n > 0 && (size_t)n < sizeof(header)) {
        send_all(socket, header, (unsigned int)n);
        send_all(socket, body, (unsigned int)strlen(body));
    }
}

static int request_target_to_callback_url(
    const char *request,
    int port,
    char *callback_url,
    size_t callback_url_size
)
{
    /*
     * Expected first line:
     * GET /callback?code=...&state=... HTTP/1.1
     */
    if (!request || !callback_url || callback_url_size == 0)
        return -1;

    const char *line_end = strstr(request, "\r\n");
    if (!line_end)
        return -2;

    const char *method_end = strchr(request, ' ');
    if (!method_end || method_end >= line_end)
        return -3;

    if ((size_t)(method_end - request) != 3 ||
        memcmp(request, "GET", 3) != 0)
        return -4;

    const char *target = method_end + 1;
    const char *target_end = strchr(target, ' ');

    if (!target_end || target_end > line_end)
        return -5;

    size_t target_len = (size_t)(target_end - target);

    if (target_len == 0 || target[0] != '/')
        return -6;

    int n = snprintf(
        callback_url,
        callback_url_size,
        "http://127.0.0.1:%d%.*s",
        port,
        (int)target_len,
        target
    );

    if (n < 0 || (size_t)n >= callback_url_size)
        return -7;

    return 0;
}

static int callback_thread(SceSize args, void *arg)
{
    (void)args;

    if (!arg || args != sizeof(SpotifyCallbackServer *))
        return -1;

    SpotifyCallbackServer *server =
        *(SpotifyCallbackServer **)arg;

    if (!server || !server->login)
        return -1;

    SceNetSockaddrIn address;
    memset(&address, 0, sizeof(address));

    address.sin_len = sizeof(address);
    address.sin_family = SCE_NET_AF_INET;
    address.sin_port = sceNetHtons((unsigned short)server->port);
    /*
     * Binding specifically to 127.0.0.1 is unreliable on some Vita
     * network-stack configurations. Listen on all local IPv4 interfaces.
     * A redirect to 127.0.0.1 still reaches this socket.
     */
    address.sin_addr.s_addr = sceNetHtonl(SCE_NET_INADDR_ANY);

    server->listen_socket = sceNetSocket(
        "SpotifyCallback",
        SCE_NET_AF_INET,
        SCE_NET_SOCK_STREAM,
        0
    );

    if (server->listen_socket < 0) {
        server->last_operation = SPOTIFY_CALLBACK_OP_SOCKET;
        server->last_error = server->listen_socket;
        server->state = SPOTIFY_CALLBACK_ERROR;
        server->running = 0;
        return server->listen_socket;
    }

    int reuse = 1;
    int sockopt_rc = sceNetSetsockopt(
        server->listen_socket,
        SCE_NET_SOL_SOCKET,
        SCE_NET_SO_REUSEADDR,
        &reuse,
        sizeof(reuse)
    );

    if (sockopt_rc < 0) {
        /*
         * SO_REUSEADDR is optional for this one-shot callback server.
         * Remember the operation for diagnostics, but continue to bind.
         */
        server->last_operation = SPOTIFY_CALLBACK_OP_SETSOCKOPT;
        server->last_error = sockopt_rc;
    }

    int rc = sceNetBind(
        server->listen_socket,
        (const SceNetSockaddr *)&address,
        sizeof(address)
    );

    if (rc < 0) {
        server->last_operation = SPOTIFY_CALLBACK_OP_BIND;
        server->last_error = rc;
        server->state = SPOTIFY_CALLBACK_ERROR;
        goto done;
    }

    rc = sceNetListen(server->listen_socket, 1);
    if (rc < 0) {
        server->last_operation = SPOTIFY_CALLBACK_OP_LISTEN;
        server->last_error = rc;
        server->state = SPOTIFY_CALLBACK_ERROR;
        goto done;
    }

    server->last_operation = SPOTIFY_CALLBACK_OP_NONE;
    server->last_error = 0;
    server->state = SPOTIFY_CALLBACK_LISTENING;

    while (server->running) {
        SceNetSockaddrIn peer;
        unsigned int peer_size = sizeof(peer);
        memset(&peer, 0, sizeof(peer));

        server->client_socket = sceNetAccept(
            server->listen_socket,
            (SceNetSockaddr *)&peer,
            &peer_size
        );

        if (server->client_socket < 0) {
            if (!server->running)
                break;

            server->last_operation = SPOTIFY_CALLBACK_OP_ACCEPT;
            server->last_error = server->client_socket;
            server->state = SPOTIFY_CALLBACK_ERROR;
            break;
        }

        char request[CALLBACK_REQUEST_MAX + 1];
        int received = sceNetRecv(
            server->client_socket,
            request,
            CALLBACK_REQUEST_MAX,
            0
        );

        if (received > 0) {
            request[received] = '\0';

            char callback_url[4096];
            rc = request_target_to_callback_url(
                request,
                server->port,
                callback_url,
                sizeof(callback_url)
            );

            if (rc == 0) {
                rc = spotify_login_accept_callback_url(
                    server->login,
                    callback_url
                );
            }

            send_browser_response(
                server->client_socket,
                rc == 0
            );

            sceNetSocketClose(server->client_socket);
            server->client_socket = -1;

            if (rc == 0) {
                server->state = SPOTIFY_CALLBACK_RECEIVED;
                server->running = 0;
                break;
            }

            /*
             * Keep listening after malformed/unrelated requests. The user's
             * actual Spotify redirect may still arrive.
             */
            server->last_operation = SPOTIFY_CALLBACK_OP_PARSE;
            server->last_error = rc;
        } else {
            if (received < 0) {
                server->last_operation = SPOTIFY_CALLBACK_OP_RECV;
                server->last_error = received;
            }

            sceNetSocketClose(server->client_socket);
            server->client_socket = -1;
        }
    }

done:
    if (server->client_socket >= 0) {
        sceNetSocketClose(server->client_socket);
        server->client_socket = -1;
    }

    if (server->listen_socket >= 0) {
        sceNetSocketClose(server->listen_socket);
        server->listen_socket = -1;
    }

    return 0;
}

int spotify_callback_server_init(
    SpotifyCallbackServer *server,
    SpotifyLogin *login,
    int port
)
{
    if (!server || !login || port <= 0 || port > 65535)
        return -1;

    memset(server, 0, sizeof(*server));

    server->login = login;
    server->port = port;
    server->listen_socket = -1;
    server->client_socket = -1;
    server->thread_id = -1;
    server->state = SPOTIFY_CALLBACK_STOPPED;
    server->last_operation = SPOTIFY_CALLBACK_OP_NONE;

    return 0;
}

int spotify_callback_server_start(
    SpotifyCallbackServer *server
)
{
    if (!server || !server->login)
        return -1;

    if (server->thread_id >= 0)
        return 0;

    server->last_error = 0;
    server->last_operation = SPOTIFY_CALLBACK_OP_NONE;
    server->running = 1;
    server->state = SPOTIFY_CALLBACK_STOPPED;

    server->thread_id = sceKernelCreateThread(
        "SpotifyCallbackThread",
        callback_thread,
        CALLBACK_THREAD_PRIORITY,
        CALLBACK_THREAD_STACK,
        0,
        0,
        NULL
    );

    if (server->thread_id < 0) {
        server->running = 0;
        server->last_operation = SPOTIFY_CALLBACK_OP_THREAD_CREATE;
        server->last_error = server->thread_id;
        server->state = SPOTIFY_CALLBACK_ERROR;
        return server->thread_id;
    }

    SpotifyCallbackServer *thread_arg = server;

    int rc = sceKernelStartThread(
        server->thread_id,
        sizeof(thread_arg),
        &thread_arg
    );

    if (rc < 0) {
        sceKernelDeleteThread(server->thread_id);
        server->thread_id = -1;
        server->running = 0;
        server->last_operation = SPOTIFY_CALLBACK_OP_THREAD_START;
        server->last_error = rc;
        server->state = SPOTIFY_CALLBACK_ERROR;
        return rc;
    }

    return 0;
}

void spotify_callback_server_stop(
    SpotifyCallbackServer *server
)
{
    if (!server)
        return;

    server->running = 0;

    /*
     * Closing sockets unblocks accept()/recv() so shutdown does not hang.
     */
    if (server->client_socket >= 0) {
        sceNetSocketClose(server->client_socket);
        server->client_socket = -1;
    }

    if (server->listen_socket >= 0) {
        sceNetSocketClose(server->listen_socket);
        server->listen_socket = -1;
    }

    if (server->thread_id >= 0) {
        sceKernelWaitThreadEnd(
            server->thread_id,
            NULL,
            NULL
        );

        sceKernelDeleteThread(server->thread_id);
        server->thread_id = -1;
    }

    /*
     * An explicit stop is also a reset. Keeping SPOTIFY_CALLBACK_ERROR here
     * caused app_controller_update() to immediately reopen the error screen
     * after the user pressed Back.
     */
    if (server->state != SPOTIFY_CALLBACK_RECEIVED)
        server->state = SPOTIFY_CALLBACK_STOPPED;

    if (server->state == SPOTIFY_CALLBACK_STOPPED) {
        server->last_operation = SPOTIFY_CALLBACK_OP_NONE;
        server->last_error = 0;
    }
}

void spotify_callback_server_shutdown(
    SpotifyCallbackServer *server
)
{
    spotify_callback_server_stop(server);

    if (!server)
        return;

    server->login = NULL;
}

SpotifyCallbackServerState spotify_callback_server_state(
    const SpotifyCallbackServer *server
)
{
    return server
        ? server->state
        : SPOTIFY_CALLBACK_ERROR;
}
