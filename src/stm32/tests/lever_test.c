#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "lever.h"

int main(void)
{
    lever_init(2048);
    assert(lever_raw() == 2048);
    assert(lever_filtered() == 2048);
    assert(lever_raw_legacy() == 128);

    lever_update(4095);
    assert(lever_raw() == 4095);
    assert(lever_raw_legacy() == 255);
    assert(lever_filtered() == 2048); /* isolated spike is rejected */
    lever_update(4095);
    assert(lever_filtered() > 2048);
    assert(lever_filtered() < 4095);

    for (unsigned i = 0; i < 64; ++i) {
        lever_update(4095);
    }
    assert(lever_filtered() > 4090);
    assert(lever_filtered_legacy() == 255);

    for (unsigned i = 0; i < 64; ++i) {
        lever_update(0);
    }
    assert(lever_filtered() < 5);
    assert(lever_raw_legacy() == 0);

    lever_init(2000);
    lever_update(2001);
    lever_update(1999);
    lever_update(2002);
    lever_update(1998);
    assert(lever_filtered() == 2000);

    puts("lever tests passed");
    return 0;
}
