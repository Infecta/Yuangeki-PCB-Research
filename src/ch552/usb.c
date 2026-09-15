#include "bridge.h"
#include "ch552.h"
#include "usb.h"

#define EP0_SIZE 64U
#define EP1_SIZE 32U

#define REQUEST_TYPE_MASK 0x60U
#define REQUEST_TYPE_STANDARD 0x00U
#define REQUEST_TYPE_CLASS 0x20U
#define REQUEST_TYPE_VENDOR 0x40U

/* USB DMA buffers occupy xRAM 0x0000..0x013F. The linker starts ordinary
   xdata at 0x0140 so these regions cannot collide. */
__xdata __at (0x0000) static uint8_t ep0_buffer[EP0_SIZE];
__xdata __at (0x0040) static uint8_t ep4_in_buffer[64];
__xdata __at (0x0080) static uint8_t ep1_out_buffer[64];
__xdata __at (0x00C0) static uint8_t ep1_in_buffer[64];
__xdata __at (0x0100) static uint8_t ep3_out_buffer[64];

static __code const uint8_t device_descriptor[] = {
    18, USB_DESC_DEVICE, 0x00, 0x02,
    0x00, 0x00, 0x00, EP0_SIZE,
    0x8F, 0x0E,             /* VID 0E8F: MU3IO.NET Ontroller contract. */
    0x16, 0x12,             /* PID 1216. */
    0x03, 0x01,             /* Device revision 1.03 forces descriptor refresh. */
    1, 2, 0, 1
};

static __code const uint8_t report_descriptor[] = {
    0x06, 0x00, 0xFF,       /* Usage Page (vendor-defined 0xFF00) */
    0x09, 0x01,             /* Usage 1 */
    0xA1, 0x01,             /* Application collection */
    0x15, 0x00,             /* Logical minimum 0 */
    0x26, 0xFF, 0x00,       /* Logical maximum 255 */
    0x75, 0x08,             /* 8-bit fields */
    0x95, HID_INPUT_REPORT_SIZE,
    0x09, 0x02,
    0x81, 0x02,             /* 16-byte input report */
    0x95, HID_OUTPUT_REPORT_SIZE,
    0x09, 0x03,
    0x91, 0x02,             /* 32-byte output report */
    0xC0
};

static __code const uint8_t configuration_descriptor[] = {
    9, USB_DESC_CONFIGURATION, 46, 0,
    1, 1, 0, 0x80, 50,
    9, 4, 0, 0, 4, 0xFF, 0x00, 0x00, 0,
    7, 5, 0x03, 0x02, 64, 0, 1,
    7, 5, 0x84, 0x02, 64, 0, 1,
    7, 5, 0x81, 0x02, EP1_SIZE, 0, 1,
    7, 5, 0x01, 0x02, EP1_SIZE, 0, 1
};

static __code const uint8_t bos_descriptor[] = {
    5, USB_DESC_BOS, 33, 0, 1,
    28, 0x10, 0x05, 0x00,
    0xDF,0x60,0xDD,0xD8, 0x89,0x45, 0xC7,0x4C,
    0x9C,0xD2,0x65,0x9D,0x9E,0x64,0x8A,0x9F,
    0x00,0x00,0x03,0x06, 178,0, 0x20,0
};

static __code const uint8_t ms_os_20_descriptor[] = {
    10,0, 0,0, 0x00,0x00,0x03,0x06, 178,0,
    8,0, 1,0, 1,0, 168,0,
    8,0, 2,0, 0,0, 160,0,
    20,0, 3,0, 'W','I','N','U','S','B',0,0,
    0,0,0,0,0,0,0,0,

    /* Publish interface 0 under GUID_DEVINTERFACE_USB_DEVICE, which is the
       interface class enumerated by MU3IO.NET's DeviceManager. */
    132,0, 4,0, 7,0, 42,0,
    'D',0,'e',0,'v',0,'i',0,'c',0,'e',0,
    'I',0,'n',0,'t',0,'e',0,'r',0,'f',0,'a',0,'c',0,'e',0,
    'G',0,'U',0,'I',0,'D',0,'s',0, 0,0,
    80,0,
    '{',0,'A',0,'5',0,'D',0,'C',0,'B',0,'F',0,'1',0,'0',0,'-',0,
    '6',0,'5',0,'3',0,'0',0,'-',0,'1',0,'1',0,'D',0,'2',0,'-',0,
    '9',0,'0',0,'1',0,'F',0,'-',0,'0',0,'0',0,'C',0,'0',0,'4',0,
    'F',0,'B',0,'9',0,'5',0,'1',0,'E',0,'D',0,'}',0, 0,0, 0,0
};

