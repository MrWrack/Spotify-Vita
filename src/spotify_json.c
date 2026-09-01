#include "spotify_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ws(const char *p, const char *end)
{
    while (p < end && isspace((unsigned char)*p))
        ++p;
    return p;
}

static int copy_unescaped_string(const char *start, const char *end, char *out, size_t out_size)
{
    size_t w = 0;
    const char *p = start;

    if (!out || out_size == 0)
        return -1;

    while (p < end && w + 1 < out_size) {
        unsigned char c = (unsigned char)*p++;

        if (c != '\\') {
            out[w++] = (char)c;
            continue;
        }

        if (p >= end)
            break;

        c = (unsigned char)*p++;
        switch (c) {
            case '"': out[w++] = '"'; break;
            case '\\': out[w++] = '\\'; break;
            case '/': out[w++] = '/'; break;
            case 'b': out[w++] = '\b'; break;
            case 'f': out[w++] = '\f'; break;
            case 'n': out[w++] = '\n'; break;
            case 'r': out[w++] = '\r'; break;
            case 't': out[w++] = '\t'; break;

            /*
             * \uXXXX is deliberately preserved as '?' here rather than
             * crashing. Spotify generally returns UTF-8 strings directly.
             * A full Unicode escape decoder can be added independently.
             */
            case 'u':
                if (end - p >= 4)
                    p += 4;
                out[w++] = '?';
                break;

            default:
                out[w++] = (char)c;
                break;
        }
    }

    out[w] = '\0';
    return 0;
}

static const char *scan_string_end(const char *p, const char *end)
{
    if (p >= end || *p != '"')
        return NULL;

    ++p;
    while (p < end) {
        if (*p == '\\') {
            ++p;
            if (p < end)
                ++p;
            continue;
        }

        if (*p == '"')
            return p;

        ++p;
    }
    return NULL;
}

static const char *scan_value_end(const char *p, const char *end)
{
    int object_depth = 0;
    int array_depth = 0;
    int in_string = 0;
    int escaped = 0;

    p = skip_ws(p, end);
    if (p >= end)
        return p;

    if (*p == '"') {
        const char *q = scan_string_end(p, end);
        return q ? q + 1 : end;
    }

    for (const char *q = p; q < end; ++q) {
        char c = *q;

        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (c == '\\') {
                escaped = 1;
            } else if (c == '"') {
                in_string = 0;
            }
            continue;
        }

        if (c == '"') {
            in_string = 1;
            continue;
        }

        if (c == '{') object_depth++;
        else if (c == '}') {
            if (object_depth == 0 && array_depth == 0)
                return q;
            object_depth--;
            if (object_depth == 0 && array_depth == 0)
                return q + 1;
        } else if (c == '[') array_depth++;
        else if (c == ']') {
            array_depth--;
            if (object_depth == 0 && array_depth == 0)
                return q + 1;
        } else if ((c == ',' || c == '}') && object_depth == 0 && array_depth == 0) {
            return q;
        }
    }

    return end;
}

static int object_find(
    const char *object,
    size_t object_size,
    const char *key,
    const char **value_start,
    const char **value_end
)
{
    const char *p = object;
    const char *end = object + object_size;

    p = skip_ws(p, end);
    if (p >= end || *p != '{')
        return -1;
    ++p;

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end || *p == '}')
            break;

        if (*p != '"')
            return -1;

        const char *key_end = scan_string_end(p, end);
        if (!key_end)
            return -1;

        const char *key_start = p + 1;
        size_t key_len = (size_t)(key_end - key_start);

        p = skip_ws(key_end + 1, end);
        if (p >= end || *p != ':')
            return -1;
        ++p;

        p = skip_ws(p, end);
        const char *vstart = p;
        const char *vend = scan_value_end(vstart, end);

        if (strlen(key) == key_len && memcmp(key_start, key, key_len) == 0) {
            if (value_start) *value_start = vstart;
            if (value_end) *value_end = vend;
            return 0;
        }

        p = skip_ws(vend, end);
        if (p < end && *p == ',')
            ++p;
    }

    return 1;
}

static int get_string(
    const char *object,
    size_t object_size,
    const char *key,
    char *out,
    size_t out_size
)
{
    const char *s = NULL;
    const char *e = NULL;

    if (object_find(object, object_size, key, &s, &e) != 0)
        return 1;

    s = skip_ws(s, e);
    if (s >= e || *s != '"')
        return 1;

    const char *q = scan_string_end(s, e);
    if (!q)
        return -1;

    return copy_unescaped_string(s + 1, q, out, out_size);
}

static int get_int(
    const char *object,
    size_t object_size,
    const char *key,
    int fallback
)
{
    const char *s = NULL;
    const char *e = NULL;

    if (object_find(object, object_size, key, &s, &e) != 0)
        return fallback;

    char temp[32];
    size_t n = (size_t)(e - s);
    if (n >= sizeof(temp))
        n = sizeof(temp) - 1;

    memcpy(temp, s, n);
    temp[n] = '\0';
    return atoi(temp);
}

static int get_bool(
    const char *object,
    size_t object_size,
    const char *key,
    int fallback
)
{
    const char *s = NULL;
    const char *e = NULL;

    if (object_find(object, object_size, key, &s, &e) != 0)
        return fallback;

    s = skip_ws(s, e);
    if ((size_t)(e - s) >= 4 && memcmp(s, "true", 4) == 0)
        return 1;
    if ((size_t)(e - s) >= 5 && memcmp(s, "false", 5) == 0)
        return 0;

    return fallback;
}

