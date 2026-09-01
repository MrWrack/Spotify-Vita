#include "vita2d_renderer.h"

#include <vita2d.h>

#include <stdio.h>
#include <string.h>

#define COLOR_BG       RGBA8(4, 6, 7, 255)
#define COLOR_PANEL    RGBA8(14, 18, 17, 255)
#define COLOR_PANEL_2  RGBA8(23, 29, 26, 255)
#define COLOR_GREEN    RGBA8(30, 215, 96, 255)
#define COLOR_GREEN_2  RGBA8(20, 150, 72, 255)
#define COLOR_DIM      RGBA8(105, 118, 110, 255)
#define COLOR_WHITE    RGBA8(240, 245, 242, 255)
#define COLOR_RED      RGBA8(230, 70, 70, 255)
#define COLOR_BLACK    RGBA8(3, 5, 5, 255)

static void rect(float x, float y, float w, float h, unsigned int color)
{
    vita2d_draw_rectangle(x, y, w, h, color);
}

static void frame(float x, float y, float w, float h, float t, unsigned int color)
{
    rect(x, y, w, t, color);
    rect(x, y + h - t, w, t, color);
    rect(x, y, t, h, color);
    rect(x + w - t, y, t, h, color);
}

static void text(
    const Vita2DRenderer *r,
    int x,
    int y,
    float scale,
    unsigned int color,
    const char *value
)
{
    if (!r || !r->font || !value)
        return;

    vita2d_pgf_draw_text(
        r->font,
        x,
        y,
        color,
        scale,
        value
    );
}

static void clipped_text(
    const Vita2DRenderer *r,
    int x,
    int y,
    float scale,
    unsigned int color,
    const char *value,
    int max_chars
)
{
    if (!value)
        value = "";

    char buf[128];
    int n = (int)strlen(value);

    if (n > max_chars)
        n = max_chars;

    if (n > (int)sizeof(buf) - 4)
        n = (int)sizeof(buf) - 4;

    memcpy(buf, value, (size_t)n);
    buf[n] = '\0';

    if (value[n] != '\0' && n >= 3) {
        buf[n - 3] = '.';
        buf[n - 2] = '.';
        buf[n - 1] = '.';
    }

    text(r, x, y, scale, color, buf);
}

static void header(
    const Vita2DRenderer *r,
    const AppController *app,
    const char *title
)
{
    rect(0, 0, 960, 64, COLOR_PANEL);
    rect(0, 61, 960, 3, COLOR_GREEN);

    text(r, 28, 40, 1.25f, COLOR_GREEN, "SPOTIFY VITA");
    text(r, 390, 39, 0.90f, COLOR_WHITE, title);

    /*
     * Header status reflects the Vita's real NetCtl connection now,
     * not Spotify authentication state.
     */
    if (app->network_connected) {
        rect(824, 23, 9, 9, COLOR_GREEN);
        text(r, 840, 36, 0.66f, COLOR_GREEN, "NET ONLINE");
    } else {
        rect(824, 23, 9, 9, COLOR_RED);
        text(r, 840, 36, 0.66f, COLOR_RED, "NET OFFLINE");
    }
}

static void draw_cover_or_placeholder(
    const Vita2DRenderer *renderer,
    float x,
    float y,
    float size
)
{
    if (renderer && renderer->pipeline_cover) {
        unsigned int width =
            vita2d_texture_get_width(renderer->pipeline_cover);
        unsigned int height =
            vita2d_texture_get_height(renderer->pipeline_cover);

        if (width > 0 && height > 0) {
            vita2d_draw_texture_scale(
                renderer->pipeline_cover,
                x,
                y,
                size / (float)width,
                size / (float)height
            );
            return;
        }
    }

    if (renderer &&
        renderer->current_cover.ready &&
        renderer->current_cover.texture) {
        float sx = size / (float)renderer->current_cover.width;
        float sy = size / (float)renderer->current_cover.height;

        vita2d_draw_texture_scale(
            renderer->current_cover.texture,
            x,
            y,
            sx,
            sy
        );
        return;
    }

    rect(x, y, size, size, COLOR_PANEL_2);
    frame(x, y, size, size, 2, COLOR_GREEN_2);
    rect(x + size * 0.23f, y + size * 0.23f,
         size * 0.54f, size * 0.54f, COLOR_GREEN_2);
}

