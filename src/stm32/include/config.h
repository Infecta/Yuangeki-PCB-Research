#ifndef YTG_CONFIG_H
#define YTG_CONFIG_H

#include <stdint.h>

void config_init(void);
void config_set_lighting(uint8_t effect, uint8_t value, uint8_t speed,
                         uint8_t reactive_mode, uint8_t off_level,
                         uint8_t on_level);
void config_set_lighting_mode(uint8_t mode);
void config_set_strip(uint8_t effect, uint8_t brightness, uint8_t speed,
                      uint8_t red, uint8_t green, uint8_t blue,
                      uint8_t color_order, uint8_t pixel_count);
uint8_t config_save(void);
uint32_t config_generation(void);
uint8_t config_was_loaded(void);
uint16_t config_save_failures(void);

#endif
