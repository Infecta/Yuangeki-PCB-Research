#include "bootloader.h"
#include "bridge.h"
#include "ch552.h"
#include "uart.h"
#include "updater.h"
#include "usb.h"

#define OPTION_CONFIRM_FRAMES 3U

enum parser_state {
    WAIT_AA = 0,
    WAIT_55,
    WAIT_LENGTH,
    WAIT_PAYLOAD,
    WAIT_CHECKSUM
};

static __xdata uint8_t input_report[HID_INPUT_REPORT_SIZE];
static __xdata uint8_t mu3_input_report[MU3_INPUT_REPORT_SIZE];
static __xdata uint8_t lighting_payload[13];
static __xdata uint8_t option_candidate;
static __xdata uint8_t option_confirmed;
static __xdata uint8_t option_confirm_count;
static __data uint8_t parser_state;
static __data uint8_t payload_length;
static __data uint8_t payload_index;
static __data uint8_t checksum;
static __xdata uint8_t payload[16];
static __data uint8_t report_sequence;
static volatile __bit report_dirty;
static volatile __bit mu3_report_dirty;
static volatile __bit mu3_session_release_pending;
static __data uint16_t valid_frames;
static __data uint16_t invalid_frames;

static uint8_t send_stm32_frame(const uint8_t *payload_data, uint8_t length);

static void counter_increment(uint16_t *counter)
{
    if (*counter != UINT16_MAX) {
        ++*counter;
    }
}

static void update_diagnostics(void)
{
    uint16_t value;
    value = valid_frames;
    input_report[8] = (uint8_t)value;
    input_report[9] = (uint8_t)(value >> 8);
    value = invalid_frames;
    input_report[10] = (uint8_t)value;
    input_report[11] = (uint8_t)(value >> 8);
    value = uart_rx_overflows();
    input_report[12] = (uint8_t)value;
    input_report[13] = (uint8_t)(value >> 8);
    value = uart_tx_overflows();
    input_report[14] = (uint8_t)value;
    input_report[15] = (uint8_t)(value >> 8);
}

static void accept_input_frame(void)
{
    uint8_t i;
    if (payload_length == 3U && payload[0] == 0x93U) {
        for (i = 0; i < HID_INPUT_REPORT_SIZE; ++i) input_report[i] = 0;
        input_report[0] = 4U;
        input_report[1] = payload[1];
        input_report[2] = payload[2];
        update_diagnostics();
        report_dirty = 1;
        return;
    }
    if (payload_length == 9U && payload[0] == 0x92U) {
        for (i = 0; i < HID_INPUT_REPORT_SIZE; ++i) input_report[i] = 0;
        input_report[0] = 3U;
        for (i = 0; i < 8U; ++i) input_report[i + 1U] = payload[i + 1U];
        update_diagnostics();
        report_dirty = 1;
        return;
    }
    if (payload_length != 6U || payload[0] != 0xA1U) {
        return;
    }
    /* Preserve a strip acknowledgement until USB has delivered it. */
    if (report_dirty && (input_report[0] == 3U || input_report[0] == 4U)) return;
    report_sequence = payload[1];
    input_report[0] = 1U;
    input_report[1] = report_sequence;
    input_report[2] = payload[2];
    input_report[3] = payload[3];
    input_report[4] = payload[4];
    input_report[5] = payload[5];
    input_report[6] = 1U; /* STM32 link valid. */
    input_report[7] = 0U;
    update_diagnostics();
    report_dirty = 1;

    /* MU3IO.NET Ontroller packet: "DDT", button maps, 16-bit big-endian
       lever value in its default calibrated range (100..600), reserved. */
    mu3_input_report[0] = 0x44U;
    mu3_input_report[1] = 0x44U;
    mu3_input_report[2] = 0x54U;
    mu3_input_report[3] = 0U;
    if (payload[2] & (1U << 0)) mu3_input_report[3] |= 0x20U;
    if (payload[2] & (1U << 1)) mu3_input_report[3] |= 0x10U;
    if (payload[2] & (1U << 2)) mu3_input_report[3] |= 0x08U;
    if (payload[2] & (1U << 3)) mu3_input_report[3] |= 0x04U;
    if (payload[2] & (1U << 4)) mu3_input_report[3] |= 0x02U;
    if (payload[2] & (1U << 5)) mu3_input_report[3] |= 0x01U;

    /* Test and Service are safety-sensitive. Require three consecutive STM32
       reports before publishing either transition to MU3IO.NET. */
    i = payload[3] & 0x0CU;
    if (i != option_candidate) {
        option_candidate = i;
        option_confirm_count = 1U;
    } else if (option_confirm_count < OPTION_CONFIRM_FRAMES) {
        ++option_confirm_count;
    }
    if (option_confirm_count >= OPTION_CONFIRM_FRAMES) {
        option_confirmed = option_candidate;
    }

    mu3_input_report[4] = 0U;
    if (payload[3] & (1U << 0)) mu3_input_report[4] |= 0x80U;
    if (payload[3] & (1U << 1)) mu3_input_report[4] |= 0x40U;
    if (payload[2] & (1U << 6)) mu3_input_report[4] |= 0x20U;
    if (payload[2] & (1U << 7)) mu3_input_report[4] |= 0x10U;
    if (option_confirmed & (1U << 2)) mu3_input_report[4] |= 0x08U;
    if (option_confirmed & (1U << 3)) mu3_input_report[4] |= 0x04U;
    {
        uint16_t lever = (uint16_t)(100UL +
            ((uint32_t)payload[5] * 500UL + 127UL) / 255UL);
        mu3_input_report[5] = (uint8_t)(lever >> 8);
        mu3_input_report[6] = (uint8_t)lever;
    }
    /* Reserved by Ontroller; expose the unfiltered STM32 high button byte to
       the diagnostic monitor without affecting standard MU3IO.NET. */
    mu3_input_report[7] = payload[3];
    mu3_report_dirty = 1;
}

