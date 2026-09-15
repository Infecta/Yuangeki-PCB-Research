#ifndef YTG_LEVER_H
#define YTG_LEVER_H

#include <stdint.h>

void lever_init(uint16_t initial_sample);
void lever_update(uint16_t sample);
uint16_t lever_raw(void);
uint16_t lever_filtered(void);
uint8_t lever_raw_legacy(void);
uint8_t lever_filtered_legacy(void);

#endif
