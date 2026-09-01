#include "spotify_http.h"

#include <psp2/net/http.h>
#include <psp2/sysmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_POOL_SIZE       (512 * 1024)
#define RESPONSE_CHUNK       (16 * 1024)
#define RESPONSE_MAX         (512 * 1024)

static int g_template = -1;
static char g_access_token[1024];

static int g_http_module_loaded_here = 0;
static int g_ssl_module_loaded_here = 0;
static int g_https_module_loaded_here = 0;

static int load_module_if_needed(
    SceSysmoduleModuleId module,
    int *loaded_here
)
{
    if (sceSysmoduleIsLoaded(module) == SCE_SYSMODULE_LOADED)
        return 0;

    int rc = sceSysmoduleLoadModule(module);
    if (rc < 0)
        return rc;

    *loaded_here = 1;
    return 0;
}

static void unload_module_if_owned(
    SceSysmoduleModuleId module,
    int *loaded_here
)
{
    if (*loaded_here) {
        sceSysmoduleUnloadModule(module);
        *loaded_here = 0;
    }
}

static int method_to_sce(SpotifyHttpMethod method)
{
    switch (method) {
        case SPOTIFY_HTTP_GET:  return SCE_HTTP_METHOD_GET;
        case SPOTIFY_HTTP_POST: return SCE_HTTP_METHOD_POST;
        case SPOTIFY_HTTP_PUT:  return SCE_HTTP_METHOD_PUT;
        default: return -1;
    }
}

static int parse_retry_after(int request)
{
    char *headers = NULL;
    unsigned int header_size = 0;
    const char *value = NULL;
    unsigned int value_len = 0;

    if (sceHttpGetAllResponseHeaders(request, &headers, &header_size) < 0)
        return 0;

    if (sceHttpParseResponseHeader(
            headers, header_size, "Retry-After", &value, &value_len) < 0)
        return 0;

    char temp[32];
    size_t n = value_len;
    if (n >= sizeof(temp))
        n = sizeof(temp) - 1;

    memcpy(temp, value, n);
    temp[n] = '\0';
    return atoi(temp);
}

static int read_body(int request, SpotifyHttpResponse *out)
{
    size_t capacity = RESPONSE_CHUNK;
    size_t used = 0;
    char *buffer = (char *)malloc(capacity + 1);
    if (!buffer)
        return -1;

    for (;;) {
        if (used == capacity) {
            if (capacity >= RESPONSE_MAX) {
                free(buffer);
                return -2;
            }

            size_t new_capacity = capacity * 2;
            if (new_capacity > RESPONSE_MAX)
                new_capacity = RESPONSE_MAX;

            char *grown = (char *)realloc(buffer, new_capacity + 1);
            if (!grown) {
                free(buffer);
                return -3;
            }

            buffer = grown;
            capacity = new_capacity;
        }

        int n = sceHttpReadData(
            request,
            buffer + used,
            (unsigned int)(capacity - used)
        );

        if (n < 0) {
            free(buffer);
            return n;
        }

        if (n == 0)
            break;

        used += (size_t)n;
    }

    buffer[used] = '\0';
    out->body = buffer;
    out->body_size = used;
    return 0;
}

int spotify_http_init(void)
{
    memset(g_access_token, 0, sizeof(g_access_token));

    int rc = load_module_if_needed(
        SCE_SYSMODULE_HTTP,
        &g_http_module_loaded_here
    );
    if (rc < 0)
        return rc;

    /*
     * Spotify endpoints are HTTPS. VitaSDK exposes SSL/HTTPS as separate
     * sysmodules, so load both before creating HTTPS requests.
     */
    rc = load_module_if_needed(
        SCE_SYSMODULE_SSL,
        &g_ssl_module_loaded_here
    );
    if (rc < 0)
        goto fail_modules;

    rc = load_module_if_needed(
        SCE_SYSMODULE_HTTPS,
        &g_https_module_loaded_here
    );
    if (rc < 0)
        goto fail_modules;

    rc = sceHttpInit(HTTP_POOL_SIZE);
    if (rc < 0)
        goto fail_modules;

    g_template = sceHttpCreateTemplate(
        "SpotifyVita/0.9",
        SCE_HTTP_VERSION_1_1,
        SCE_HTTP_PROXY_AUTO
    );

    if (g_template < 0) {
        rc = g_template;
        sceHttpTerm();
        g_template = -1;
        goto fail_modules;
    }

    sceHttpSetResolveTimeOut(g_template, 10 * 1000 * 1000U);
    sceHttpSetConnectTimeOut(g_template, 15 * 1000 * 1000U);
    sceHttpSetSendTimeOut(g_template, 30 * 1000 * 1000U);
    sceHttpSetRecvTimeOut(g_template, 30 * 1000 * 1000U);
    sceHttpSetAutoRedirect(g_template, SCE_HTTP_ENABLE);

    return 0;

fail_modules:
    unload_module_if_owned(
        SCE_SYSMODULE_HTTPS,
        &g_https_module_loaded_here
    );
    unload_module_if_owned(
        SCE_SYSMODULE_SSL,
        &g_ssl_module_loaded_here
    );
    unload_module_if_owned(
        SCE_SYSMODULE_HTTP,
        &g_http_module_loaded_here
    );

    return rc;
}

