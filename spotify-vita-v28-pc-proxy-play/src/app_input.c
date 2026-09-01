#include "app_input.h"

#include <psp2/ctrl.h>
#include <psp2/touch.h>

#include <string.h>

static uint32_t g_previous_buttons = 0;

int app_input_init(void)
{
    sceCtrlSetSamplingMode(
        SCE_CTRL_MODE_ANALOG
    );

    sceTouchSetSamplingState(
        SCE_TOUCH_PORT_FRONT,
        SCE_TOUCH_SAMPLING_STATE_START
    );

    g_previous_buttons = 0;
    return 0;
}

int app_input_poll(
    AppInput *out
)
{
    if (!out)
        return -1;

    memset(out, 0, sizeof(*out));

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));

    int rc = sceCtrlPeekBufferPositive(
        0,
        &pad,
        1
    );

    if (rc < 0)
        return rc;

    out->buttons_held = pad.buttons;
    out->buttons_down =
        pad.buttons & ~g_previous_buttons;

    g_previous_buttons = pad.buttons;

    SceTouchData touch;
    memset(&touch, 0, sizeof(touch));

    rc = sceTouchPeek(
        SCE_TOUCH_PORT_FRONT,
        &touch,
        1
    );

    if (rc >= 0 && touch.reportNum > 0) {
        out->touch_active = 1;

        /*
         * Vita front touch resolution is 1920x1088 while the display is
         * 960x544, so divide by two for screen-space UI hit testing.
         */
        out->touch_x =
            touch.report[0].x / 2;

        out->touch_y =
            touch.report[0].y / 2;
    }

    return 0;
}
