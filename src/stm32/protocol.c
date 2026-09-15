#include "board.h"
#include "config.h"
#include "lighting.h"
#include "protocol.h"
#include "strip.h"

#define MAX_PAYLOAD_LENGTH 32U

enum parser_state {
    WAIT_HEADER_AA = 0,
    WAIT_HEADER_55,
    WAIT_LENGTH,
    WAIT_PAYLOAD,
    WAIT_CHECKSUM
};

static enum parser_state state;
static uint8_t payload[MAX_PAYLOAD_LENGTH];
static uint8_t payload_length;
static uint8_t payload_index;
static uint8_t checksum;
static uint16_t valid_frames;
static uint16_t invalid_frames;
static uint8_t save_requested;
static uint8_t save_token;
static uint8_t bootloader_requested;
static uint8_t input_sequence;

static void increment_saturating(uint16_t *value)
{
    if (*value != UINT16_MAX) {
        ++*value;
    }
}

static void send_strip_ack(void)
{
    uint8_t frame[13];
    uint8_t sum = 0;
    frame[0] = 0xAAU;
    frame[1] = 0x55U;
    frame[2] = 0x09U;
    frame[3] = 0x92U;
    for (uint8_t i = 0; i < 8U; ++i) frame[4U + i] = payload[1U + i];
    for (uint8_t i = 3U; i < 12U; ++i) sum ^= frame[i];
    frame[12] = sum;
    hardware_uart_write(frame, sizeof(frame));
}

static void dispatch_payload(void)
{
    if (payload_length == 0U) {
        return;
    }

    switch (payload[0]) {
    case 0x01U:
        if (payload_length == 7U) {
            config_set_lighting(payload[1], payload[2], payload[3],
                                payload[4], payload[5], payload[6]);
            lighting_apply_settings(payload[1], payload[2], payload[3],
                                    payload[4], payload[5], payload[6]);
        }
        break;
    case 0x03U:
        if (payload_length == 1U || payload_length == 2U) {
            save_token = payload_length == 2U ? payload[1] : 0U;
            save_requested = 1U;
        }
        break;
    case 0x14U:
        /* WebUI direct control is deliberately separate from MU3 layout. */
        if (payload_length == 13U) {
            lighting_set_host_levels(&payload[1]);
        }
        break;
    case 0x11U:
        /* Yuan'tGeki extension: return control to local/reactive lighting. */
        if (payload_length == 1U) {
            lighting_clear_host_levels();
        }
        break;
    case 0x12U:
        if (payload_length == 9U || payload_length == 11U) {
            const uint8_t value_offset = payload_length == 11U ? 2U : 1U;
            config_set_strip(payload[value_offset], payload[value_offset + 1U],
                             payload[value_offset + 2U], payload[value_offset + 3U],
                             payload[value_offset + 4U], payload[value_offset + 5U],
                             payload[value_offset + 6U], payload[value_offset + 7U]);
            strip_apply_settings(payload[value_offset], payload[value_offset + 1U],
                                 payload[value_offset + 2U], payload[value_offset + 3U],
                                 payload[value_offset + 4U], payload[value_offset + 5U],
                                 payload[value_offset + 6U], payload[value_offset + 7U]);
            if (payload_length == 11U && (payload[10] & 0x01U) != 0U) {
                save_token = payload[1];
                save_requested = 1U;
            } else {
                send_strip_ack();
            }
        }
        break;
    case 0x20U:
        if (payload_length == 5U && payload[1] == 'B' && payload[2] == 'O' &&
            payload[3] == 'O' && payload[4] == 'T') {
            bootloader_requested = 1U;
        }
        break;
    default:
        break;
    }
}

static void consume_byte(uint8_t byte)
{
    switch (state) {
    case WAIT_HEADER_AA:
        if (byte == 0xAAU) {
            state = WAIT_HEADER_55;
        }
        break;
    case WAIT_HEADER_55:
        if (byte == 0x55U) {
            state = WAIT_LENGTH;
        } else if (byte != 0xAAU) {
            state = WAIT_HEADER_AA;
        }
        break;
    case WAIT_LENGTH:
        if (byte == 0U || byte > MAX_PAYLOAD_LENGTH) {
            increment_saturating(&invalid_frames);
            state = (byte == 0xAAU) ? WAIT_HEADER_55 : WAIT_HEADER_AA;
        } else {
            payload_length = byte;
            payload_index = 0;
            checksum = 0;
            state = WAIT_PAYLOAD;
        }
        break;
    case WAIT_PAYLOAD:
        payload[payload_index++] = byte;
        checksum ^= byte;
        if (payload_index == payload_length) {
            state = WAIT_CHECKSUM;
        }
        break;
    case WAIT_CHECKSUM:
        if (byte == checksum) {
            increment_saturating(&valid_frames);
            dispatch_payload();
            state = WAIT_HEADER_AA;
        } else {
            increment_saturating(&invalid_frames);
            state = (byte == 0xAAU) ? WAIT_HEADER_55 : WAIT_HEADER_AA;
        }
        break;
    default:
        state = WAIT_HEADER_AA;
        break;
    }
}

#ifdef YTG_UNIT_TEST
void protocol_feed_byte_for_test(uint8_t byte)
{
    consume_byte(byte);
}
#endif

void protocol_init(void)
{
    state = WAIT_HEADER_AA;
    payload_length = 0;
    payload_index = 0;
    checksum = 0;
    valid_frames = 0;
    invalid_frames = 0;
    save_requested = 0;
    save_token = 0;
    bootloader_requested = 0;
    input_sequence = 0;
}

void protocol_poll(void)
{
    int16_t byte;
    while ((byte = hardware_uart_read()) >= 0) {
        consume_byte((uint8_t)byte);
    }
}

void protocol_send_input(uint16_t buttons, uint8_t raw_lever, uint8_t filtered_lever)
{
    uint8_t frame[10];
    frame[0] = 0xAAU;
    frame[1] = 0x55U;
    frame[2] = 0x06U;
    frame[3] = 0xA1U;       /* Typed input report marker. */
    frame[4] = ++input_sequence;
    frame[5] = (uint8_t)buttons;
    frame[6] = (uint8_t)(buttons >> 8);
    frame[7] = raw_lever;
    frame[8] = filtered_lever;
    frame[9] = frame[3] ^ frame[4] ^ frame[5] ^
               frame[6] ^ frame[7] ^ frame[8];
    hardware_uart_write(frame, sizeof(frame));
}

void protocol_send_save_ack(uint8_t token, uint8_t success)
{
    uint8_t frame[7];
    frame[0] = 0xAAU;
    frame[1] = 0x55U;
    frame[2] = 0x03U;
    frame[3] = 0x93U;
    frame[4] = token;
    frame[5] = success != 0U ? 1U : 0U;
    frame[6] = frame[3] ^ frame[4] ^ frame[5];
    hardware_uart_write(frame, sizeof(frame));
}

uint16_t protocol_valid_frames(void)
{
    return valid_frames;
}

uint16_t protocol_invalid_frames(void)
{
    return invalid_frames;
}

uint8_t protocol_take_save_request(void)
{
    const uint8_t result = save_requested;
    save_requested = 0;
    return result;
}

uint8_t protocol_save_token(void)
{
    return save_token;
}

uint8_t protocol_take_bootloader_request(void)
{
    const uint8_t result = bootloader_requested;
    bootloader_requested = 0;
    return result;
}
