#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "input.h"

static uint16_t raw_buttons;

uint16_t hardware_read_buttons(void)
{
    return raw_buttons;
}

int main(void)
{
    input_init();
    raw_buttons = (uint16_t)1U << BUTTON_LEFT_A;

    for (unsigned i = 0; i < 4; ++i) {
        input_tick_1khz();
    }
    assert(input_buttons() == 0);
    assert(input_pressed_edges() == 0);

    input_tick_1khz();
    assert(input_buttons() == ((uint16_t)1U << BUTTON_LEFT_A));
    assert(input_pressed_edges() == ((uint16_t)1U << BUTTON_LEFT_A));
    assert(input_pressed_edges() == 0);

    raw_buttons = 0;
    for (unsigned i = 0; i < 4; ++i) {
        input_tick_1khz();
    }
    assert(input_buttons() != 0);
    input_tick_1khz();
    assert(input_buttons() == 0);

    raw_buttons = ((uint16_t)1U << BUTTON_TEST) |
                  ((uint16_t)1U << BUTTON_SERVICE);
    for (unsigned i = 0; i < 5; ++i) {
        input_tick_1khz();
    }
    assert(input_buttons() == raw_buttons);
    assert(input_pressed_edges() == raw_buttons);

    puts("input tests passed");
    return 0;
}
