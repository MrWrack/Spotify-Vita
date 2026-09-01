#include "spotify_auth_pkce.h"

#include "spotify_http.h"
#include "spotify_json.h"

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/rng.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TOKEN_URL "https://accounts.spotify.com/api/token"
#define AUTHORIZE_BASE "https://accounts.spotify.com/authorize"

/* ---------- SHA-256 ---------- */

typedef struct {
    uint32_t h[8];
    uint64_t bits;
    uint8_t block[64];
    size_t used;
} Sha256Ctx;

static uint32_t rotr32(uint32_t v, unsigned n)
{
    return (v >> n) | (v << (32 - n));
}

static const uint32_t k256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static void sha256_transform(Sha256Ctx *c, const uint8_t block[64])
{
    uint32_t w[64];

    for (int i = 0; i < 16; ++i) {
        w[i] =
            ((uint32_t)block[i*4+0] << 24) |
            ((uint32_t)block[i*4+1] << 16) |
            ((uint32_t)block[i*4+2] << 8)  |
            ((uint32_t)block[i*4+3]);
    }

    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i-2], 17) ^ rotr32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }

    uint32_t a=c->h[0], b=c->h[1], cc=c->h[2], d=c->h[3];
    uint32_t e=c->h[4], f=c->h[5], g=c->h[6], h=c->h[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + k256[i] + w[i];
        uint32_t S0 = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;

        h=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }

    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=h;
}

static void sha256_init(Sha256Ctx *c)
{
    c->h[0]=0x6a09e667; c->h[1]=0xbb67ae85;
    c->h[2]=0x3c6ef372; c->h[3]=0xa54ff53a;
    c->h[4]=0x510e527f; c->h[5]=0x9b05688c;
    c->h[6]=0x1f83d9ab; c->h[7]=0x5be0cd19;
    c->bits=0; c->used=0;
}

static void sha256_update(Sha256Ctx *c, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    c->bits += (uint64_t)len * 8u;

    while (len) {
        size_t room = 64 - c->used;
        size_t n = len < room ? len : room;
        memcpy(c->block + c->used, p, n);
        c->used += n;
        p += n;
        len -= n;

        if (c->used == 64) {
            sha256_transform(c, c->block);
            c->used = 0;
        }
    }
}

static void sha256_final(Sha256Ctx *c, uint8_t out[32])
{
    c->block[c->used++] = 0x80;

    if (c->used > 56) {
        while (c->used < 64) c->block[c->used++] = 0;
        sha256_transform(c, c->block);
        c->used = 0;
    }

    while (c->used < 56) c->block[c->used++] = 0;

    for (int i = 7; i >= 0; --i)
        c->block[c->used++] = (uint8_t)(c->bits >> (i * 8));

    sha256_transform(c, c->block);

    for (int i = 0; i < 8; ++i) {
        out[i*4+0] = (uint8_t)(c->h[i] >> 24);
        out[i*4+1] = (uint8_t)(c->h[i] >> 16);
        out[i*4+2] = (uint8_t)(c->h[i] >> 8);
        out[i*4+3] = (uint8_t)c->h[i];
    }
}

/* ---------- Encoding ---------- */

static const char b64url_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int base64url_no_padding(
    const uint8_t *src,
    size_t src_len,
    char *dst,
    size_t dst_size
)
{
    size_t required = ((src_len + 2) / 3) * 4 + 1;
    if (!src || !dst || dst_size < required)
        return -1;

    size_t r = 0, w = 0;

    while (r + 3 <= src_len) {
        uint32_t v =
            ((uint32_t)src[r] << 16) |
            ((uint32_t)src[r+1] << 8) |
            src[r+2];

        dst[w++] = b64url_table[(v >> 18) & 63];
        dst[w++] = b64url_table[(v >> 12) & 63];
        dst[w++] = b64url_table[(v >> 6) & 63];
        dst[w++] = b64url_table[v & 63];
        r += 3;
    }

    if (r < src_len) {
        uint32_t v = (uint32_t)src[r] << 16;
        dst[w++] = b64url_table[(v >> 18) & 63];

        if (r + 1 < src_len) {
            v |= (uint32_t)src[r+1] << 8;
            dst[w++] = b64url_table[(v >> 12) & 63];
            dst[w++] = b64url_table[(v >> 6) & 63];
        } else {
            dst[w++] = b64url_table[(v >> 12) & 63];
        }
    }

    dst[w] = '\0';
    return 0;
}

static int url_encode(const char *src, char *dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t w = 0;

    if (!src || !dst)
        return -1;

    for (; *src; ++src) {
        unsigned char c = (unsigned char)*src;
        int safe =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~';

        if (safe) {
            if (w + 1 >= dst_size) return -1;
            dst[w++] = (char)c;
        } else {
            if (w + 3 >= dst_size) return -1;
            dst[w++] = '%';
            dst[w++] = hex[(c >> 4) & 15];
            dst[w++] = hex[c & 15];
        }
    }

    if (w >= dst_size)
        return -1;

    dst[w] = '\0';
    return 0;
}