static void compact_player(
    const Vita2DRenderer *r,
    const AppController *app
)
{
    rect(0, 458, 960, 86, COLOR_PANEL);
    rect(0, 458, 960, 2, COLOR_GREEN_2);

    draw_cover_or_placeholder(r, 20, 472, 58);

    if (app->now_playing.has_track) {
        clipped_text(r, 96, 493, 0.88f, COLOR_WHITE,
                     app->now_playing.track.title, 34);
        clipped_text(r, 96, 518, 0.70f, COLOR_DIM,
                     app->now_playing.track.artist, 34);

        int duration = app->now_playing.track.duration_ms;
        int progress = app->now_playing.track.progress_ms;

        rect(520, 502, 300, 5, COLOR_DIM);

        if (duration > 0) {
            float ratio = (float)progress / (float)duration;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            rect(520, 502, 300.0f * ratio, 5, COLOR_GREEN);
        }

        text(r, 845, 516, 0.95f, COLOR_GREEN,
             app->now_playing.track.is_playing ? "II" : ">");
        text(r, 888, 516, 0.80f, COLOR_WHITE, "NOW");
    } else {
        text(r, 96, 501, 0.82f, COLOR_DIM, "Ingen lat spelas just nu");
        text(r, 96, 524, 0.67f, COLOR_DIM, "Starta Spotify pa en enhet");
    }
}

static void nav_item(
    const Vita2DRenderer *r,
    int y,
    const char *label,
    int selected
)
{
    if (selected) {
        rect(24, y - 33, 270, 46, COLOR_PANEL_2);
        rect(24, y - 33, 5, 46, COLOR_GREEN);
        text(r, 48, y, 1.02f, COLOR_GREEN, label);
    } else {
        text(r, 48, y, 0.95f, COLOR_WHITE, label);
    }
}

static void login_screen(
    const Vita2DRenderer *r,
    const AppController *app
)
{
    header(r, app, "LOGIN");

    text(r, 365, 135, 1.45f, COLOR_WHITE, "VALKOMMEN");
    if (1) {
        frame(300, 220, 360, 86, 3, COLOR_GREEN);
        text(r, 278, 274, 1.00f, COLOR_GREEN, ">");
    }

    rect(310, 230, 340, 66, COLOR_GREEN);
    text(r, 389, 272, 1.05f, COLOR_BLACK, "LOGGA IN MED SPOTIFY");
    text(r, 900, 525, 0.55f, COLOR_DIM, "v26");
}

static void home_screen(
    const Vita2DRenderer *r,
    const AppController *app
)
{
    header(r, app, "HOME");

    text(r, 28, 91, 0.70f, COLOR_DIM, "MENY");
    nav_item(r, 143, "Home",     app->selected_nav == 0);
    nav_item(r, 201, "Search",   app->selected_nav == 1);
    nav_item(r, 259, "Library",  app->selected_nav == 2);
    nav_item(r, 317, "Settings", app->selected_nav == 3);

    rect(330, 92, 2, 330, COLOR_PANEL_2);

    text(r, 370, 118, 1.25f, COLOR_WHITE, "Good evening");
    text(r, 370, 149, 0.72f, COLOR_DIM,
         "Din Spotify-uppspelning pa PS Vita");

    draw_cover_or_placeholder(r, 370, 182, 210);

    if (app->now_playing.has_track) {
        clipped_text(r, 610, 216, 1.05f, COLOR_WHITE,
                     app->now_playing.track.title, 25);
        clipped_text(r, 610, 251, 0.78f, COLOR_GREEN,
                     app->now_playing.track.artist, 28);
        clipped_text(r, 610, 281, 0.70f, COLOR_DIM,
                     app->now_playing.track.album, 30);

        rect(610, 330, 225, 52, COLOR_GREEN);
        text(r, 653, 364, 0.90f, COLOR_BLACK, "NOW PLAYING");
    } else {
        text(r, 610, 226, 0.92f, COLOR_WHITE, "Ingen aktiv lat");
        text(r, 610, 257, 0.72f, COLOR_DIM,
             "Spela nagot i Spotify");
        text(r, 610, 282, 0.72f, COLOR_DIM,
             "sa visas det har.");
    }

    text(r, 28, 425, 0.67f, COLOR_DIM,
         "D-pad: valj   X: oppna   L/R: forra/nasta   START: avsluta");

    compact_player(r, app);
}

