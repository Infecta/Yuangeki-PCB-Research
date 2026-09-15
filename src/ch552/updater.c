#include "uart.h"
#include "updater.h"
#include "usb.h"

#define ACK 0x79U
#define NACK 0x1FU
#define APP_FIRST_ADDRESS 0x08000000UL
#define APP_LAST_ADDRESS  0x0800EFFFUL
#define MAX_WRITE_SIZE 24U

enum update_phase {
    PHASE_IDLE = 0,
    PHASE_ENTER_DELAY,
    PHASE_SYNC_ACK,
    PHASE_READY,
    PHASE_ERASE_COMMAND_ACK,
    PHASE_ERASE_RESULT_ACK,
    PHASE_WRITE_COMMAND_ACK,
    PHASE_WRITE_ADDRESS_ACK,
    PHASE_WRITE_DATA_ACK,
    PHASE_READ_COMMAND_ACK,
    PHASE_READ_ADDRESS_ACK,
    PHASE_READ_LENGTH_ACK,
    PHASE_READ_DATA,
    PHASE_GO_COMMAND_ACK,
    PHASE_GO_ADDRESS_ACK,
    PHASE_GO_DELAY,
    PHASE_ERROR
};

enum update_error {
    UPDATE_ERR_NONE = 0,
    UPDATE_ERR_BAD_REQUEST = 1,
    UPDATE_ERR_BUSY = 2,
    UPDATE_ERR_UART_TIMEOUT = 3,
    UPDATE_ERR_NACK = 4,
    UPDATE_ERR_VERIFY = 5,
    UPDATE_ERR_TX_OVERFLOW = 6,
    UPDATE_ERR_UNEXPECTED_RESPONSE = 7
};

static __xdata uint8_t phase;
static __xdata uint8_t public_state;
static __xdata uint8_t status_token;
static __xdata uint8_t status_error;
static volatile __bit status_changed;
static __xdata uint16_t deadline;
static __xdata uint32_t operation_address;
static __xdata uint16_t erase_page;
static __xdata uint8_t write_length;
static __xdata uint8_t read_index;
static __xdata uint8_t sync_retries;
static __xdata uint8_t rom_was_ready;
static __xdata uint8_t write_data[MAX_WRITE_SIZE];
static __xdata uint8_t tx_scratch[29];
static __code const uint8_t app_boot_frame[9] = {
    0xAAU, 0x55U, 0x05U, 0x20U, 'B', 'O', 'O', 'T', 0x36U
};

static uint8_t deadline_expired(void)
{
    return (int16_t)(usb_millis() - deadline) >= 0;
}

static void set_deadline(uint16_t milliseconds)
{
    deadline = (uint16_t)(usb_millis() + milliseconds);
}

static void publish(uint8_t state, uint8_t error)
{
    public_state = state;
    status_error = error;
    status_changed = 1;
}

static void fail(uint8_t error)
{
    phase = PHASE_ERROR;
    publish(UPDATE_ERROR, error);
}

static uint8_t send_bytes(const uint8_t *data, uint8_t length)
{
    if (!uart_write(data, length)) {
        fail(UPDATE_ERR_TX_OVERFLOW);
        return 0;
    }
    return 1;
}

static uint8_t send_command(uint8_t command)
{
    tx_scratch[0] = command;
    tx_scratch[1] = (uint8_t)~command;
    return send_bytes(tx_scratch, 2U);
}

static uint8_t send_address(uint32_t address)
{
    tx_scratch[0] = (uint8_t)(address >> 24);
    tx_scratch[1] = (uint8_t)(address >> 16);
    tx_scratch[2] = (uint8_t)(address >> 8);
    tx_scratch[3] = (uint8_t)address;
    tx_scratch[4] = tx_scratch[0] ^ tx_scratch[1] ^
                    tx_scratch[2] ^ tx_scratch[3];
    return send_bytes(tx_scratch, 5U);
}

static uint8_t accept_ack(int16_t value)
{
    if (value < 0) {
        if (deadline_expired()) fail(UPDATE_ERR_UART_TIMEOUT);
        return 0;
    }
    if ((uint8_t)value != ACK) {
        fail((uint8_t)value == NACK ? UPDATE_ERR_NACK : UPDATE_ERR_UNEXPECTED_RESPONSE);
        return 0;
    }
    return 1;
}

static void begin_sync(void)
{
    const uint8_t sync = 0x7FU;
    uart_init_bootloader();
    sync_retries = 0;
    if (!send_bytes(&sync, 1U)) return;
    phase = PHASE_SYNC_ACK;
    set_deadline(1000U);
}

