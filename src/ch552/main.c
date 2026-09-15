#include "bootloader.h"
#include "bridge.h"
#include "ch552.h"
#include "uart.h"
#include "usb.h"
#include "updater.h"

void main(void)
{
    clock_init_12mhz();
    uart_init();
    bridge_init();
    updater_init();
    usb_init();
    EA = 1;

    for (;;) {
        bridge_poll();
        usb_poll();
        ch552_bootloader_poll();
        if (bridge_input_changed()) {
            if (usb_send_input(bridge_input_report(), HID_INPUT_REPORT_SIZE)) {
                bridge_input_sent();
            }
        }
        if (bridge_mu3_input_changed()) {
            if (usb_send_mu3_input(bridge_mu3_input_report(),
                                   MU3_INPUT_REPORT_SIZE)) {
                bridge_mu3_input_sent();
            }
        }
    }
}
