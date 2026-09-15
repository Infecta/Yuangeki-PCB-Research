#include <stdint.h>

#include "bootloader.h"
#include "ch552.h"
#include "usb.h"

#define BOOTLOADER_ADDRESS 0x3800
#define BOOTLOADER_DELAY_MS 50U

static __bit bootloader_pending;
static __data uint16_t bootloader_deadline;

static void usb_detach_delay(void)
{
    uint8_t outer;
    uint8_t inner;

    /* Give the host time to observe the application disconnect before the
       factory bootloader reconnects. Interrupts are already disabled here. */
    for (outer = 0U; outer < 128U; ++outer) {
        for (inner = 0U; inner < 255U; ++inner) {
            __asm
                nop
            __endasm;
        }
    }
}

void ch552_bootloader_request(void)
{
    if (!bootloader_pending) {
        bootloader_pending = 1;
        bootloader_deadline = (uint16_t)(usb_millis() + BOOTLOADER_DELAY_MS);
    }
}

void ch552_bootloader_poll(void)
{
    if (!bootloader_pending ||
        (int16_t)(usb_millis() - bootloader_deadline) < 0) {
        return;
    }

    USB_INT_EN = 0;
    IE_EX &= (uint8_t)~bIE_USB;
    UDEV_CTRL = 0;
    USB_CTRL = 0;
    ES = 0;
    REN = 0;
    T2CON &= (uint8_t)~bTR2;
    EA = 0;
    usb_detach_delay();

    __asm
        ljmp BOOTLOADER_ADDRESS
    __endasm;
}