static uint64_t now_ms(void)
{
    /*
     * Process time is monotonic and sufficient for access-token expiry within
     * one application run. Persistent token storage should save wall-clock
     * expiry separately when secure_store is added.
     */
    return sceKernelGetProcessTimeWide() / 1000u;
}

static int make_random_urlsafe(char *out, size_t chars)
{
    uint8_t raw[64];

    if (!out || chars < 43 || chars > 86)
        return -1;

    size_t raw_len = (chars * 3 + 3) / 4;
    if (raw_len > sizeof(raw))
        raw_len = sizeof(raw);

    if (sceKernelGetRandomNumber(raw, (SceSize)raw_len) < 0)
        return -2;

    char temp[128];
    if (base64url_no_padding(raw, raw_len, temp, sizeof(temp)) < 0)
        return -3;

    if (strlen(temp) < chars)
        return -4;

    memcpy(out, temp, chars);
    out[chars] = '\0';
    return 0;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');

    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');

    return -1;
}

static int url_decode_component(
    const char *src,
    size_t src_len,
    char *out,
    size_t out_size
)
{
    if (!src || !out || out_size == 0)
        return -1;

    size_t w = 0;

    for (size_t i = 0; i < src_len; ++i) {
        unsigned char value;

        if (src[i] == '%') {
            if (i + 2 >= src_len)
                return -2;

            int hi = hex_value(src[i + 1]);
            int lo = hex_value(src[i + 2]);

            if (hi < 0 || lo < 0)
                return -3;

            value = (unsigned char)((hi << 4) | lo);
            i += 2;
        } else if (src[i] == '+') {
            value = ' ';
        } else {
            value = (unsigned char)src[i];
        }

        if (w + 1 >= out_size)
            return -4;

        out[w++] = (char)value;
    }

    out[w] = '\0';
    return 0;
}

static int parse_query_param(
    const char *url,
    const char *name,
    char *out,
    size_t out_size
)
{
    const char *q = strchr(url, '?');
    if (!q)
        return 1;

    ++q;

    size_t name_len = strlen(name);

    while (*q) {
        const char *segment_end = strchr(q, '&');
        if (!segment_end)
            segment_end = q + strlen(q);

        const char *eq = memchr(
            q,
            '=',
            (size_t)(segment_end - q)
        );

        if (eq &&
            (size_t)(eq - q) == name_len &&
            memcmp(q, name, name_len) == 0) {

            return url_decode_component(
                eq + 1,
                (size_t)(segment_end - (eq + 1)),
                out,
                out_size
            );
        }

        q = *segment_end
            ? segment_end + 1
            : segment_end;
    }

    return 1;
}

int spotify_auth_pkce_init(
    SpotifyAuthPkce *auth,
    const char *client_id,
    const char *redirect_uri
)
{
    if (!auth || !client_id || !redirect_uri)
        return -1;

    memset(auth, 0, sizeof(*auth));
    strncpy(auth->client_id, client_id, sizeof(auth->client_id) - 1);
    strncpy(auth->redirect_uri, redirect_uri, sizeof(auth->redirect_uri) - 1);
    return 0;
}

int spotify_auth_pkce_begin(
    SpotifyAuthPkce *auth,
    const char *scopes,
    char *authorization_url,
    size_t authorization_url_size
)
{
    if (!auth || !scopes || !authorization_url)
        return -1;

    if (make_random_urlsafe(auth->code_verifier, 64) < 0)
        return -2;

    if (make_random_urlsafe(auth->state, 48) < 0)
        return -3;

    uint8_t digest[32];
    Sha256Ctx sha;

    sha256_init(&sha);
    sha256_update(&sha, auth->code_verifier, strlen(auth->code_verifier));
    sha256_final(&sha, digest);

    if (base64url_no_padding(
            digest, sizeof(digest),
            auth->code_challenge, sizeof(auth->code_challenge)) < 0)
        return -4;

    char redirect[1024];
    char scope_enc[1024];

    if (url_encode(auth->redirect_uri, redirect, sizeof(redirect)) < 0 ||
        url_encode(scopes, scope_enc, sizeof(scope_enc)) < 0)
        return -5;

    int n = snprintf(
        authorization_url,
        authorization_url_size,
        AUTHORIZE_BASE
        "?client_id=%s"
        "&response_type=code"
        "&redirect_uri=%s"
        "&code_challenge_method=S256"
        "&code_challenge=%s"
        "&state=%s"
        "&scope=%s",
        auth->client_id,
        redirect,
        auth->code_challenge,
        auth->state,
        scope_enc
    );

    return (n < 0 || (size_t)n >= authorization_url_size) ? -6 : 0;
}

int spotify_auth_pkce_parse_callback(
    SpotifyAuthPkce *auth,
    const char *callback_url,
    char *code,
    size_t code_size
)
{
    char callback_state[129];
    char oauth_error[256];

    if (!auth || !callback_url || !code || code_size == 0)
        return -1;

    /*
     * Spotify returns error=access_denied (and state) when the user declines.
     * Surface that as a distinct callback failure before looking for code.
     */
    if (parse_query_param(
            callback_url,
            "error",
            oauth_error,
            sizeof(oauth_error)) == 0) {
        return -5;
    }

    if (parse_query_param(
            callback_url,
            "state",
            callback_state,
            sizeof(callback_state)) != 0)
        return -2;

    if (strcmp(callback_state, auth->state) != 0)
        return -3;

    if (parse_query_param(
            callback_url,
            "code",
            code,
            code_size) != 0)
        return -4;

    return 0;
}

