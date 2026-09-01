#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>

#include "app_controller.h"
#include "app_cover.h"
#include "app_input.h"
#include "app_ui.h"
#include "cover_pipeline.h"
#include "spotify_http.h"
#include "test_cover.h"
#include "vita2d_renderer.h"
#include "vita_network.h"

int main(void)
{
    int rc = vita_network_init();
    if (rc < 0)
        sceKernelExitProcess(1);

    rc = spotify_http_init();
    if (rc < 0) {
        vita_network_shutdown();
        sceKernelExitProcess(2);
    }

    Vita2DRenderer renderer;

    rc = vita2d_renderer_init(
        &renderer
    );

    if (rc < 0) {
        spotify_http_shutdown();
        vita_network_shutdown();
        sceKernelExitProcess(3);
    }

    rc = cover_pipeline_init();
    if (rc < 0) {
        vita2d_renderer_shutdown(&renderer);
        spotify_http_shutdown();
        vita_network_shutdown();
        sceKernelExitProcess(4);
    }

    rc = cover_pipeline_start();
    if (rc < 0) {
        cover_pipeline_shutdown();
        vita2d_renderer_shutdown(&renderer);
        spotify_http_shutdown();
        vita_network_shutdown();
        sceKernelExitProcess(5);
    }

    AppController app;

    rc = app_controller_init(
        &app
    );

    if (rc < 0) {
        cover_pipeline_shutdown();
        vita2d_renderer_shutdown(&renderer);
        spotify_http_shutdown();
        vita_network_shutdown();
        sceKernelExitProcess(6);
    }

    AppCoverState covers;
    app_cover_init(&covers);

    app_input_init();

    /*
     * Keep the generated cover as a fallback until the first real Spotify
     * cover finishes downloading and decoding.
     */
    static uint32_t test_pixels[
        TEST_COVER_W *
        TEST_COVER_H
    ];

    test_cover_generate(test_pixels);

    vita2d_renderer_set_cover(
        &renderer,
        "test://spotify-cover",
        test_pixels,
        TEST_COVER_W,
        TEST_COVER_H
    );

    int running = 1;

    while (running) {
        AppInput input;

        if (app_input_poll(&input) >= 0) {
            if (input.buttons_down &
                SCE_CTRL_START) {
                running = 0;
            } else {
                UiAction action =
                    app_ui_action_from_input(
                        &app,
                        &input
                    );

                app_ui_execute_action(
                    &app,
                    action
                );
            }
        }

        app_controller_update(
            &app
        );

        /*
         * UI/GXM thread stage:
         * downloaded bytes -> PNG/JPEG decoder -> vita2d texture -> LRU.
         */
        cover_pipeline_update_ui();

        /*
         * Request current at priority 100 and queue preloads at 90/80, then
         * bind the current READY texture to the renderer.
         */
        app_cover_update(
            &covers,
            &app,
            &renderer
        );

        vita2d_renderer_draw(
            &renderer,
            &app
        );
    }

    /*
     * Release renderer references before destroying cached GPU textures.
     */
    app_cover_shutdown(
        &covers,
        &renderer
    );

    app_controller_shutdown(
        &app
    );

    cover_pipeline_shutdown();

    vita2d_renderer_shutdown(
        &renderer
    );

    spotify_http_shutdown();
    vita_network_shutdown();

    sceKernelExitProcess(0);
    return 0;
}
