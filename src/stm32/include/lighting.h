#ifndef YTG_LIGHTING_H
#define YTG_LIGHTING_H

#include <stdint.h>

void lighting_init(void);
void lighting_tick_1khz(uint16_t buttons, uint16_t pressed_edges);
void lighting_pwm_tick(void);
uint8_t lighting_mode(void);
void lighting_apply_settings(uint8_t effect, uint8_t value, uint8_t speed,
                             uint8_t reactive_mode, uint8_t off_level,
                             uint8_t on_level);
void lighting_set_host_levels(const uint8_t levels[12]);
void lighting_clear_host_levels(void);
void lighting_set_mode(uint8_t new_mode);
uint8_t lighting_take_mode_changed(void);

#endif
