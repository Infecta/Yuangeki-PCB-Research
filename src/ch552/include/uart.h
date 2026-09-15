#ifndef YTG_CH552_UART_H
#define YTG_CH552_UART_H

#include <stdint.h>

void uart_init(void);
void uart_init_application(void);
void uart_init_bootloader(void);
void uart_clear(void);
int16_t uart_read(void);
uint8_t uart_write(const uint8_t *data, uint8_t length);
uint16_t uart_rx_overflows(void);
uint16_t uart_tx_overflows(void);
void uart0_isr(void) __interrupt (4);

#endif