static int get_object(
    const char *object,
    size_t object_size,
    const char *key,
    const char **out_start,
    size_t *out_size
)
{
    const char *s = NULL;
    const char *e = NULL;

    if (object_find(object, object_size, key, &s, &e) != 0)
        return 1;

    s = skip_ws(s, e);
    if (s >= e || *s != '{')
        return 1;

    *out_start = s;
    *out_size = (size_t)(e - s);
    return 0;
}

static int get_array(
    const char *object,
    size_t object_size,
    const char *key,
    const char **out_start,
    size_t *out_size
)
{
    const char *s = NULL;
    const char *e = NULL;

    if (object_find(object, object_size, key, &s, &e) != 0)
        return 1;

    s = skip_ws(s, e);
    if (s >= e || *s != '[')
        return 1;

    *out_start = s;
    *out_size = (size_t)(e - s);
    return 0;
}

static int array_object_at(
    const char *array,
    size_t array_size,
    int index,
    const char **out_start,
    size_t *out_size
)
{
    const char *p = array;
    const char *end = array + array_size;
    int current = 0;

    p = skip_ws(p, end);
    if (p >= end || *p != '[')
        return -1;
    ++p;

    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end || *p == ']')
            break;

        const char *s = p;
        const char *e = scan_value_end(s, end);

        if (current == index) {
            if (*s != '{')
                return 1;
            *out_start = s;
            *out_size = (size_t)(e - s);
            return 0;
        }

        ++current;
        p = skip_ws(e, end);
        if (p < end && *p == ',')
            ++p;
    }

    return 1;
}

static int parse_track_object(const char *obj, size_t size, SpotifyTrack *out)
{
    const char *album = NULL;
    size_t album_size = 0;
    const char *artists = NULL;
    size_t artists_size = 0;

    memset(out, 0, sizeof(*out));

    if (get_string(obj, size, "name", out->title, sizeof(out->title)) != 0)
        return -1;

    get_string(obj, size, "id", out->id, sizeof(out->id));
    get_string(obj, size, "uri", out->uri, sizeof(out->uri));
    out->duration_ms = get_int(obj, size, "duration_ms", 0);

    if (get_array(obj, size, "artists", &artists, &artists_size) == 0) {
        const char *artist0 = NULL;
        size_t artist0_size = 0;
        if (array_object_at(artists, artists_size, 0, &artist0, &artist0_size) == 0)
            get_string(artist0, artist0_size, "name", out->artist, sizeof(out->artist));
    }

    if (get_object(obj, size, "album", &album, &album_size) == 0) {
        const char *images = NULL;
        size_t images_size = 0;

        get_string(album, album_size, "name", out->album, sizeof(out->album));

        if (get_array(album, album_size, "images", &images, &images_size) == 0) {
            const char *image0 = NULL;
            size_t image0_size = 0;
            if (array_object_at(images, images_size, 0, &image0, &image0_size) == 0)
                get_string(image0, image0_size, "url", out->cover_url, sizeof(out->cover_url));
        }
    }

    out->valid = 1;
    return 0;
}

int spotify_json_parse_player(
    const char *json,
    size_t size,
    SpotifyTrack *out
)
{
    const char *item = NULL;
    size_t item_size = 0;

    if (!json || !out || size == 0)
        return -1;

    memset(out, 0, sizeof(*out));

    if (get_object(json, size, "item", &item, &item_size) != 0)
        return 1; /* no active item */

    if (parse_track_object(item, item_size, out) < 0)
        return -2;

    out->progress_ms = get_int(json, size, "progress_ms", 0);
    out->is_playing = get_bool(json, size, "is_playing", 0);
    return 0;
}

int spotify_json_parse_queue(
    const char *json,
    size_t size,
    SpotifyQueue *out
)
{
    const char *current = NULL;
    size_t current_size = 0;
    const char *queue = NULL;
    size_t queue_size = 0;

    if (!json || !out || size == 0)
        return -1;

    memset(out, 0, sizeof(*out));

    if (get_object(json, size, "currently_playing", &current, &current_size) == 0)
        parse_track_object(current, current_size, &out->current);

    if (get_array(json, size, "queue", &queue, &queue_size) == 0) {
        for (int i = 0; i < SPOTIFY_MAX_QUEUE; ++i) {
            const char *obj = NULL;
            size_t obj_size = 0;

            if (array_object_at(queue, queue_size, i, &obj, &obj_size) != 0)
                break;

            if (parse_track_object(obj, obj_size, &out->items[out->count]) == 0)
                ++out->count;
        }
    }

    out->valid = 1;
    return 0;
}

int spotify_json_parse_token(
    const char *json,
    size_t size,
    SpotifyTokenResponse *out
)
{
    if (!json || !out || size == 0)
        return -1;

    memset(out, 0, sizeof(*out));

    if (get_string(json, size, "access_token",
                   out->access_token, sizeof(out->access_token)) != 0)
        return -2;

    out->expires_in = get_int(json, size, "expires_in", 3600);

    if (get_string(json, size, "refresh_token",
                   out->refresh_token, sizeof(out->refresh_token)) == 0)
        out->has_refresh_token = 1;

    return 0;
}

int spotify_json_get_error(
    const char *json,
    size_t size,
    char *out,
    size_t out_size
)
{
    if (!json || !out || out_size == 0)
        return -1;

    out[0] = '\0';
    return get_string(json, size, "error", out, out_size);
}
