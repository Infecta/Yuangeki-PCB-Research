#include "ch552.h"
#include "uart.h"

#define RX_SIZE 32U
#define TX_SIZE 64U
#define RX_MASK (RX_SIZE - 1U)
#define TX_MASK (TX_SIZE - 1U)

static __xdata uint8_t rx_buffer[RX_SIZE];
static __xdata uint8_t tx_buffer[TX_SIZE];
static volatile __data uint8_t rx_head;
static volatile __data uint8_t rx_tail;
static volatile __data uint8_t tx_head;
static volatile __data uint8_t tx_tail;
static volatile __bit tx_active;
static volatile __data uint16_t rx_overflow_count;
static volatile __data uint16_t tx_overflow_count;
static volatile __bit bootloader_mode;

static uint8_t even_parity_bit(uint8_t value)
{
    value ^= value >> 4;
    value ^= value >> 2;
    value ^= value >> 1;
    return value & 1U;
}

void uart_clear(void)
{
    uint8_t saved_ea = EA;
    EA = 0;
    rx_head = rx_tail = 0;
    tx_head = tx_tail = 0;
    tx_active = 0;
    RI = 0;
    TI = 0;
    EA = saved_ea;
}

void uart_init_application(void)
{
    ES = 0;
    T2CON &= (uint8_t)~bTR2;
    uart_clear();
    bootloader_mode = 0;
    RCAP2H = 0xFFU;
    RCAP2L = 0xFDU;
    TH2 = 0xFFU;
    TL2 = 0xFDU;
    SCON = 0x50U;
    T2CON |= bTR2;
    ES = 1;
}

void uart_init_bootloader(void)
{
    ES = 0;
    T2CON &= (uint8_t)~bTR2;
    uart_clear();
    bootloader_mode = 1;
    /* 12 MHz / 16 / 13 = 57692 baud, 0.16% above 57600. Mode 3 carries
       eight data bits plus an explicitly generated even-parity ninth bit. */
    RCAP2H = 0xFFU;
    RCAP2L = 0xF3U;
    TH2 = 0xFFU;
    TL2 = 0xF3U;
    SCON = 0xD0U;
    T2CON |= bTR2;
    ES = 1;
}

void uart_init(void)
{
    rx_head = 0;
    rx_tail = 0;
    tx_head = 0;
    tx_tail = 0;
    tx_active = 0;
    rx_overflow_count = 0;
    tx_overflow_count = 0;

    PIN_FUNC |= bUART0_PIN_X;
    T2CON = bRCLK | bTCLK;
    T2MOD = bTMR_CLK | bT2_CLK;
    /* LED OUT packets arrive in the USB ISR.  Let UART RX pre-empt that ISR
       so a 250 kbaud byte is never lost while USB work is in progress. */
    IP |= 0x10U;
    uart_init_application();
}

int16_t uart_read(void)
{
    uint8_t value;
    if (rx_tail == rx_head) {
        return -1;
    }
    value = rx_buffer[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1U) & RX_MASK);
    return value;
}

uint8_t uart_write(const uint8_t *data, uint8_t length)
{
    uint8_t i;
    uint8_t available;
    uint8_t saved_ea = EA;
    EA = 0;

    available = (uint8_t)((tx_tail - tx_head - 1U) & TX_MASK);
    if (length > available) {
        if (tx_overflow_count != UINT16_MAX) {
            ++tx_overflow_count;
        }
        EA = saved_ea;
        return 0;
    }

    for (i = 0; i < length; ++i) {
        tx_buffer[tx_head] = data[i];
        tx_head = (uint8_t)((tx_head + 1U) & TX_MASK);
    }
    if (!tx_active) {
        tx_active = 1;
        if (bootloader_mode) TB8 = even_parity_bit(tx_buffer[tx_tail]);
        SBUF = tx_buffer[tx_tail];
        tx_tail = (uint8_t)((tx_tail + 1U) & TX_MASK);
    }
    EA = saved_ea;
    return 1;
}

uint16_t uart_rx_overflows(void)
{
    return rx_overflow_count;
}

uint16_t uart_tx_overflows(void)
{
    return tx_overflow_count;
}

void uart0_isr(void) __interrupt (4)
{
    if (RI) {
        uint8_t next;
        uint8_t value;
        RI = 0;
        value = SBUF;
        if (bootloader_mode && RB8 != even_parity_bit(value)) {
            if (rx_overflow_count != UINT16_MAX) ++rx_overflow_count;
            goto receive_done;
        }
        next = (uint8_t)((rx_head + 1U) & RX_MASK);
        if (next == rx_tail) {
            if (rx_overflow_count != UINT16_MAX) {
                ++rx_overflow_count;
            }
        } else {
            rx_buffer[rx_head] = value;
            rx_head = next;
        }
receive_done:
        ;
    }
    if (TI) {
        TI = 0;
        if (tx_tail != tx_head) {
            if (bootloader_mode) TB8 = even_parity_bit(tx_buffer[tx_tail]);
            SBUF = tx_buffer[tx_tail];
            tx_tail = (uint8_t)((tx_tail + 1U) & TX_MASK);
        } else {
            tx_active = 0;
        }
    }
}
