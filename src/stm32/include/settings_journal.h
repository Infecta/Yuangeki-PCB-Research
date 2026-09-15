#ifndef YTG_SETTINGS_JOURNAL_H
#define YTG_SETTINGS_JOURNAL_H

#include <stdint.h>

#define YTG_SETTINGS_NO_RECORD 0xFFU

enum ytg_settings_slot {
    YTG_SETTINGS_SLOT_NONE = 0,
    YTG_SETTINGS_SLOT_A = 1,
    YTG_SETTINGS_SLOT_B = 2
};

enum ytg_settings_slot settings_journal_target(enum ytg_settings_slot active);
uint8_t settings_journal_generation_is_newer(uint32_t candidate,
                                              uint32_t reference);
uint32_t settings_journal_next_generation(uint32_t active_generation);
uint8_t settings_journal_find_erased(const uint32_t *records,
                                     uint8_t words_per_record,
                                     uint8_t record_count);

#endif
