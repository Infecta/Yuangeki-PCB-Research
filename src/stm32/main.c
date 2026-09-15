#include <stdint.h>

#include "board.h"
#include "config.h"
#include "input.h"
#include "lever.h"
#include "lighting.h"
#include "mcu.h"
#include "protocol.h"
#include "strip.h"

static volatile uint8_t work_pending;
static uint8_t tick_divider;
static uint8_t sensor_drive_phase;

void SysTick_Handler(void)
{
    lighting_pwm_tick();
    ++sensor_drive_phase;
    if (sensor_drive_phase == 10U) {
        sensor_drive_phase = 0;
    }
    hardware_write_sensor_drive(sensor_drive_phase >= 5U ? 1U : 0U);
    ++tick_divider;
    if (tick_divider == 8U) {
        tick_divider = 0;
        input_tick_1khz();
        work_pending = 1;
    }
}

int main(void)
{
    irq_disable();
    hardware_init();
    input_init();
    lighting_init();
    strip_init();
    config_init();
    protocol_init();
    lever_init(hardware_read_adc());
    work_pending = 0;
    tick_divider = 0;
    sensor_drive_phase = 0;
    irq_enable();

    uint8_t report_divider = 0;
    for (;;) {
        protocol_poll();
        if (work_pending == 0U) {
            wait_for_interrupt();
            continue;
        }

        irq_disable();
        work_pending = 0;
        const uint16_t buttons = input_buttons();
        const uint16_t edges = input_pressed_edges();
        irq_enable();

        lever_update(hardware_read_adc());
        lighting_tick_1khz(buttons, edges);
        strip_tick_1khz();

        if (protocol_take_bootloader_request() != 0U) {
            hardware_request_rom_bootloader();
        }

        if (lighting_take_mode_changed() != 0U) {
            config_set_lighting_mode(lighting_mode());
            (void)config_save();
        }
        if (protocol_take_save_request() != 0U) {
            const uint8_t token = protocol_save_token();
            const uint8_t success = config_save();
            protocol_send_save_ack(token, success);
        }

        ++report_divider;
        if (report_divider == 4U) {
            report_divider = 0;
            protocol_send_input(buttons, lever_raw_legacy(), lever_filtered_legacy());
        }
    }
}
