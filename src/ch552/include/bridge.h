#ifndef YTG_CH552_BRIDGE_H
#define YTG_CH552_BRIDGE_H

#include <stdint.h>

#define HID_INPUT_REPORT_SIZE 16U
#define HID_OUTPUT_REPORT_SIZE 32U
#define MU3_INPUT_REPORT_SIZE 8U
#define MU3_OUTPUT_REPORT_SIZE 33U

void bridge_init(void);
void bridge_poll(void);
void bridge_handle_output(const uint8_t *report, uint8_t length);
void bridge_handle_mu3_output(const uint8_t *report, uint8_t length);
void bridge_begin_mu3_session(void);
const uint8_t *bridge_input_report(void);
const uint8_t *bridge_mu3_input_report(void);
uint8_t bridge_input_changed(void);
void bridge_input_sent(void);
uint8_t bridge_mu3_input_changed(void);
void bridge_mu3_input_sent(void);

#endif
