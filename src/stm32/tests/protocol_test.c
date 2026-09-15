#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "protocol.h"

static uint8_t settings[6];
static uint8_t received_levels[12];
static unsigned settings_calls;
static unsigned levels_calls;
static unsigned clear_calls;
static unsigned strip_calls;
static uint8_t strip_settings[8];
static uint8_t last_uart_frame[16];
static uint8_t last_uart_length;

void config_set_lighting(uint8_t effect, uint8_t value, uint8_t speed,
                         uint8_t reactive_mode, uint8_t off_level,
                         uint8_t on_level)
{
    (void)effect;
    (void)value;
    (void)speed;
    (void)reactive_mode;
    (void)off_level;
    (void)on_level;
}

void config_set_strip(uint8_t effect, uint8_t brightness, uint8_t speed,
                      uint8_t red, uint8_t green, uint8_t blue,
                      uint8_t color_order, uint8_t pixel_count)
{
    (void)effect; (void)brightness; (void)speed;
    (void)red; (void)green; (void)blue;
    (void)color_order; (void)pixel_count;
}

void strip_apply_settings(uint8_t effect, uint8_t brightness, uint8_t speed,
                          uint8_t red, uint8_t green, uint8_t blue,
                          uint8_t color_order, uint8_t pixel_count)
{
    strip_settings[0] = effect;
    strip_settings[1] = brightness;
    strip_settings[2] = speed;
    strip_settings[3] = red;
    strip_settings[4] = green;
    strip_settings[5] = blue;
    strip_settings[6] = color_order;
    strip_settings[7] = pixel_count;
    ++strip_calls;
}

int16_t hardware_uart_read(void)
{
    return -1;
}

void hardware_uart_write(const uint8_t *data, uint8_t length)
{
    assert(length <= sizeof(last_uart_frame));
    memcpy(last_uart_frame, data, length);
    last_uart_length = length;
}

void lighting_apply_settings(uint8_t effect, uint8_t value, uint8_t speed,
                             uint8_t reactive_mode, uint8_t off_level,
                             uint8_t on_level)
{
    settings[0] = effect;
    settings[1] = value;
    settings[2] = speed;
    settings[3] = reactive_mode;
    settings[4] = off_level;
    settings[5] = on_level;
    ++settings_calls;
}

void lighting_set_host_levels(const uint8_t levels[12])
{
    memcpy(received_levels, levels, sizeof(received_levels));
    ++levels_calls;
}

void lighting_clear_host_levels(void)
{
    ++clear_calls;
}

static void feed_frame(const uint8_t *payload, uint8_t length, uint8_t corrupt)
{
    uint8_t checksum = 0;
    protocol_feed_byte_for_test(0xAA);
    protocol_feed_byte_for_test(0x55);
    protocol_feed_byte_for_test(length);
    for (uint8_t i = 0; i < length; ++i) {
        protocol_feed_byte_for_test(payload[i]);
        checksum ^= payload[i];
    }
    protocol_feed_byte_for_test(corrupt ? (uint8_t)(checksum ^ 1U) : checksum);
}

int main(void)
{
    protocol_init();

    const uint8_t settings_payload[] = {0x01, 2, 90, 4, 1, 20, 240};
    protocol_feed_byte_for_test(0x00);
    protocol_feed_byte_for_test(0xAA);
    protocol_feed_byte_for_test(0x12);
    feed_frame(settings_payload, sizeof(settings_payload), 0);
    assert(protocol_valid_frames() == 1);
    assert(protocol_invalid_frames() == 0);
    assert(settings_calls == 1);
    assert(memcmp(settings, &settings_payload[1], sizeof(settings)) == 0);

    feed_frame(settings_payload, sizeof(settings_payload), 1);
    assert(protocol_valid_frames() == 1);
    assert(protocol_invalid_frames() == 1);
    assert(settings_calls == 1);

    uint8_t levels_payload[13] = {0x14};
    for (uint8_t i = 0; i < 12; ++i) {
        levels_payload[i + 1] = (uint8_t)(i * 17U);
    }
    feed_frame(levels_payload, sizeof(levels_payload), 0);
    assert(levels_calls == 1);
    assert(memcmp(received_levels, &levels_payload[1], 12) == 0);

    const uint8_t clear_payload[] = {0x11};
    feed_frame(clear_payload, sizeof(clear_payload), 0);
    assert(clear_calls == 1);

    const uint8_t save_payload[] = {0x03, 0x5AU};
    feed_frame(save_payload, sizeof(save_payload), 0);
    assert(protocol_take_save_request() == 1);
    assert(protocol_save_token() == 0x5AU);
    assert(protocol_take_save_request() == 0);

    protocol_send_save_ack(0x5AU, 1U);
    assert(last_uart_length == 7U);
    assert(last_uart_frame[2] == 3U && last_uart_frame[3] == 0x93U);
    assert(last_uart_frame[4] == 0x5AU && last_uart_frame[5] == 1U);
    assert(last_uart_frame[6] == (uint8_t)(0x93U ^ 0x5AU ^ 1U));

    const uint8_t strip_payload[] = {0x12, 2, 96, 40, 10, 20, 30, 0, 16};
    feed_frame(strip_payload, sizeof(strip_payload), 0);
    assert(strip_calls == 1);
    assert(memcmp(strip_settings, &strip_payload[1], sizeof(strip_settings)) == 0);
    assert(last_uart_length == 13U);
    assert(last_uart_frame[2] == 9U && last_uart_frame[3] == 0x92U);
    assert(memcmp(&last_uart_frame[4], &strip_payload[1], 8U) == 0);

    const uint8_t strip_save_payload[] = {
        0x12, 0x6BU, 3, 77, 55, 11, 22, 33, 1, 24, 1
    };
    last_uart_length = 0U;
    feed_frame(strip_save_payload, sizeof(strip_save_payload), 0);
    assert(strip_calls == 2);
    assert(memcmp(strip_settings, &strip_save_payload[2], sizeof(strip_settings)) == 0);
    assert(protocol_take_save_request() == 1U);
    assert(protocol_save_token() == 0x6BU);
    assert(last_uart_length == 0U);

    const uint8_t boot_payload[] = {0x20, 'B', 'O', 'O', 'T'};
    feed_frame(boot_payload, sizeof(boot_payload), 0);
    assert(protocol_take_bootloader_request() == 1);
    assert(protocol_take_bootloader_request() == 0);

    protocol_feed_byte_for_test(0xAA);
    protocol_feed_byte_for_test(0x55);
    protocol_feed_byte_for_test(0xFF);
    assert(protocol_invalid_frames() == 2);

    protocol_send_input(0x0F00U, 0xFFU, 0xAAU);
    assert(last_uart_length == 10U);
    assert(last_uart_frame[0] == 0xAAU && last_uart_frame[1] == 0x55U);
    assert(last_uart_frame[2] == 6U && last_uart_frame[3] == 0xA1U);
    assert(last_uart_frame[4] == 1U);
    assert(last_uart_frame[5] == 0x00U && last_uart_frame[6] == 0x0FU);
    assert(last_uart_frame[7] == 0xFFU && last_uart_frame[8] == 0xAAU);
    assert(last_uart_frame[9] == (uint8_t)(0xA1U ^ 1U ^ 0x0FU ^ 0xFFU ^ 0xAAU));

    puts("protocol tests passed");
    return 0;
}
