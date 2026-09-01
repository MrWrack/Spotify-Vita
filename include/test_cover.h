#ifndef SPOTIFY_VITA_TEST_COVER_H
#define SPOTIFY_VITA_TEST_COVER_H

#include <stdint.h>

#define TEST_COVER_W 128
#define TEST_COVER_H 128

void test_cover_generate(
    uint32_t pixels[TEST_COVER_W * TEST_COVER_H]
);

#endif
