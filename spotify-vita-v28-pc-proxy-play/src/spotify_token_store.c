#include "spotify_token_store.h"

#include "spotify_http.h"
#include "spotify_config.h"

#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define STORE_MAGIC   0x53505654u /* SPVT */
#define STORE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t payload_size;
    uint32_t checksum;
} StoreHeader;

typedef struct {
    char client_id[128];
    char redirect_uri[512];
    char access_token[1024];
    char refresh_token[2048];
    uint64_t expires_at_ms;
    int authenticated;
} StorePayload;

static uint32_t fnv1a32(const void *data, size_t size)
{
    const unsigned char *p = (const unsigned char *)data;
    uint32_t h = 2166136261u;

    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }

    return h;
}

int spotify_token_store_save(
    const char *path,
    const SpotifyAuthPkce *auth
)
{
    if (!path || !auth)
        return -1;

    StorePayload payload;
    memset(&payload, 0, sizeof(payload));

    strncpy(payload.client_id, auth->client_id, sizeof(payload.client_id) - 1);
    strncpy(payload.redirect_uri, auth->redirect_uri, sizeof(payload.redirect_uri) - 1);
    strncpy(payload.access_token, auth->access_token, sizeof(payload.access_token) - 1);
    strncpy(payload.refresh_token, auth->refresh_token, sizeof(payload.refresh_token) - 1);

    payload.expires_at_ms = auth->expires_at_ms;
    payload.authenticated = auth->authenticated;

    StoreHeader header;
    header.magic = STORE_MAGIC;
    header.version = STORE_VERSION;
    header.payload_size = (uint32_t)sizeof(payload);
    header.checksum = fnv1a32(&payload, sizeof(payload));

    /*
     * ux0:data exists, but the app-specific directory may not.
     * SCE_ERROR_ERRNO_EEXIST is harmless here, so the return value is not
     * treated as fatal; fopen below remains the authoritative check.
     */
    sceIoMkdir(SPOTIFY_DATA_DIR, 0777);

    FILE *f = fopen(path, "wb");
    if (!f)
        return -2;

    int ok =
        fwrite(&header, sizeof(header), 1, f) == 1 &&
        fwrite(&payload, sizeof(payload), 1, f) == 1;

    fclose(f);

    memset(&payload, 0, sizeof(payload));
    return ok ? 0 : -3;
}

int spotify_token_store_load(
    const char *path,
    SpotifyAuthPkce *auth
)
{
    if (!path || !auth)
        return -1;

    FILE *f = fopen(path, "rb");
    if (!f)
        return 1; /* no saved session */

    StoreHeader header;
    StorePayload payload;

    int ok =
        fread(&header, sizeof(header), 1, f) == 1 &&
        fread(&payload, sizeof(payload), 1, f) == 1;

    fclose(f);

    if (!ok)
        return -2;

    if (header.magic != STORE_MAGIC ||
        header.version != STORE_VERSION ||
        header.payload_size != sizeof(payload))
        return -3;

    if (header.checksum != fnv1a32(&payload, sizeof(payload)))
        return -4;

    /*
     * Keep the caller's configured app identity authoritative.
     */
    if (strcmp(auth->client_id, payload.client_id) != 0 ||
        strcmp(auth->redirect_uri, payload.redirect_uri) != 0) {
        memset(&payload, 0, sizeof(payload));
        return -5;
    }

    strncpy(auth->access_token, payload.access_token, sizeof(auth->access_token) - 1);
    strncpy(auth->refresh_token, payload.refresh_token, sizeof(auth->refresh_token) - 1);
    /*
     * expires_at_ms uses process monotonic time. It is only meaningful within
     * the process that obtained the token, so never restore it across launch.
     * Setting it to zero forces ensure_valid() to refresh before API use.
     */
    auth->expires_at_ms = 0;
    auth->authenticated =
        payload.authenticated &&
        payload.refresh_token[0];

    if (auth->authenticated)
        spotify_http_set_access_token(auth->access_token);

    memset(&payload, 0, sizeof(payload));
    return auth->authenticated ? 0 : 1;
}

int spotify_token_store_delete(
    const char *path
)
{
    if (!path)
        return -1;

    return remove(path) == 0 ? 0 : 1;
}