static void content_screen(
    const Vita2DRenderer *r,
    const AppController *app,
    const char *title,
    const char *headline,
    const char *line1,
    const char *line2
)
{
    header(r, app, title);

    text(r, 54, 132, 1.42f, COLOR_WHITE, headline);
    rect(54, 156, 130, 3, COLOR_GREEN);

    rect(54, 198, 852, 178, COLOR_PANEL);
    text(r, 82, 245, 0.90f, COLOR_WHITE, line1);
    text(r, 82, 282, 0.76f, COLOR_DIM, line2);
    compact_player(r, app);
}

static void now_playing_screen(
    const Vita2DRenderer *r,
    const AppController *app
)
{
    header(r, app, "NOW PLAYING");

    draw_cover_or_placeholder(r, 64, 105, 320);

    if (!app->now_playing.has_track) {
        text(r, 438, 190, 1.10f, COLOR_WHITE, "Ingen aktiv lat");
        text(r, 438, 225, 0.76f, COLOR_DIM,
             "Starta uppspelning i Spotify.");
        return;
    }

    clipped_text(r, 430, 137, 1.35f, COLOR_WHITE,
                 app->now_playing.track.title, 30);
    clipped_text(r, 430, 181, 0.92f, COLOR_GREEN,
                 app->now_playing.track.artist, 34);
    clipped_text(r, 430, 215, 0.74f, COLOR_DIM,
                 app->now_playing.track.album, 40);

    rect(430, 276, 440, 7, COLOR_DIM);

    if (app->now_playing.track.duration_ms > 0) {
        float ratio =
            (float)app->now_playing.track.progress_ms /
            (float)app->now_playing.track.duration_ms;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        rect(430, 276, 440.0f * ratio, 7, COLOR_GREEN);
    }

    rect(458, 328, 112, 72, COLOR_PANEL_2);
    rect(594, 318, 128, 92, COLOR_GREEN);
    rect(746, 328, 112, 72, COLOR_PANEL_2);

    text(r, 493, 374, 1.15f, COLOR_WHITE, "<<");
    text(r, 644, 374, 1.20f, COLOR_BLACK,
         app->now_playing.track.is_playing ? "II" : ">");
    text(r, 781, 374, 1.15f, COLOR_WHITE, ">>");
}

static void error_screen(
    const Vita2DRenderer *r,
    const AppController *app
)
{
    header(r, app, "ERROR");

    text(r, 320, 180, 1.45f, COLOR_RED, "NAGOT GICK FEL");

    const char *stage = "UNKNOWN";

    switch (app->last_error_stage) {
        case APP_ERROR_STAGE_LOGIN:
            stage = "LOGIN / PKCE";
            break;
        case APP_ERROR_STAGE_APP_URI_CALLBACK:
            stage = "APP URI CALLBACK";
            break;
        case APP_ERROR_STAGE_BROWSER:
            stage = "VITA BROWSER";
            break;
        case APP_ERROR_STAGE_NETWORK:
            stage = "VITA NETWORK";
            break;
        case APP_ERROR_STAGE_SPOTIFY_HTTP:
            stage = "SPOTIFY HTTP";
            break;
        case APP_ERROR_STAGE_NONE:
        default:
            stage = "UNKNOWN";
            break;
    }

    char buf[112];
    snprintf(buf, sizeof(buf), "STEG: %s", stage);
    text(r, 337, 225, 0.82f, COLOR_GREEN, buf);

    if (app->last_error_stage == APP_ERROR_STAGE_APP_URI_CALLBACK) {
        snprintf(buf, sizeof(buf), "APP URI callback Error: %d",
                 app->last_error);
    } else if (app->last_error_stage == APP_ERROR_STAGE_NETWORK) {
        snprintf(buf, sizeof(buf), "NetCtl state: %d   Anslut Wi-Fi",
                 app->network_state);
    } else if (app->last_error_stage == APP_ERROR_STAGE_SPOTIFY_HTTP) {
        snprintf(buf, sizeof(buf), "Error: %d   HTTP: %d",
                 app->last_error, app->last_http_status);
    } else {
        if (app->last_error == -3001) {
            snprintf(buf, sizeof(buf), "Spotify Client ID saknas i spotify_config.h");
        } else {
            snprintf(buf, sizeof(buf), "Error: %d   (ingen HTTP-request an)",
                     app->last_error);
        }
    }

    text(r, 260, 260, 0.72f, COLOR_WHITE, buf);
}