static void consume_uart(uint8_t byte)
{
    switch (parser_state) {
    case WAIT_AA:
        if (byte == 0xAAU) parser_state = WAIT_55;
        break;
    case WAIT_55:
        if (byte == 0x55U) parser_state = WAIT_LENGTH;
        else if (byte != 0xAAU) parser_state = WAIT_AA;
        break;
    case WAIT_LENGTH:
        if (byte == 0U || byte > sizeof(payload)) {
            counter_increment(&invalid_frames);
            parser_state = byte == 0xAAU ? WAIT_55 : WAIT_AA;
        } else {
            payload_length = byte;
            payload_index = 0;
            checksum = 0;
            parser_state = WAIT_PAYLOAD;
        }
        break;
    case WAIT_PAYLOAD:
        payload[payload_index++] = byte;
        checksum ^= byte;
        if (payload_index == payload_length) parser_state = WAIT_CHECKSUM;
        break;
    case WAIT_CHECKSUM:
        if (byte == checksum) {
            /* Only twelve button bits exist.  Reject impossible input values
               even if a damaged UART stream happens to pass its XOR. */
            if (payload_length == 6U && payload[0] == 0xA1U &&
                (payload[3] & 0xF0U)) {
                counter_increment(&invalid_frames);
            } else {
                counter_increment(&valid_frames);
                accept_input_frame();
            }
            parser_state = WAIT_AA;
        } else {
            counter_increment(&invalid_frames);
            parser_state = byte == 0xAAU ? WAIT_55 : WAIT_AA;
        }
        break;
    default:
        parser_state = WAIT_AA;
        break;
    }
}

static uint8_t send_stm32_frame(const uint8_t *payload_data, uint8_t length)
{
    uint8_t frame[18];
    uint8_t i;
    uint8_t sum = 0;
    if (length > 14U) return 0;
    frame[0] = 0xAAU;
    frame[1] = 0x55U;
    frame[2] = length;
    for (i = 0; i < length; ++i) {
        frame[3U + i] = payload_data[i];
        sum ^= payload_data[i];
    }
    frame[3U + length] = sum;
    return uart_write(frame, (uint8_t)(length + 4U));
}

static void flush_mu3_session_release(void)
{
    static const uint8_t release_payload[1] = {0x11U};
    if (mu3_session_release_pending &&
        send_stm32_frame(release_payload, sizeof(release_payload))) {
        mu3_session_release_pending = 0;
    }
}

void bridge_init(void)
{
    uint8_t i;
    parser_state = WAIT_AA;
    payload_length = 0;
    payload_index = 0;
    checksum = 0;
    report_sequence = 0;
    valid_frames = 0;
    invalid_frames = 0;
    report_dirty = 1;
    mu3_report_dirty = 1;
    mu3_session_release_pending = 0;
    option_candidate = 0;
    option_confirmed = 0;
    option_confirm_count = 0;
    for (i = 0; i < HID_INPUT_REPORT_SIZE; ++i) input_report[i] = 0;
    input_report[0] = 1U;
    for (i = 0; i < MU3_INPUT_REPORT_SIZE; ++i) mu3_input_report[i] = 0;
    mu3_input_report[0] = 0x44U;
    mu3_input_report[1] = 0x44U;
    mu3_input_report[2] = 0x54U;
    mu3_input_report[5] = 0x01U;
    mu3_input_report[6] = 0x5EU; /* Center of default 100..600 range: 350. */
}

