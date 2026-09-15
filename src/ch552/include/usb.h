#ifndef YTG_CH552_USB_H
#define YTG_CH552_USB_H

#include <stdint.h>

void usb_init(void);
void usb_poll(void);
uint8_t usb_send_input(const uint8_t *report, uint8_t length);
uint8_t usb_send_mu3_input(const uint8_t *report, uint8_t length);
uint8_t usb_configured(void);
uint16_t usb_millis(void);
void usb_isr(void) __interrupt (8);

#endif