static int apply_token_response(
    SpotifyAuthPkce *auth,
    const SpotifyTokenResponse *token
)
{
    strncpy(auth->access_token,
            token->access_token,
            sizeof(auth->access_token) - 1);

    if (token->has_refresh_token) {
        strncpy(auth->refresh_token,
                token->refresh_token,
                sizeof(auth->refresh_token) - 1);
    }

    auth->expires_at_ms =
        now_ms() + (uint64_t)token->expires_in * 1000u;

    auth->authenticated = 1;
    spotify_http_set_access_token(auth->access_token);
    return 0;
}

int spotify_auth_pkce_exchange_code(
    SpotifyAuthPkce *auth,
    const char *authorization_code
)
{
    if (!auth || !authorization_code || !auth->code_verifier[0])
        return -1;

    char code[2048], redirect[1024], verifier[512], client[512];
    if (url_encode(authorization_code, code, sizeof(code)) < 0 ||
        url_encode(auth->redirect_uri, redirect, sizeof(redirect)) < 0 ||
        url_encode(auth->code_verifier, verifier, sizeof(verifier)) < 0 ||
        url_encode(auth->client_id, client, sizeof(client)) < 0)
        return -2;

    char body[4096];
    int n = snprintf(
        body, sizeof(body),
        "grant_type=authorization_code"
        "&code=%s"
        "&redirect_uri=%s"
        "&client_id=%s"
        "&code_verifier=%s",
        code, redirect, client, verifier
    );

    if (n < 0 || (size_t)n >= sizeof(body))
        return -3;

    SpotifyHttpResponse response;
    int rc = spotify_http_request_absolute(
        SPOTIFY_HTTP_POST,
        TOKEN_URL,
        body,
        "application/x-www-form-urlencoded",
        NULL,
        &response
    );

    if (rc < 0)
        return rc;

    if (response.status_code != 200) {
        spotify_http_response_free(&response);
        return -response.status_code;
    }

    SpotifyTokenResponse token;
    rc = spotify_json_parse_token(response.body, response.body_size, &token);
    spotify_http_response_free(&response);

    if (rc < 0)
        return rc;

    return apply_token_response(auth, &token);
}

int spotify_auth_pkce_refresh(
    SpotifyAuthPkce *auth
)
{
    if (!auth || !auth->refresh_token[0])
        return -1;

    char refresh[4096], client[512];

    if (url_encode(auth->refresh_token, refresh, sizeof(refresh)) < 0 ||
        url_encode(auth->client_id, client, sizeof(client)) < 0)
        return -2;

    char body[4608];
    int n = snprintf(
        body, sizeof(body),
        "grant_type=refresh_token"
        "&refresh_token=%s"
        "&client_id=%s",
        refresh, client
    );

    if (n < 0 || (size_t)n >= sizeof(body))
        return -3;

    SpotifyHttpResponse response;
    int rc = spotify_http_request_absolute(
        SPOTIFY_HTTP_POST,
        TOKEN_URL,
        body,
        "application/x-www-form-urlencoded",
        NULL,
        &response
    );

    if (rc < 0)
        return rc;

    if (response.status_code != 200) {
        /*
         * For invalid_grant or an expired refresh token, caller should return
         * to the authorization flow rather than retry forever.
         */
        spotify_http_response_free(&response);
        auth->authenticated = 0;
        return -response.status_code;
    }

    SpotifyTokenResponse token;
    rc = spotify_json_parse_token(response.body, response.body_size, &token);
    spotify_http_response_free(&response);

    if (rc < 0)
        return rc;

    return apply_token_response(auth, &token);
}

int spotify_auth_pkce_ensure_valid(
    SpotifyAuthPkce *auth
)
{
    if (!auth || !auth->authenticated)
        return -1;

    if (now_ms() + 60000u < auth->expires_at_ms) {
        spotify_http_set_access_token(auth->access_token);
        return 0;
    }

    return spotify_auth_pkce_refresh(auth);
}

void spotify_auth_pkce_clear(
    SpotifyAuthPkce *auth
)
{
    if (!auth)
        return;

    memset(auth->access_token, 0, sizeof(auth->access_token));
    memset(auth->refresh_token, 0, sizeof(auth->refresh_token));
    memset(auth->code_verifier, 0, sizeof(auth->code_verifier));
    memset(auth->code_challenge, 0, sizeof(auth->code_challenge));
    memset(auth->state, 0, sizeof(auth->state));

    auth->expires_at_ms = 0;
    auth->authenticated = 0;
    spotify_http_set_access_token(NULL);
}

const char *spotify_auth_pkce_access_token(
    const SpotifyAuthPkce *auth
)
{
    return (auth && auth->authenticated) ? auth->access_token : NULL;
}
