#ifndef YTG_BOARD_H
#define YTG_BOARD_H

#include <stdint.h>

enum button_id {
    BUTTON_LEFT_A = 0,
    BUTTON_LEFT_B,
    BUTTON_LEFT_C,
    BUTTON_RIGHT_A,
    BUTTON_RIGHT_B,
    BUTTON_RIGHT_C,
    BUTTON_MENU_LEFT,
    BUTTON_MENU_RIGHT,
    BUTTON_WAD_LEFT,
    BUTTON_WAD_RIGHT,
    BUTTON_TEST,
    BUTTON_SERVICE,
    BUTTON_COUNT
};

void hardware_init(void);
uint16_t hardware_read_buttons(void);
uint16_t hardware_read_adc(void);
void hardware_write_button_leds(uint16_t on_mask);
void hardware_write_sensor_drive(uint8_t high);
void hardware_write_addressable(const uint8_t *data, uint16_t length);
void hardware_uart_write(const uint8_t *data, uint8_t length);
void hardware_uart_drain(void);
int16_t hardware_uart_read(void);
uint16_t hardware_uart_rx_overflows(void);
void hardware_request_rom_bootloader(void);

#endif
