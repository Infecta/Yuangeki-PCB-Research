#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "board.h"
#include "lighting.h"

static uint16_t last_led_mask;

void hardware_write_button_leds(uint16_t on_mask)
{
    last_led_mask = on_mask;
}

void hardware_write_sensor_drive(uint8_t high)
{
    (void)high;
}

static unsigned count_on_cycles(uint8_t button)
{
    unsigned count = 0;
    const uint16_t bit = (uint16_t)1U << button;
    for (unsigned i = 0; i < 64; ++i) {
        lighting_pwm_tick();
        if ((last_led_mask & bit) != 0U) {
            ++count;
        }
    }
    return count;
}

int main(void)
{
    lighting_init();

    lighting_tick_1khz(0, 0);
    assert(count_on_cycles(BUTTON_LEFT_A) == 20);

    lighting_tick_1khz((uint16_t)1U << BUTTON_LEFT_A, 0);
    assert(count_on_cycles(BUTTON_LEFT_A) == 64);

    lighting_tick_1khz(0, (uint16_t)1U << BUTTON_TEST);
    lighting_tick_1khz(0, (uint16_t)1U << BUTTON_SERVICE);
    assert(lighting_mode() == 1);
    lighting_tick_1khz((uint16_t)1U << BUTTON_LEFT_A, 0);
    assert(count_on_cycles(BUTTON_LEFT_A) == 16);

    lighting_tick_1khz(0, (uint16_t)1U << BUTTON_TEST);
    lighting_tick_1khz(0, (uint16_t)1U << BUTTON_SERVICE);
    assert(lighting_mode() == 2);
    assert(count_on_cycles(BUTTON_LEFT_A) == 0);

    lighting_tick_1khz(0, (uint16_t)1U << BUTTON_TEST);
    lighting_tick_1khz(0, (uint16_t)1U << BUTTON_SERVICE);
    assert(lighting_mode() == 0);

    uint8_t host_levels[12] = {0};
    host_levels[BUTTON_LEFT_B] = 255;
    lighting_set_host_levels(host_levels);
    lighting_tick_1khz(0, 0);
    assert(count_on_cycles(BUTTON_LEFT_A) == 0);
    assert(count_on_cycles(BUTTON_LEFT_B) == 64);
    /* Direct WebUI control has no Test/Service lamps and must not switch off
       the local output behavior associated with those controls. */
    assert(count_on_cycles(BUTTON_TEST) == 20);
    assert(count_on_cycles(BUTTON_SERVICE) == 20);

    host_levels[BUTTON_WAD_LEFT] = 255;
    host_levels[BUTTON_WAD_RIGHT] = 128;
    lighting_set_host_levels(host_levels);
    lighting_tick_1khz(0, 0);
    /* Allow the next WAD cycle boundary to latch the new state. */
    for (unsigned i = 0; i < 8; ++i) lighting_pwm_tick();
    assert(count_on_cycles(BUTTON_WAD_LEFT) == 24);
    assert(count_on_cycles(BUTTON_WAD_RIGHT) == 24);

    lighting_tick_1khz((uint16_t)((1U << BUTTON_WAD_LEFT) |
                                  (1U << BUTTON_WAD_RIGHT)), 0);
    for (unsigned i = 0; i < 8; ++i) lighting_pwm_tick();
    assert(count_on_cycles(BUTTON_WAD_LEFT) == 64);
    assert(count_on_cycles(BUTTON_WAD_RIGHT) == 64);

    for (unsigned i = 0; i < 251; ++i) {
        lighting_tick_1khz(0, 0);
    }
    assert(count_on_cycles(BUTTON_LEFT_A) == 0);

    lighting_clear_host_levels();
    lighting_tick_1khz(0, 0);
    (void)count_on_cycles(BUTTON_LEFT_A);
    assert(count_on_cycles(BUTTON_LEFT_A) == 20);
    lighting_apply_settings(0, 40, 0, 0, 0, 0);
    lighting_tick_1khz(0, 0);
    (void)count_on_cycles(BUTTON_LEFT_A);
    assert(count_on_cycles(BUTTON_LEFT_A) == 10);

    /* Host values cannot alter WAD output. */
    lighting_init();
    host_levels[BUTTON_WAD_LEFT] = 255;
    host_levels[BUTTON_WAD_RIGHT] = 255;
    lighting_set_host_levels(host_levels);
    lighting_tick_1khz(0, 0);
    for (unsigned i = 0; i < 8; ++i) lighting_pwm_tick();
    assert(count_on_cycles(BUTTON_WAD_LEFT) == 24);
    assert(count_on_cycles(BUTTON_WAD_RIGHT) == 24);

    puts("lighting tests passed");
    return 0;
}