int vita2d_renderer_init(Vita2DRenderer *renderer)
{
    if (!renderer)
        return -1;

    memset(renderer, 0, sizeof(*renderer));

    if (vita2d_init() <= 0)
        return -2;

    vita2d_set_vblank_wait(1);
    vita2d_set_clear_color(COLOR_BG);

    renderer->font = vita2d_load_default_pgf();

    renderer->initialized = 1;
    return 0;
}

void vita2d_renderer_shutdown(Vita2DRenderer *renderer)
{
    if (!renderer)
        return;

    cover_vita2d_destroy(&renderer->current_cover);

    if (renderer->font) {
        vita2d_free_pgf(renderer->font);
        renderer->font = NULL;
    }

    if (renderer->initialized)
        vita2d_fini();

    memset(renderer, 0, sizeof(*renderer));
}

void vita2d_renderer_set_cover(
    Vita2DRenderer *renderer,
    const char *url,
    const uint32_t *argb_pixels,
    unsigned int width,
    unsigned int height
)
{
    if (!renderer || !argb_pixels || width == 0 || height == 0)
        return;

    CoverVita2D next;

    if (cover_vita2d_from_argb8888(
            argb_pixels, width, height, &next) < 0)
        return;

    cover_vita2d_destroy(&renderer->current_cover);
    renderer->current_cover = next;

    if (url) {
        strncpy(renderer->current_cover_url, url,
                sizeof(renderer->current_cover_url) - 1);
        renderer->current_cover_url[
            sizeof(renderer->current_cover_url) - 1] = '\0';
    } else {
        renderer->current_cover_url[0] = '\0';
    }
}

void vita2d_renderer_set_pipeline_cover(
    Vita2DRenderer *renderer,
    int handle,
    const char *url,
    vita2d_texture *texture
)
{
    if (!renderer)
        return;

    renderer->pipeline_cover = texture;
    renderer->pipeline_cover_handle = handle;

    if (url) {
        strncpy(renderer->current_cover_url, url,
                sizeof(renderer->current_cover_url) - 1);
        renderer->current_cover_url[
            sizeof(renderer->current_cover_url) - 1] = '\0';
    } else {
        renderer->current_cover_url[0] = '\0';
    }
}

void vita2d_renderer_draw(
    Vita2DRenderer *renderer,
    const AppController *app
)
{
    if (!renderer || !renderer->initialized || !app)
        return;

    vita2d_start_drawing();
    vita2d_clear_screen();

    switch (app->screen) {
        case APP_SCREEN_LOGIN:
            login_screen(renderer, app);
            break;

        case APP_SCREEN_HOME:
            home_screen(renderer, app);
            break;

        case APP_SCREEN_SEARCH:
            content_screen(renderer, app, "SEARCH", "Search",
                           "Sok efter musik fran Spotify.",
                           "Sokfunktionen kopplas till Spotify API i nasta steg.");
            break;

        case APP_SCREEN_LIBRARY:
            content_screen(renderer, app, "LIBRARY", "Your Library",
                           "Dina sparade Spotify-objekt.",
                           "Biblioteksvyn ar redo for riktiga API-resultat.");
            break;

        case APP_SCREEN_SETTINGS:
            content_screen(renderer, app, "SETTINGS", "Settings",
                           "Spotify Vita",
                           "D-pad + X/O + touch. START stanger appen.");
            break;

        case APP_SCREEN_NOW_PLAYING:
            now_playing_screen(renderer, app);
            break;

        case APP_SCREEN_ERROR:
        default:
            error_screen(renderer, app);
            break;
    }

    vita2d_end_drawing();
    vita2d_swap_buffers();
}