static void begin_entry(void)
{
    rom_was_ready = 0;
    uart_init_application();
    if (!send_bytes(app_boot_frame, sizeof(app_boot_frame))) return;
    phase = PHASE_ENTER_DELAY;
    set_deadline(250U);
}

void updater_init(void)
{
    phase = PHASE_IDLE;
    public_state = UPDATE_IDLE;
    status_token = 0;
    status_error = UPDATE_ERR_NONE;
    status_changed = 0;
    operation_address = 0;
    rom_was_ready = 0;
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

uint8_t updater_handle_output(const uint8_t *report, uint8_t length)
{
    uint8_t i;
    if (length == 0U || report[0] < 0x20U || report[0] > 0x24U) return 0;
    if (length < 2U) return 1;
    status_token = report[1];

    if (report[0] == 0x20U) {
        if (length < 6U || report[2] != 'Y' || report[3] != 'T' ||
            report[4] != 'G' || report[5] != '!') {
            fail(UPDATE_ERR_BAD_REQUEST);
            return 1;
        }
        publish(UPDATE_ENTERING, UPDATE_ERR_NONE);
        begin_entry();
        return 1;
    }

    if (report[0] == 0x24U) {
        publish(UPDATE_ENTERING, UPDATE_ERR_NONE);
        /* Repeat application boot entry if synchronization never succeeded;
           otherwise keep the already-running ROM and resynchronize it. */
        if (rom_was_ready) begin_sync();
        else begin_entry();
        return 1;
    }

    if (phase != PHASE_READY || public_state != UPDATE_READY) {
        fail(UPDATE_ERR_BUSY);
        return 1;
    }
    if (report[0] == 0x21U) {
        if (length < 4U) { fail(UPDATE_ERR_BAD_REQUEST); return 1; }
        erase_page = ((uint16_t)report[2] << 8) | report[3];
        if (erase_page >= 30U) { fail(UPDATE_ERR_BAD_REQUEST); return 1; }
        publish(UPDATE_ERASING, UPDATE_ERR_NONE);
        if (!send_command(0x44U)) return 1;
        phase = PHASE_ERASE_COMMAND_ACK;
        set_deadline(1000U);
        return 1;
    }

    if (report[0] == 0x22U) {
        if (length < 7U) { fail(UPDATE_ERR_BAD_REQUEST); return 1; }
        operation_address = read_le32(&report[2]);
        write_length = report[6];
        if (write_length == 0U || write_length > MAX_WRITE_SIZE ||
            (write_length & 3U) != 0U || length < (uint8_t)(7U + write_length) ||
            (operation_address & 3UL) != 0UL ||
            operation_address < APP_FIRST_ADDRESS ||
            operation_address > APP_LAST_ADDRESS ||
            write_length > APP_LAST_ADDRESS - operation_address + 1UL) {
            fail(UPDATE_ERR_BAD_REQUEST);
            return 1;
        }
        for (i = 0; i < write_length; ++i) write_data[i] = report[7U + i];
        publish(UPDATE_WRITING, UPDATE_ERR_NONE);
        if (!send_command(0x31U)) return 1;
        phase = PHASE_WRITE_COMMAND_ACK;
        set_deadline(1000U);
        return 1;
    }

    if (report[0] == 0x23U) {
        if (length < 6U) { fail(UPDATE_ERR_BAD_REQUEST); return 1; }
        operation_address = read_le32(&report[2]);
        if (operation_address < APP_FIRST_ADDRESS ||
            operation_address > APP_LAST_ADDRESS) {
            fail(UPDATE_ERR_BAD_REQUEST);
            return 1;
        }
        publish(UPDATE_STARTING, UPDATE_ERR_NONE);
        if (!send_command(0x21U)) return 1;
        phase = PHASE_GO_COMMAND_ACK;
        set_deadline(1000U);
        return 1;
    }

    return 1;
}

void updater_poll(void)
{
    int16_t value;
    uint8_t checksum;
    uint8_t i;

    if (phase == PHASE_IDLE || phase == PHASE_READY || phase == PHASE_ERROR) return;
    if (phase == PHASE_ENTER_DELAY) {
        if (deadline_expired()) begin_sync();
        return;
    }
    if (phase == PHASE_GO_DELAY) {
        if (deadline_expired()) {
            uart_init_application();
            phase = PHASE_IDLE;
        }
        return;
    }

    value = uart_read();
    if (phase == PHASE_SYNC_ACK) {
        if (value < 0) {
            if (deadline_expired()) {
                if (sync_retries < 2U) {
                    const uint8_t sync = 0x7FU;
                    ++sync_retries;
                    uart_clear();
                    if (!send_bytes(&sync, 1U)) return;
                    set_deadline(1000U);
                } else {
                    fail(UPDATE_ERR_UART_TIMEOUT);
                }
            }
            return;
        }
        /* Ignore bytes left from the application UART while the ROM takes
           ownership. NACK remains meaningful; ACK completes autobaud. */
        if ((uint8_t)value != ACK) {
            if ((uint8_t)value == NACK) fail(UPDATE_ERR_NACK);
            return;
        }
        phase = PHASE_READY;
        rom_was_ready = 1;
        publish(UPDATE_READY, UPDATE_ERR_NONE);
        return;
    }
    if (phase == PHASE_READ_DATA) {
        if (value < 0) {
            if (deadline_expired()) fail(UPDATE_ERR_UART_TIMEOUT);
            return;
        }
        if ((uint8_t)value != write_data[read_index++]) {
            fail(UPDATE_ERR_VERIFY);
            return;
        }
        if (read_index == write_length) {
            phase = PHASE_READY;
            publish(UPDATE_READY, UPDATE_ERR_NONE);
        }
        return;
    }
    if (!accept_ack(value)) return;

    switch (phase) {
    case PHASE_ERASE_COMMAND_ACK:
        tx_scratch[0] = 0;
        tx_scratch[1] = 0;
        tx_scratch[2] = (uint8_t)(erase_page >> 8);
        tx_scratch[3] = (uint8_t)erase_page;
        tx_scratch[4] = tx_scratch[0] ^ tx_scratch[1] ^
                        tx_scratch[2] ^ tx_scratch[3];
        if (!send_bytes(tx_scratch, 5U)) return;
        phase = PHASE_ERASE_RESULT_ACK;
        set_deadline(5000U);
        break;
    case PHASE_ERASE_RESULT_ACK:
        phase = PHASE_READY;
        publish(UPDATE_READY, UPDATE_ERR_NONE);
        break;
    case PHASE_WRITE_COMMAND_ACK:
        if (!send_address(operation_address)) return;
        phase = PHASE_WRITE_ADDRESS_ACK;
        set_deadline(1000U);
        break;
    case PHASE_WRITE_ADDRESS_ACK:
        tx_scratch[0] = (uint8_t)(write_length - 1U);
        checksum = tx_scratch[0];
        for (i = 0; i < write_length; ++i) {
            tx_scratch[i + 1U] = write_data[i];
            checksum ^= write_data[i];
        }
        tx_scratch[write_length + 1U] = checksum;
        if (!send_bytes(tx_scratch, (uint8_t)(write_length + 2U))) return;
        phase = PHASE_WRITE_DATA_ACK;
        set_deadline(1000U);
        break;
    case PHASE_WRITE_DATA_ACK:
        publish(UPDATE_VERIFYING, UPDATE_ERR_NONE);
        if (!send_command(0x11U)) return;
        phase = PHASE_READ_COMMAND_ACK;
        set_deadline(1000U);
        break;
    case PHASE_READ_COMMAND_ACK:
        if (!send_address(operation_address)) return;
        phase = PHASE_READ_ADDRESS_ACK;
        set_deadline(1000U);
        break;
    case PHASE_READ_ADDRESS_ACK:
        tx_scratch[0] = (uint8_t)(write_length - 1U);
        tx_scratch[1] = (uint8_t)~tx_scratch[0];
        if (!send_bytes(tx_scratch, 2U)) return;
        phase = PHASE_READ_LENGTH_ACK;
        set_deadline(1000U);
        break;
    case PHASE_READ_LENGTH_ACK:
        read_index = 0;
        phase = PHASE_READ_DATA;
        set_deadline(1000U);
        break;
    case PHASE_GO_COMMAND_ACK:
        if (!send_address(operation_address)) return;
        phase = PHASE_GO_ADDRESS_ACK;
        set_deadline(1000U);
        break;
    case PHASE_GO_ADDRESS_ACK:
        publish(UPDATE_COMPLETE, UPDATE_ERR_NONE);
        phase = PHASE_GO_DELAY;
        set_deadline(250U);
        break;
    default:
        fail(UPDATE_ERR_UNEXPECTED_RESPONSE);
        break;
    }
}

uint8_t updater_active(void) { return phase != PHASE_IDLE; }
uint8_t updater_take_status_changed(void)
{
    uint8_t result = status_changed ? 1U : 0U;
    status_changed = 0;
    return result;
}
uint8_t updater_status_state(void) { return public_state; }
uint8_t updater_status_token(void) { return status_token; }
uint8_t updater_status_error(void) { return status_error; }
uint32_t updater_status_address(void) { return operation_address; }
