#include "ui_renderer.h"

#include "now_playing.h"

#include <stdio.h>
#include <string.h>

#define ARGB(a,r,g,b) \
    (((uint32_t)(a) << 24) | \
     ((uint32_t)(b) << 16) | \
     ((uint32_t)(g) << 8) | \
     ((uint32_t)(r)))

static const uint32_t C_BG =
    ARGB(255, 5, 7, 8);

static const uint32_t C_PANEL =
    ARGB(255, 15, 20, 18);

static const uint32_t C_GREEN =
    ARGB(255, 30, 215, 96);

static const uint32_t C_DIM =
    ARGB(255, 65, 85, 72);

static const uint32_t C_WHITE =
    ARGB(255, 235, 245, 238);

static const uint32_t C_RED =
    ARGB(255, 230, 70, 70);

static void fill_rect(
    UiSurface *s,
    int x,
    int y,
    int w,
    int h,
    uint32_t color
)
{
    if (!s || !s->pixels)
        return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }

    if (x + w > s->width)
        w = s->width - x;

    if (y + h > s->height)
        h = s->height - y;

    if (w <= 0 || h <= 0)
        return;

    for (int yy = y; yy < y + h; ++yy) {
        uint32_t *row =
            s->pixels + yy * s->stride + x;

        for (int xx = 0; xx < w; ++xx)
            row[xx] = color;
    }
}

static void border_rect(
    UiSurface *s,
    int x,
    int y,
    int w,
    int h,
    int thickness,
    uint32_t color
)
{
    fill_rect(s, x, y, w, thickness, color);
    fill_rect(s, x, y+h-thickness, w, thickness, color);
    fill_rect(s, x, y, thickness, h, color);
    fill_rect(s, x+w-thickness, y, thickness, h, color);
}

/*
 * Compact built-in 3x5 pixel font.
 * This avoids requiring PGF/freefont before the screen renderer is usable.
 */
static const unsigned short glyphs[38] = {
    /* A-Z */
    0x7B6F,0x7B6E,0x7927,0x6B6E,0x79A7,0x79A4,0x79EB,0x5BED,
    0x7497,0x249E,0x5BAD,0x4927,0x5FED,0x5BED,0x7B6F,0x7BE4,
    0x7B7B,0x7BEA,0x79CF,0x7492,0x5B6F,0x5B6A,0x5FF5,0x5AAD,
    0x5A92,0x72A7,
    /* 0-9 */
    0x7B6F,0x2492,0x73E7,0x73CF,0x5BC9,0x79CF,0x79EF,0x7249,
    0x7BEF,0x7BCF,
    /* ':' '-' */
    0x00A0,0x01C0
};

static unsigned short glyph_for(char c)
{
    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');

    if (c >= 'A' && c <= 'Z')
        return glyphs[c - 'A'];

    if (c >= '0' && c <= '9')
        return glyphs[26 + (c - '0')];

    if (c == ':')
        return glyphs[36];

    if (c == '-')
        return glyphs[37];

    return 0;
}