void spotify_http_shutdown(void)
{
    if (g_template >= 0) {
        sceHttpDeleteTemplate(g_template);
        g_template = -1;
        sceHttpTerm();
    }

    unload_module_if_owned(
        SCE_SYSMODULE_HTTPS,
        &g_https_module_loaded_here
    );
    unload_module_if_owned(
        SCE_SYSMODULE_SSL,
        &g_ssl_module_loaded_here
    );
    unload_module_if_owned(
        SCE_SYSMODULE_HTTP,
        &g_http_module_loaded_here
    );

    memset(g_access_token, 0, sizeof(g_access_token));
}

void spotify_http_set_access_token(
    const char *access_token
)
{
    if (!access_token) {
        g_access_token[0] = '\0';
        return;
    }

    strncpy(g_access_token, access_token, sizeof(g_access_token) - 1);
    g_access_token[sizeof(g_access_token) - 1] = '\0';
}

int spotify_http_request_absolute(
    SpotifyHttpMethod method,
    const char *url,
    const char *body,
    const char *content_type,
    const char *authorization,
    SpotifyHttpResponse *out
)
{
    if (!url || !out || g_template < 0)
        return -1;

    memset(out, 0, sizeof(*out));

    int sce_method = method_to_sce(method);
    if (sce_method < 0)
        return -2;

    int connection = sceHttpCreateConnectionWithURL(
        g_template,
        url,
        SCE_HTTP_ENABLE
    );

    if (connection < 0)
        return connection;

    unsigned long long body_len = body ? (unsigned long long)strlen(body) : 0;

    int request = sceHttpCreateRequestWithURL(
        connection,
        sce_method,
        url,
        body_len
    );

    if (request < 0) {
        sceHttpDeleteConnection(connection);
        return request;
    }

    if (authorization && authorization[0]) {
        sceHttpAddRequestHeader(
            request,
            "Authorization",
            authorization,
            SCE_HTTP_HEADER_OVERWRITE
        );
    }

    sceHttpAddRequestHeader(
        request,
        "Accept",
        "application/json",
        SCE_HTTP_HEADER_OVERWRITE
    );

    if (content_type && content_type[0]) {
        sceHttpAddRequestHeader(
            request,
            "Content-Type",
            content_type,
            SCE_HTTP_HEADER_OVERWRITE
        );
    }

    int rc = sceHttpSendRequest(
        request,
        body,
        body ? strlen(body) : 0
    );

    if (rc < 0)
        goto cleanup;

    rc = sceHttpGetStatusCode(request, &out->status_code);
    if (rc < 0)
        goto cleanup;

    if (out->status_code == 429)
        out->retry_after = parse_retry_after(request);

    rc = read_body(request, out);

cleanup:
    sceHttpDeleteRequest(request);
    sceHttpDeleteConnection(connection);

    if (rc < 0)
        spotify_http_response_free(out);

    return rc;
}

int spotify_http_request_api(
    SpotifyHttpMethod method,
    const char *path,
    const char *body,
    const char *content_type,
    SpotifyHttpResponse *out
)
{
    if (!path || path[0] != '/')
        return -1;

    char url[1536];
    char auth[1200];

    snprintf(url, sizeof(url), "https://api.spotify.com%s", path);

    if (!g_access_token[0])
        return -2;

    snprintf(auth, sizeof(auth), "Bearer %s", g_access_token);

    return spotify_http_request_absolute(
        method,
        url,
        body,
        content_type,
        auth,
        out
    );
}

void spotify_http_response_free(
    SpotifyHttpResponse *response
)
{
    if (!response)
        return;

    free(response->body);
    memset(response, 0, sizeof(*response));
}
