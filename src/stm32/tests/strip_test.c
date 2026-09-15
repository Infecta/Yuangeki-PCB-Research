#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "strip.h"

static uint8_t captured[STRIP_MAX_PIXELS * 3U];
static uint16_t captured_length;

void hardware_write_addressable(const uint8_t *data, uint16_t length)
{
    assert(length <= sizeof(captured));
    memcpy(captured, data, length);
    captured_length = length;
}

int main(void)
{
    strip_init();
    assert(captured_length == 16U * 3U);
    assert(captured[0] == 0U && captured[1] == 0U && captured[2] == 0U);

    strip_apply_settings(1, 255, 0, 255, 0, 0, 0, 16);
    assert(captured[0] == 0U);
    assert(captured[1] == 255U);
    assert(captured[2] == 0U);

    strip_apply_settings(0, 255, 0, 255, 255, 255, 0, 16);
    for (uint16_t i = 0; i < captured_length; ++i) {
        assert(captured[i] == 0U);
    }

    puts("strip tests passed");
    return 0;
}
