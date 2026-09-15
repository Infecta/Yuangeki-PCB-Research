#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "settings_journal.h"

int main(void)
{
    uint32_t records[4U * 3U];

    assert(settings_journal_target(YTG_SETTINGS_SLOT_NONE) == YTG_SETTINGS_SLOT_A);
    assert(settings_journal_target(YTG_SETTINGS_SLOT_A) == YTG_SETTINGS_SLOT_B);
    assert(settings_journal_target(YTG_SETTINGS_SLOT_B) == YTG_SETTINGS_SLOT_A);
    assert(settings_journal_generation_is_newer(5U, 4U) == 1U);
    assert(settings_journal_generation_is_newer(4U, 5U) == 0U);
    assert(settings_journal_generation_is_newer(0U, UINT32_MAX) == 1U);
    assert(settings_journal_next_generation(UINT32_MAX) == 0U);

    for (uint8_t i = 0U; i < 12U; ++i) records[i] = UINT32_MAX;
    records[0] = 0U;
    assert(settings_journal_find_erased(records, 4U, 3U) == 1U);
    records[4] = 0U;
    assert(settings_journal_find_erased(records, 4U, 3U) == 2U);
    records[8] = 0U;
    assert(settings_journal_find_erased(records, 4U, 3U) ==
           YTG_SETTINGS_NO_RECORD);

    puts("settings journal tests passed");
    return 0;
}
