#include "spotify_token_import.h"
#include "spotify_config.h"

#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMPORT_PATH SPOTIFY_DATA_DIR "/import_tokens.txt"

static void trim_eol(char *s)
{
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n && (s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[--n] = '\0';
    }
}

int spotify_token_import_try(SpotifyAuthPkce *auth)
{
    FILE *f;
    char line[2300];
    char access_token[2048] = {0};
    char refresh_token[2048] = {0};
    int expires_in = 3600;

    if (!auth) return -1;

    f = fopen(IMPORT_PATH, "rb");
    if (!f) return 0;

    while (fgets(line, sizeof(line), f)) {
        trim_eol(line);
        if (strncmp(line, "access_token=", 13) == 0) {
            snprintf(access_token, sizeof(access_token), "%s", line + 13);
        } else if (strncmp(line, "refresh_token=", 14) == 0) {
            snprintf(refresh_token, sizeof(refresh_token), "%s", line + 14);
        } else if (strncmp(line, "expires_in=", 11) == 0) {
            expires_in = atoi(line + 11);
        }
    }
    fclose(f);

    if (!access_token[0]) return -2;

    snprintf(auth->access_token, sizeof(auth->access_token), "%s", access_token);
    if (refresh_token[0]) {
        snprintf(auth->refresh_token, sizeof(auth->refresh_token), "%s", refresh_token);
    }
    auth->expires_at_ms = (sceKernelGetProcessTimeWide() / 1000u) + ((uint64_t)expires_in * 1000u);
    auth->authenticated = 1;

    sceIoRemove(IMPORT_PATH);
    return 1;
}
