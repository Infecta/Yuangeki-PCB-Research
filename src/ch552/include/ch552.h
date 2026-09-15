#ifndef YTG_CH552_H
#define YTG_CH552_H

#include <stdint.h>

/* Standard 8051 registers. */
__sfr __at (0x81) SP;
__sfr __at (0x87) PCON;
__sfr __at (0x88) TCON;
__sfr __at (0x89) TMOD;
__sfr __at (0x8B) TL1;
__sfr __at (0x8D) TH1;
__sfr __at (0x90) P1;
__sfr __at (0x98) SCON;
__sfr __at (0x99) SBUF;
__sfr __at (0xA1) SAFE_MOD;
__sfr __at (0xA8) IE;
__sfr __at (0xB0) P3;
__sfr __at (0xB8) IP;

__sbit __at (0x98) RI;
__sbit __at (0x99) TI;
__sbit __at (0x9A) RB8;
__sbit __at (0x9B) TB8;
__sbit __at (0x9C) REN;
__sbit __at (0xAC) ES;
__sbit __at (0xAF) EA;

/* CH552 extended registers. */
__sfr __at (0x91) USB_C_CTRL;
__sfr __at (0xB9) CLOCK_CFG;
__sfr __at (0xC6) PIN_FUNC;
__sfr __at (0xC8) T2CON;
__sfr __at (0xC9) T2MOD;
__sfr __at (0xCA) RCAP2L;
__sfr __at (0xCB) RCAP2H;
__sfr __at (0xCC) TL2;
__sfr __at (0xCD) TH2;
__sfr __at (0xD1) UDEV_CTRL;
__sfr __at (0xD2) UEP1_CTRL;
__sfr __at (0xD3) UEP1_T_LEN;
__sfr __at (0xD6) UEP3_CTRL;
__sfr __at (0xD7) UEP3_T_LEN;
__sfr __at (0xDC) UEP0_CTRL;
__sfr __at (0xDD) UEP0_T_LEN;
__sfr __at (0xDE) UEP4_CTRL;
__sfr __at (0xDF) UEP4_T_LEN;
__sfr __at (0xD8) USB_INT_FG;
__sfr __at (0xD9) USB_INT_ST;
__sfr __at (0xDA) USB_MIS_ST;
__sfr __at (0xDB) USB_RX_LEN;
__sfr __at (0xE1) USB_INT_EN;
__sfr __at (0xE2) USB_CTRL;
__sfr __at (0xE3) USB_DEV_AD;
__sfr __at (0xE8) IE_EX;
__sfr __at (0xEA) UEP4_1_MOD;
__sfr __at (0xEB) UEP2_3_MOD;
__sfr16 __at (0xE7E6) UEP3_DMA;
/* SDCC encodes paired SFR addresses as high-byte-address:low-byte-address. */
__sfr16 __at (0xEDEC) UEP0_DMA;
__sfr16 __at (0xEFEE) UEP1_DMA;

#define bUART0_PIN_X 0x10U

#define bTMR_CLK 0x80U
#define bT2_CLK 0x40U
#define bRCLK 0x20U
#define bTCLK 0x10U
#define bTR2 0x04U

#define bIE_USB 0x04U

#define UIF_FIFO_OV 0x10U
#define UIF_SUSPEND 0x04U
#define UIF_TRANSFER 0x02U
#define UIF_BUS_RST 0x01U
#define bUIE_DEV_SOF 0x80U
#define U_TOG_OK 0x40U

#define MASK_UIS_TOKEN 0x30U
#define MASK_UIS_ENDP 0x0FU
#define UIS_TOKEN_OUT 0x00U
#define UIS_TOKEN_SOF 0x10U
#define UIS_TOKEN_IN 0x20U
#define UIS_TOKEN_SETUP 0x30U

#define bUC_DEV_PU_EN 0x20U
#define bUC_INT_BUSY 0x08U
#define bUC_RESET_SIE 0x04U
#define bUC_CLR_ALL 0x02U
#define bUC_DMA_EN 0x01U

#define bUD_PD_DIS 0x80U
#define bUD_PORT_EN 0x01U

#define bUEP1_RX_EN 0x80U
#define bUEP1_TX_EN 0x40U
#define bUEP4_TX_EN 0x04U
#define bUEP3_RX_EN 0x80U
#define bUEP_AUTO_TOG 0x10U
#define bUEP_R_TOG 0x80U
#define bUEP_T_TOG 0x40U
#define MASK_UEP_R_RES 0x0CU
#define MASK_UEP_T_RES 0x03U
#define UEP_R_RES_ACK 0x00U
#define UEP_R_RES_NAK 0x08U
#define UEP_R_RES_STALL 0x0CU
#define UEP_T_RES_ACK 0x00U
#define UEP_T_RES_NAK 0x02U
#define UEP_T_RES_STALL 0x03U

#define USB_GET_STATUS 0x00U
#define USB_CLEAR_FEATURE 0x01U
#define USB_SET_FEATURE 0x03U
#define USB_SET_ADDRESS 0x05U
#define USB_GET_DESCRIPTOR 0x06U
#define USB_GET_CONFIGURATION 0x08U
#define USB_SET_CONFIGURATION 0x09U
#define USB_GET_INTERFACE 0x0AU
#define USB_SET_INTERFACE 0x0BU

#define HID_GET_REPORT 0x01U
#define HID_GET_IDLE 0x02U
#define HID_GET_PROTOCOL 0x03U
#define HID_SET_REPORT 0x09U
#define HID_SET_IDLE 0x0AU
#define HID_SET_PROTOCOL 0x0BU

#define USB_DESC_DEVICE 0x01U
#define USB_DESC_CONFIGURATION 0x02U
#define USB_DESC_STRING 0x03U
#define USB_DESC_HID 0x21U
#define USB_DESC_REPORT 0x22U
#define USB_DESC_BOS 0x0FU

static inline void clock_init_12mhz(void)
{
    SAFE_MOD = 0x55U;
    SAFE_MOD = 0xAAU;
    CLOCK_CFG = 0x84U; /* Internal 24 MHz oscillator, Fsys = 12 MHz. */
    SAFE_MOD = 0x00U;
}

#endif
