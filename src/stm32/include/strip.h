#ifndef YTG_STRIP_H
#define YTG_STRIP_H

#include <stdint.h>

#define STRIP_MAX_PIXELS 32U

void strip_init(void);
void strip_tick_1khz(void);
void strip_apply_settings(uint8_t effect, uint8_t brightness, uint8_t speed,
                          uint8_t red, uint8_t green, uint8_t blue,
                          uint8_t color_order, uint8_t pixel_count);

#ifdef YTG_UNIT_TEST
const uint8_t *strip_encoded_for_test(void);
uint16_t strip_encoded_length_for_test(void);
#endif

#endif