static __code const uint8_t string_language[] = {4, USB_DESC_STRING, 0x09, 0x04};
static __code const uint8_t ms_os_10_string[] = {
    18, USB_DESC_STRING,
    'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,
    0x21, 0
};
static __code const uint8_t ms_os_10_compat_id[] = {
    40,0,0,0,               /* dwLength */
    0x00,0x01,              /* bcdVersion 1.00 */
    0x04,0x00,              /* Extended compatible ID descriptor. */
    1, 0,0,0,0,0,0,0,      /* One function section. */
    0, 1,                   /* Interface 0, reserved. */
    'W','I','N','U','S','B',0,0,
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0
};
static __code const uint8_t string_manufacturer[] = {
    22, USB_DESC_STRING,
    'Y',0,'u',0,'a',0,'n',0,'t',0,'G',0,'e',0,'k',0,'i',0
};
static __code const uint8_t string_product[] = {
    40, USB_DESC_STRING,
    'Y',0,'u',0,'a',0,'n',0,'g',0,'e',0,'k',0,'i',0,' ',0,
    'C',0,'o',0,'n',0,'t',0,'r',0,'o',0,'l',0,'l',0,'e',0,'r',0
};

static __data uint8_t configuration_value;
static __data uint8_t pending_address;
static __data uint8_t setup_request;
static __data uint8_t hid_idle;
static __data uint8_t hid_protocol;
static __data uint8_t ep0_remaining;
static __data uint8_t ep0_source_kind;
static __data uint8_t ep0_out_expected;
static volatile __bit ep0_zlp_pending;
static __code const uint8_t *ep0_code_pointer;
static const __xdata uint8_t *ep0_xdata_pointer;
static volatile __bit ep1_in_busy;
static volatile __bit ep4_in_busy;
static volatile __bit output_pending;
static volatile __bit mu3_output_pending;
static volatile __data uint8_t output_length;
static __xdata uint8_t pending_output[HID_OUTPUT_REPORT_SIZE];
static volatile __data uint16_t millisecond_count;

static uint16_t read_setup_word(uint8_t offset)
{
    return (uint16_t)ep0_buffer[offset] |
           ((uint16_t)ep0_buffer[offset + 1U] << 8);
}

static void ep0_stall(void)
{
    UEP0_T_LEN = 0;
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG |
                UEP_R_RES_STALL | UEP_T_RES_STALL;
}