static void draw_char(
    UiSurface *s,
    int x,
    int y,
    char c,
    int scale,
    uint32_t color
)
{
    unsigned short bits = glyph_for(c);
    if (!bits)
        return;

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            int bit = 14 - (row * 3 + col);
            if (bits & (1u << bit)) {
                fill_rect(
                    s,
                    x + col * scale,
                    y + row * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }
}

static void draw_text(
    UiSurface *s,
    int x,
    int y,
    const char *text,
    int scale,
    uint32_t color
)
{
    if (!text)
        return;

    int cursor = x;

    while (*text) {
        if (*text == ' ') {
            cursor += 4 * scale;
        } else {
            draw_char(
                s,
                cursor,
                y,
                *text,
                scale,
                color
            );

            cursor += 4 * scale;
        }

        ++text;
    }
}

static void clear(
    UiSurface *s,
    uint32_t color
)
{
    fill_rect(
        s,
        0, 0,
        s->width,
        s->height,
        color
    );
}

static void draw_header(
    UiSurface *s,
    const char *right
)
{
    fill_rect(s, 0, 0, s->width, 58, C_PANEL);
    fill_rect(s, 0, 55, s->width, 3, C_GREEN);

    draw_text(
        s,
        30,
        19,
        "SPOTIFY VITA",
        3,
        C_GREEN
    );

    if (right) {
        draw_text(
            s,
            760,
            21,
            right,
            2,
            C_WHITE
        );
    }
}

static void draw_compact_player(
    UiSurface *s,
    const AppController *app
)
{
    fill_rect(
        s,
        0,
        s->height - 86,
        s->width,
        86,
        C_PANEL
    );

    fill_rect(
        s,
        0,
        s->height - 86,
        s->width,
        2,
        C_DIM
    );

    border_rect(
        s,
        22,
        s->height - 69,
        52,
        52,
        2,
        C_GREEN
    );

    if (!app->now_playing.has_track) {
        draw_text(
            s,
            95,
            s->height - 56,
            "NO ACTIVE TRACK",
            2,
            C_DIM
        );

        return;
    }

    draw_text(
        s,
        95,
        s->height - 65,
        app->now_playing.track.title,
        2,
        C_WHITE
    );

    draw_text(
        s,
        95,
        s->height - 38,
        app->now_playing.track.artist,
        1,
        C_GREEN
    );

    int duration =
        app->now_playing.track.duration_ms;

    int progress =
        app->now_playing.track.progress_ms;

    int bar_x = 540;
    int bar_y = s->height - 44;
    int bar_w = 330;

    fill_rect(
        s,
        bar_x,
        bar_y,
        bar_w,
        5,
        C_DIM
    );

    if (duration > 0) {
        int filled =
            (int)((long long)bar_w *
                  progress /
                  duration);

        if (filled < 0) filled = 0;
        if (filled > bar_w) filled = bar_w;

        fill_rect(
            s,
            bar_x,
            bar_y,
            filled,
            5,
            C_GREEN
        );
    }
}

static void draw_login(
    UiSurface *s,
    const AppController *app
)
{
    (void)app;

    draw_header(s, "LOGIN");

    border_rect(
        s,
        265,
        145,
        430,
        220,
        3,
        C_GREEN
    );

    fill_rect(
        s,
        275,
        155,
        410,
        200,
        C_PANEL
    );

    draw_text(
        s,
        338,
        192,
        "SPOTIFY LOGIN",
        4,
        C_GREEN
    );

    draw_text(
        s,
        335,
        265,
        "PRESS X TO LOGIN",
        2,
        C_WHITE
    );

    draw_text(
        s,
        326,
        305,
        "OR TAP THE SCREEN",
        2,
        C_DIM
    );
}

static void draw_home(
    UiSurface *s,
    const AppController *app
)
{
    draw_header(
        s,
        app->auth.authenticated
            ? "ONLINE"
            : "OFFLINE"
    );

    const char *items[] = {
        "HOME",
        "SEARCH",
        "LIBRARY",
        "QUEUE",
        "PLAYLISTS",
        "SETTINGS"
    };

    int y = 92;

    for (int i = 0; i < 6; ++i) {
        uint32_t color =
            i == 0 ? C_GREEN : C_WHITE;

        if (i == 0)
            fill_rect(s, 24, y-7, 8, 29, C_GREEN);

        draw_text(
            s,
            52,
            y,
            items[i],
            2,
            color
        );

        y += 48;
    }

    draw_text(
        s,
        535,
        108,
        "NOW PLAYING",
        2,
        C_GREEN
    );

    border_rect(
        s,
        530,
        145,
        180,
        180,
        3,
        C_GREEN
    );

    /*
     * Album-art placeholder. The GPU texture quad is deliberately not faked:
     * cover_texture.c owns GXM textures, while a shader textured-quad renderer
     * is the next isolated module.
     */
    fill_rect(
        s,
        542,
        157,
        156,
        156,
        C_PANEL
    );

    if (app->now_playing.has_track) {
        draw_text(
            s,
            735,
            160,
            app->now_playing.track.title,
            2,
            C_WHITE
        );

        draw_text(
            s,
            735,
            195,
            app->now_playing.track.artist,
            1,
            C_GREEN
        );
    } else {
        draw_text(
            s,
            735,
            180,
            "NO TRACK",
            2,
            C_DIM
        );
    }

    draw_compact_player(
        s,
        app
    );
}

static void draw_now_playing(
    UiSurface *s,
    const AppController *app
)
{
    draw_header(s, "NOW PLAYING");

    border_rect(
        s,
        70,
        100,
        290,
        290,
        3,
        C_GREEN
    );

    fill_rect(
        s,
        84,
        114,
        262,
        262,
        C_PANEL
    );

    if (!app->now_playing.has_track) {
        draw_text(
            s,
            465,
            180,
            "NO ACTIVE TRACK",
            3,
            C_DIM
        );

        draw_text(
            s,
            420,
            275,
            "O BACK",
            2,
            C_WHITE
        );

        return;
    }

    draw_text(
        s,
        420,
        126,
        app->now_playing.track.title,
        3,
        C_WHITE
    );

    draw_text(
        s,
        420,
        175,
        app->now_playing.track.artist,
        2,
        C_GREEN
    );

    draw_text(
        s,
        420,
        210,
        app->now_playing.track.album,
        1,
        C_DIM
    );

    int bar_x = 420;
    int bar_y = 286;
    int bar_w = 430;

    fill_rect(
        s,
        bar_x,
        bar_y,
        bar_w,
        7,
        C_DIM
    );

    if (app->now_playing.track.duration_ms > 0) {
        int filled =
            (int)((long long)bar_w *
                app->now_playing.track.progress_ms /
                app->now_playing.track.duration_ms);

        if (filled < 0) filled = 0;
        if (filled > bar_w) filled = bar_w;

        fill_rect(
            s,
            bar_x,
            bar_y,
            filled,
            7,
            C_GREEN
        );
    }

    draw_text(
        s,
        455,
        345,
        "LEFT PREV",
        2,
        C_WHITE
    );

    draw_text(
        s,
        625,
        345,
        "X PLAY",
        2,
        C_GREEN
    );

    draw_text(
        s,
        755,
        345,
        "RIGHT NEXT",
        2,
        C_WHITE
    );

    draw_text(
        s,
        420,
        405,
        "O BACK",
        2,
        C_DIM
    );
}

static void draw_error(
    UiSurface *s,
    const AppController *app
)
{
    draw_header(s, "ERROR");

    draw_text(
        s,
        280,
        180,
        "SPOTIFY ERROR",
        4,
        C_RED
    );

    char code[64];
    snprintf(
        code,
        sizeof(code),
        "HTTP %d",
        app->last_http_status
    );

    draw_text(
        s,
        390,
        270,
        code,
        2,
        C_WHITE
    );

    draw_text(
        s,
        340,
        330,
        "RESTART OR LOGIN AGAIN",
        2,
        C_DIM
    );
}

void ui_renderer_draw(
    UiSurface *surface,
    const AppController *app
)
{
    if (!surface || !surface->pixels || !app)
        return;

    clear(
        surface,
        C_BG
    );

    switch (app->screen) {
        case APP_SCREEN_LOGIN:
            draw_login(surface, app);
            break;

        case APP_SCREEN_HOME:
            draw_home(surface, app);
            break;

        case APP_SCREEN_NOW_PLAYING:
            draw_now_playing(surface, app);
            break;

        case APP_SCREEN_ERROR:
        default:
            draw_error(surface, app);
            break;
    }
}
