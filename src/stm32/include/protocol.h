#ifndef YTG_PROTOCOL_H
#define YTG_PROTOCOL_H

#include <stdint.h>

void protocol_send_input(uint16_t buttons, uint8_t raw_lever, uint8_t filtered_lever);
void protocol_send_save_ack(uint8_t token, uint8_t success);
void protocol_init(void);
void protocol_poll(void);
uint16_t protocol_valid_frames(void);
uint16_t protocol_invalid_frames(void);
uint8_t protocol_take_save_request(void);
uint8_t protocol_save_token(void);
uint8_t protocol_take_bootloader_request(void);
#ifdef YTG_UNIT_TEST
void protocol_feed_byte_for_test(uint8_t byte);
#endif

#endif