void bridge_poll(void)
{
    int16_t byte;
    updater_poll();
    if (updater_take_status_changed()) {
        uint32_t address = updater_status_address();
        uint8_t i;
        for (i = 0; i < HID_INPUT_REPORT_SIZE; ++i) input_report[i] = 0;
        input_report[0] = 2U;
        input_report[1] = updater_status_token();
        input_report[2] = updater_status_state();
        input_report[3] = updater_status_error();
        input_report[4] = (uint8_t)address;
        input_report[5] = (uint8_t)(address >> 8);
        input_report[6] = (uint8_t)(address >> 16);
        input_report[7] = (uint8_t)(address >> 24);
        report_dirty = 1;
    }
    if (updater_active()) return;
    while ((byte = uart_read()) >= 0) {
        consume_uart((uint8_t)byte);
    }
    flush_mu3_session_release();
}

void bridge_handle_output(const uint8_t *report, uint8_t length)
{
    uint8_t i;
    if (length == 0U) return;
    if (report[0] == 0x30U) {
        if (length >= 6U && report[1] == 'Y' && report[2] == 'T' &&
            report[3] == 'G' && report[4] == 'C' && report[5] == '!') {
            ch552_bootloader_request();
        }
        return;
    }
    if (updater_handle_output(report, length)) return;
    if (updater_active()) return;

    switch (report[0]) {
    case 0x01U:
        if (length >= 13U) {
            lighting_payload[0] = 0x14U;
            for (i = 0; i < 12U; ++i) lighting_payload[i + 1U] = report[i + 1U];
            send_stm32_frame(lighting_payload, 13U);
        }
        break;
    case 0x02U:
        if (length >= 7U) {
            lighting_payload[0] = 0x01U;
            for (i = 0; i < 6U; ++i) lighting_payload[i + 1U] = report[i + 1U];
            send_stm32_frame(lighting_payload, 7U);
            if (length >= 8U && (report[7] & 0x01U)) {
                lighting_payload[0] = 0x03U;
                lighting_payload[1] = 0U;
                send_stm32_frame(lighting_payload, 1U);
            }
        }
        break;
    case 0x03U:
        lighting_payload[0] = 0x11U;
        send_stm32_frame(lighting_payload, 1U);
        break;
    case 0x04U:
        lighting_payload[0] = 0x03U;
        lighting_payload[1] = length >= 2U ? report[1] : 0U;
        send_stm32_frame(lighting_payload, 2U);
        break;
    case 0x05U:
        if (length >= 9U) {
            lighting_payload[0] = 0x12U;
            if (length >= 10U && (report[9] & 0x01U) != 0U) {
                lighting_payload[1] = report[30];
                for (i = 0; i < 8U; ++i) lighting_payload[i + 2U] = report[i + 1U];
                lighting_payload[10] = report[9];
                send_stm32_frame(lighting_payload, 11U);
            } else {
                for (i = 0; i < 8U; ++i) lighting_payload[i + 1U] = report[i + 1U];
                send_stm32_frame(lighting_payload, 9U);
            }
        }
        break;
    default:
        break;
    }
}

void bridge_handle_mu3_output(const uint8_t *report, uint8_t length)
{
    /* MU3IO.NET lighting is deliberately ignored. The USB endpoint still
       acknowledges reports so input compatibility is unaffected. */
    (void)report;
    (void)length;
}

void bridge_begin_mu3_session(void)
{
    /* Ensure the STM32 uses its configured local/reactive lighting. */
    mu3_session_release_pending = 1;
}

const uint8_t *bridge_input_report(void)
{
    return input_report;
}

const uint8_t *bridge_mu3_input_report(void)
{
    return mu3_input_report;
}

uint8_t bridge_input_changed(void)
{
    return report_dirty ? 1U : 0U;
}

void bridge_input_sent(void)
{
    report_dirty = 0;
}

uint8_t bridge_mu3_input_changed(void)
{
    return mu3_report_dirty ? 1U : 0U;
}

void bridge_mu3_input_sent(void)
{
    mu3_report_dirty = 0;
}
