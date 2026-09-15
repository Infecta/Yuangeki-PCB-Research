#include "board.h"
#include "input.h"

#define DEBOUNCE_MAX 5U

static uint8_t counters[BUTTON_COUNT];
static volatile uint16_t stable_buttons;
static volatile uint16_t pressed;

void input_init(void)
{
    stable_buttons = 0;
    pressed = 0;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        counters[i] = 0;
    }
}

void input_tick_1khz(void)
{
    const uint16_t raw = hardware_read_buttons();

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        const uint16_t bit = (uint16_t)1U << i;
        if ((raw & bit) != 0U) {
            if (counters[i] < DEBOUNCE_MAX) {
                ++counters[i];
            }
            if (counters[i] == DEBOUNCE_MAX && (stable_buttons & bit) == 0U) {
                stable_buttons |= bit;
                pressed |= bit;
            }
        } else {
            if (counters[i] > 0U) {
                --counters[i];
            }
            if (counters[i] == 0U) {
                stable_buttons &= (uint16_t)~bit;
            }
        }
    }
}

uint16_t input_buttons(void)
{
    return stable_buttons;
}

uint16_t input_pressed_edges(void)
{
    const uint16_t result = pressed;
    pressed = 0;
    return result;
}
