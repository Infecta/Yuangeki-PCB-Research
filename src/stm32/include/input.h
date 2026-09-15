#ifndef YTG_INPUT_H
#define YTG_INPUT_H

#include <stdint.h>

void input_init(void);
void input_tick_1khz(void);
uint16_t input_buttons(void);
uint16_t input_pressed_edges(void);

#endif
