#include <stdint.h>

#include "settings_journal.h"

uint8_t settings_journal_generation_is_newer(uint32_t candidate,
                                              uint32_t reference)
{
    return (int32_t)(candidate - reference) > 0;
}

enum ytg_settings_slot settings_journal_target(enum ytg_settings_slot active)
{
    return active == YTG_SETTINGS_SLOT_A ?
           YTG_SETTINGS_SLOT_B : YTG_SETTINGS_SLOT_A;
}

uint32_t settings_journal_next_generation(uint32_t active_generation)
{
    return active_generation + 1U;
}

uint8_t settings_journal_find_erased(const uint32_t *records,
                                     uint8_t words_per_record,
                                     uint8_t record_count)
{
    for (uint8_t record = 0U; record < record_count; ++record) {
        uint8_t erased = 1U;
        for (uint8_t word = 0U; word < words_per_record; ++word) {
            if (records[(uint16_t)record * words_per_record + word] !=
                UINT32_MAX) {
                erased = 0U;
                break;
            }
        }
        if (erased != 0U) return record;
    }
    return YTG_SETTINGS_NO_RECORD;
}
