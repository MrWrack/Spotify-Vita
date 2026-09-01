#include "test_cover.h"

#define ARGB(a,r,g,b) \
    (((uint32_t)(a) << 24) | \
     ((uint32_t)(r) << 16) | \
     ((uint32_t)(g) << 8) | \
     ((uint32_t)(b)))

void test_cover_generate(
    uint32_t pixels[
        TEST_COVER_W *
        TEST_COVER_H
    ]
)
{
    for (int y = 0; y < TEST_COVER_H; ++y) {
        for (int x = 0; x < TEST_COVER_W; ++x) {
            int dx = x - TEST_COVER_W / 2;
            int dy = y - TEST_COVER_H / 2;
            int d2 = dx * dx + dy * dy;

            uint32_t color;

            if (d2 < 44 * 44) {
                color = ARGB(
                    255,
                    30,
                    215,
                    96
                );
            } else {
                int stripe =
                    ((x / 8) + (y / 8)) & 1;

                color = stripe
                    ? ARGB(255, 20, 24, 22)
                    : ARGB(255, 8, 10, 9);
            }

            pixels[y * TEST_COVER_W + x] =
                color;
        }
    }
}
