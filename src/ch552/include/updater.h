#ifndef YTG_CH552_UPDATER_H
#define YTG_CH552_UPDATER_H

#include <stdint.h>

enum updater_public_state {
    UPDATE_IDLE = 0,
    UPDATE_ENTERING = 1,
    UPDATE_READY = 2,
    UPDATE_ERASING = 3,
    UPDATE_WRITING = 4,
    UPDATE_VERIFYING = 5,
    UPDATE_STARTING = 6,
    UPDATE_COMPLETE = 7,
    UPDATE_ERROR = 0x80
};

void updater_init(void);
void updater_poll(void);
uint8_t updater_handle_output(const uint8_t *report, uint8_t length);
uint8_t updater_active(void);
uint8_t updater_take_status_changed(void);
uint8_t updater_status_state(void);
uint8_t updater_status_token(void);
uint8_t updater_status_error(void);
uint32_t updater_status_address(void);

#endif
