#ifndef APP_INPUT_H
#define APP_INPUT_H

#include <stdint.h>

typedef struct {
    uint32_t buttons_down;
    uint32_t buttons_held;

    int touch_active;
    int touch_x;
    int touch_y;
} AppInput;

int app_input_init(void);
int app_input_poll(AppInput *out);

#endif