static void ep0_send_next(void)
{
    uint8_t count = ep0_remaining > EP0_SIZE ? EP0_SIZE : ep0_remaining;
    uint8_t i;
    for (i = 0; i < count; ++i) {
        if (ep0_source_kind == 0U) {
            ep0_buffer[i] = *ep0_code_pointer++;
        } else {
            ep0_buffer[i] = *ep0_xdata_pointer++;
        }
    }
    ep0_remaining -= count;
    UEP0_T_LEN = count;
    UEP0_CTRL = (UEP0_CTRL & (uint8_t)~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

static void ep0_begin_code(const __code uint8_t *data, uint8_t available,
                           uint16_t requested)
{
    ep0_code_pointer = data;
    ep0_source_kind = 0;
    ep0_zlp_pending = (available != 0U &&
                       (available % EP0_SIZE) == 0U &&
                       requested > available);
    ep0_remaining = available;
    if (requested < ep0_remaining) ep0_remaining = (uint8_t)requested;
    ep0_send_next();
}

static void ep0_begin_xdata(const __xdata uint8_t *data, uint8_t available,
                            uint16_t requested)
{
    ep0_xdata_pointer = data;
    ep0_source_kind = 1;
    ep0_zlp_pending = (available != 0U &&
                       (available % EP0_SIZE) == 0U &&
                       requested > available);
    ep0_remaining = available;
    if (requested < ep0_remaining) ep0_remaining = (uint8_t)requested;
    ep0_send_next();
}

static void ep0_send_small(uint8_t first, uint8_t second, uint8_t length,
                           uint16_t requested)
{
    ep0_buffer[0] = first;
    ep0_buffer[1] = second;
    if (requested < length) length = (uint8_t)requested;
    UEP0_T_LEN = length;
    ep0_remaining = 0;
    ep0_zlp_pending = 0;
    UEP0_CTRL = (UEP0_CTRL & (uint8_t)~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

static void handle_get_descriptor(uint16_t value, uint16_t length)
{
    uint8_t type = (uint8_t)(value >> 8);
    uint8_t index = (uint8_t)value;
    switch (type) {
    case USB_DESC_DEVICE:
        ep0_begin_code(device_descriptor, sizeof(device_descriptor), length);
        break;
    case USB_DESC_CONFIGURATION:
        ep0_begin_code(configuration_descriptor, sizeof(configuration_descriptor), length);
        break;
    case USB_DESC_STRING:
        if (index == 0U) ep0_begin_code(string_language, sizeof(string_language), length);
        else if (index == 1U) ep0_begin_code(string_manufacturer, sizeof(string_manufacturer), length);
        else if (index == 2U) ep0_begin_code(string_product, sizeof(string_product), length);
        else if (index == 0xEEU) ep0_begin_code(ms_os_10_string, sizeof(ms_os_10_string), length);
        else ep0_stall();
        break;
    case USB_DESC_REPORT:
        ep0_begin_code(report_descriptor, sizeof(report_descriptor), length);
        break;
    case USB_DESC_BOS:
        ep0_begin_code(bos_descriptor, sizeof(bos_descriptor), length);
        break;
    default:
        ep0_stall();
        break;
    }
}

static void handle_setup(void)
{
    uint8_t request_type;
    uint16_t value;
    uint16_t length;

    if (USB_RX_LEN != 8U) {
        ep0_stall();
        return;
    }
    request_type = ep0_buffer[0] & REQUEST_TYPE_MASK;
    setup_request = ep0_buffer[1];
    value = read_setup_word(2);
    length = read_setup_word(6);
    ep0_out_expected = 0;
    ep0_zlp_pending = 0;
    UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;

    if (request_type == REQUEST_TYPE_STANDARD) {
        switch (setup_request) {
        case USB_GET_DESCRIPTOR:
            handle_get_descriptor(value, length);
            return;
        case USB_SET_ADDRESS:
            pending_address = (uint8_t)value & 0x7FU;
            ep0_send_small(0, 0, 0, length);
            return;
        case USB_SET_CONFIGURATION:
            configuration_value = (uint8_t)value;
            if (configuration_value != 0U) bridge_begin_mu3_session();
            ep0_send_small(0, 0, 0, length);
            return;
        case USB_GET_CONFIGURATION:
            ep0_send_small(configuration_value, 0, 1, length);
            return;
        case USB_GET_STATUS:
            ep0_send_small(0, 0, 2, length);
            return;
        case USB_GET_INTERFACE:
            ep0_send_small(0, 0, 1, length);
            return;
        case USB_SET_INTERFACE:
        case USB_CLEAR_FEATURE:
        case USB_SET_FEATURE:
            ep0_send_small(0, 0, 0, length);
            return;
        default:
            ep0_stall();
            return;
        }
    }

    if (request_type == REQUEST_TYPE_CLASS) {
        switch (setup_request) {
        case HID_GET_REPORT:
            ep0_begin_xdata((const __xdata uint8_t *)bridge_input_report(),
                            HID_INPUT_REPORT_SIZE, length);
            return;
        case HID_GET_IDLE:
            ep0_send_small(hid_idle, 0, 1, length);
            return;
        case HID_GET_PROTOCOL:
            ep0_send_small(hid_protocol, 0, 1, length);
            return;
        case HID_SET_IDLE:
            hid_idle = (uint8_t)(value >> 8);
            ep0_send_small(0, 0, 0, length);
            return;
        case HID_SET_PROTOCOL:
            hid_protocol = (uint8_t)value;
            ep0_send_small(0, 0, 0, length);
            return;
        case HID_SET_REPORT:
            if (length <= HID_OUTPUT_REPORT_SIZE) {
                ep0_out_expected = (uint8_t)length;
                UEP0_T_LEN = 0;
                UEP0_CTRL = bUEP_R_TOG | bUEP_T_TOG |
                            UEP_R_RES_ACK | UEP_T_RES_NAK;
                return;
            }
            ep0_stall();
            return;
        default:
            ep0_stall();
            return;
        }
    }
    if (request_type == REQUEST_TYPE_VENDOR) {
        if (setup_request == 0x20U && read_setup_word(4) == 0x0007U) {
            ep0_begin_code(ms_os_20_descriptor, sizeof(ms_os_20_descriptor), length);
            return;
        }
        if (setup_request == 0x21U && read_setup_word(4) == 0x0004U) {
            ep0_begin_code(ms_os_10_compat_id,
                           sizeof(ms_os_10_compat_id), length);
            return;
        }
    }
    ep0_stall();
}

void usb_init(void)
{
    configuration_value = 0;
    pending_address = 0;
    setup_request = 0;
    hid_idle = 0;
    hid_protocol = 1;
    ep0_remaining = 0;
    ep0_out_expected = 0;
    ep0_zlp_pending = 0;
    ep1_in_busy = 0;
    ep4_in_busy = 0;
    output_pending = 0;
    mu3_output_pending = 0;
    output_length = 0;
    millisecond_count = 0;

    USB_CTRL = bUC_RESET_SIE | bUC_CLR_ALL;
    USB_CTRL = 0;
    UEP0_DMA = 0x0000U;
    UEP1_DMA = 0x0080U;
    UEP3_DMA = 0x0100U;
    UEP4_1_MOD = bUEP1_RX_EN | bUEP1_TX_EN | bUEP4_TX_EN;
    UEP2_3_MOD = bUEP3_RX_EN;
    UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    UEP1_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
    UEP3_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
    UEP4_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
    UEP0_T_LEN = 0;
    UEP1_T_LEN = 0;
    UEP3_T_LEN = 0;
    UEP4_T_LEN = 0;
    USB_DEV_AD = 0;
    USB_INT_FG = 0x1FU;
    USB_INT_EN = bUIE_DEV_SOF | UIF_TRANSFER | UIF_BUS_RST | UIF_SUSPEND;
    USB_CTRL = bUC_DEV_PU_EN | bUC_INT_BUSY | bUC_DMA_EN;
    UDEV_CTRL = bUD_PD_DIS | bUD_PORT_EN;
    IE_EX |= bIE_USB;
}

void usb_poll(void)
{
    if (output_pending) {
        bridge_handle_output(pending_output, output_length);
        output_pending = 0;
        UEP1_CTRL = (UEP1_CTRL & (uint8_t)~MASK_UEP_R_RES) |
                    UEP_R_RES_ACK;
    }
    if (mu3_output_pending) {
        /* EP3 remains NAKed, so its DMA buffer is stable until processing is
           complete.  Avoid copying 33 bytes inside the USB interrupt. */
        bridge_handle_mu3_output(ep3_out_buffer, MU3_OUTPUT_REPORT_SIZE);
        mu3_output_pending = 0;
        UEP3_CTRL = (UEP3_CTRL & (uint8_t)~MASK_UEP_R_RES) |
                    UEP_R_RES_ACK;
    }
}

uint8_t usb_send_input(const uint8_t *report, uint8_t length)
{
    uint8_t i;
    uint8_t saved_ea;
    if (configuration_value == 0U || ep1_in_busy || length > EP1_SIZE) return 0;

    saved_ea = EA;
    EA = 0;
    if (ep1_in_busy) {
        EA = saved_ea;
        return 0;
    }
    for (i = 0; i < length; ++i) ep1_in_buffer[i] = report[i];
    UEP1_T_LEN = length;
    ep1_in_busy = 1;
    UEP1_CTRL = (UEP1_CTRL & (uint8_t)~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    EA = saved_ea;
    return 1;
}

uint8_t usb_send_mu3_input(const uint8_t *report, uint8_t length)
{
    uint8_t i;
    uint8_t saved_ea;
    if (configuration_value == 0U || ep4_in_busy || length > 64U) return 0;

    saved_ea = EA;
    EA = 0;
    if (ep4_in_busy) {
        EA = saved_ea;
        return 0;
    }
    for (i = 0; i < length; ++i) ep4_in_buffer[i] = report[i];
    UEP4_T_LEN = length;
    ep4_in_busy = 1;
    UEP4_CTRL = (UEP4_CTRL & (uint8_t)~MASK_UEP_T_RES) | UEP_T_RES_ACK;
    EA = saved_ea;
    return 1;
}

uint8_t usb_configured(void)
{
    return configuration_value != 0U;
}

uint16_t usb_millis(void)
{
    uint16_t result;
    uint8_t saved_ea = EA;
    EA = 0;
    result = millisecond_count;
    EA = saved_ea;
    return result;
}

void usb_isr(void) __interrupt (8)
{
    if (USB_INT_FG & UIF_TRANSFER) {
        uint8_t token = USB_INT_ST & MASK_UIS_TOKEN;
        uint8_t endpoint = USB_INT_ST & MASK_UIS_ENDP;

        if (token == UIS_TOKEN_SOF) ++millisecond_count;

        if (endpoint == 0U) {
            if (token == UIS_TOKEN_SETUP) {
                handle_setup();
            } else if (token == UIS_TOKEN_IN) {
                if (setup_request == USB_SET_ADDRESS) {
                    USB_DEV_AD = pending_address;
                    setup_request = 0;
                }
                if (ep0_remaining != 0U) {
                    UEP0_CTRL ^= bUEP_T_TOG;
                    ep0_send_next();
                } else if (ep0_zlp_pending) {
                    ep0_zlp_pending = 0;
                    UEP0_CTRL ^= bUEP_T_TOG;
                    UEP0_T_LEN = 0;
                    UEP0_CTRL = (UEP0_CTRL & (uint8_t)~MASK_UEP_T_RES) |
                                UEP_T_RES_ACK;
                } else {
                    UEP0_T_LEN = 0;
                    UEP0_CTRL = (UEP0_CTRL & (uint8_t)~MASK_UEP_T_RES) |
                                UEP_T_RES_NAK;
                }
            } else if (token == UIS_TOKEN_OUT) {
                if (ep0_out_expected != 0U && (USB_INT_FG & U_TOG_OK)) {
                    uint8_t length = USB_RX_LEN;
                    uint8_t i;
                    if (length > ep0_out_expected) length = ep0_out_expected;
                    if (!output_pending) {
                        for (i = 0; i < length; ++i) {
                            pending_output[i] = ep0_buffer[i];
                        }
                        output_length = length;
                        output_pending = 1;
                    }
                    ep0_out_expected = 0;
                    UEP0_T_LEN = 0;
                    UEP0_CTRL = (UEP0_CTRL & (uint8_t)~MASK_UEP_T_RES) |
                                UEP_T_RES_ACK;
                }
            }
        } else if (endpoint == 1U) {
            if (token == UIS_TOKEN_IN) {
                UEP1_T_LEN = 0;
                ep1_in_busy = 0;
                UEP1_CTRL = (UEP1_CTRL & (uint8_t)~MASK_UEP_T_RES) |
                            UEP_T_RES_NAK;
            } else if (token == UIS_TOKEN_OUT && (USB_INT_FG & U_TOG_OK)) {
                if (!output_pending) {
                    uint8_t i;
                    output_length = USB_RX_LEN > HID_OUTPUT_REPORT_SIZE ?
                                    HID_OUTPUT_REPORT_SIZE : USB_RX_LEN;
                    for (i = 0; i < output_length; ++i) {
                        pending_output[i] = ep1_out_buffer[i];
                    }
                    output_pending = 1;
                    UEP1_CTRL = (UEP1_CTRL & (uint8_t)~MASK_UEP_R_RES) |
                                UEP_R_RES_NAK;
                }
            }
        } else if (endpoint == 3U) {
            if (token == UIS_TOKEN_OUT && (USB_INT_FG & U_TOG_OK)) {
                if (!mu3_output_pending && USB_RX_LEN >= MU3_OUTPUT_REPORT_SIZE) {
                    mu3_output_pending = 1;
                    UEP3_CTRL = (UEP3_CTRL & (uint8_t)~MASK_UEP_R_RES) |
                                UEP_R_RES_NAK;
                }
            }
        } else if (endpoint == 4U) {
            if (token == UIS_TOKEN_IN) {
                UEP4_T_LEN = 0;
                ep4_in_busy = 0;
                UEP4_CTRL ^= bUEP_T_TOG;
                UEP4_CTRL = (UEP4_CTRL & (uint8_t)~MASK_UEP_T_RES) |
                            UEP_T_RES_NAK;
            }
        }
        USB_INT_FG = UIF_TRANSFER;
    }

    if (USB_INT_FG & UIF_BUS_RST) {
        USB_DEV_AD = 0;
        configuration_value = 0;
        ep1_in_busy = 0;
        ep4_in_busy = 0;
        output_pending = 0;
        mu3_output_pending = 0;
        ep0_zlp_pending = 0;
        UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        UEP1_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
        UEP3_CTRL = bUEP_AUTO_TOG | UEP_R_RES_ACK | UEP_T_RES_NAK;
        UEP4_CTRL = UEP_R_RES_NAK | UEP_T_RES_NAK;
        UEP0_T_LEN = 0;
        UEP1_T_LEN = 0;
        UEP3_T_LEN = 0;
        UEP4_T_LEN = 0;
        USB_INT_FG = UIF_BUS_RST;
    }
    if (USB_INT_FG & UIF_SUSPEND) {
        USB_INT_FG = UIF_SUSPEND;
    }
    if (USB_INT_FG & UIF_FIFO_OV) {
        USB_INT_FG = UIF_FIFO_OV;
    }
}
