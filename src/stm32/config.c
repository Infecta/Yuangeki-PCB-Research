#include <stdint.h>

#include "board.h"
#include "config.h"
#include "lighting.h"
#include "mcu.h"
#include "settings_journal.h"
#include "strip.h"

#define CONFIG_ADDRESS_A 0x0800F000UL
#define CONFIG_ADDRESS_B 0x0800F800UL
#define CONFIG_PAGE_A 30U
#define CONFIG_PAGE_B 31U
#define CONFIG_PAGE_SIZE 0x800UL
#define CONFIG_RECORDS_PER_PAGE (CONFIG_PAGE_SIZE / sizeof(struct config_record))
#define CONFIG_MAGIC 0x59475443UL /* "YGTC" */
#define CONFIG_VERSION 2U

#define FLASH_KEY1 0x45670123UL
#define FLASH_KEY2 0xCDEF89ABUL
#define FLASH_SR_EOP (1UL << 0)
#define FLASH_SR_ERROR_MASK 0x000003FAUL
#define FLASH_SR_BSY (1UL << 16)
#define FLASH_SR_CFGBSY (1UL << 18)
#define FLASH_CR_PG (1UL << 0)
#define FLASH_CR_PER (1UL << 1)
#define FLASH_CR_PNB_MASK (0x3FFUL << 3)
#define FLASH_CR_STRT (1UL << 16)
#define FLASH_CR_LOCK (1UL << 31)

struct config_record_v1 {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t effect;
    uint8_t value;
    uint8_t speed;
    uint8_t reactive_mode;
    uint8_t off_level;
    uint8_t on_level;
    uint8_t lighting_mode;
    uint8_t reserved0;
    uint32_t reserved1;
    uint32_t crc32;
};

struct config_record {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t effect;
    uint8_t value;
    uint8_t speed;
    uint8_t reactive_mode;
    uint8_t off_level;
    uint8_t on_level;
    uint8_t lighting_mode;
    uint8_t strip_effect;
    uint8_t strip_brightness;
    uint8_t strip_speed;
    uint8_t strip_red;
    uint8_t strip_green;
    uint8_t strip_blue;
    uint8_t strip_color_order;
    uint8_t strip_pixel_count;
    uint32_t generation;
    uint32_t crc32;
};

_Static_assert(sizeof(struct config_record) == 32U,
               "configuration record must use four flash double-words");
_Static_assert(sizeof(struct config_record_v1) == 24U,
               "legacy configuration layout changed unexpectedly");

static struct config_record current;
static uint8_t loaded_from_flash;
static uint16_t save_failure_count;
static enum ytg_settings_slot active_slot;
static uint32_t active_generation;

static uint32_t crc32_bytes(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

static uint32_t record_crc(const struct config_record *record)
{
    return crc32_bytes((const uint8_t *)record,
                       (uint32_t)sizeof(*record) - sizeof(record->crc32));
}

static void set_defaults(void)
{
    current.magic = CONFIG_MAGIC;
    current.version = CONFIG_VERSION;
    current.size = sizeof(current);
    current.effect = 0;
    current.value = 80U;
    current.speed = 0;
    current.reactive_mode = 1U;
    current.off_level = 80U;
    current.on_level = 255U;
    current.lighting_mode = 0;
    current.strip_effect = 0;
    current.strip_brightness = 64U;
    current.strip_speed = 64U;
    current.strip_red = 0;
    current.strip_green = 160U;
    current.strip_blue = 255U;
    current.strip_color_order = 0;
    current.strip_pixel_count = 16U;
    current.generation = 0;
    current.crc32 = record_crc(&current);
}

static uint8_t record_valid(const struct config_record *record)
{
    return record->magic == CONFIG_MAGIC &&
           record->version == CONFIG_VERSION &&
           record->size == sizeof(*record) &&
           record->lighting_mode < 3U &&
           record->strip_effect < 4U &&
           record->strip_color_order < 2U &&
           record->strip_pixel_count >= 1U && record->strip_pixel_count <= STRIP_MAX_PIXELS &&
           record->crc32 == record_crc(record);
}

static uint8_t legacy_record_valid(const struct config_record_v1 *record)
{
    return record->magic == CONFIG_MAGIC && record->version == 1U &&
           record->size == sizeof(*record) && record->lighting_mode < 3U &&
           record->crc32 == crc32_bytes((const uint8_t *)record,
               (uint32_t)sizeof(*record) - sizeof(record->crc32));
}

void config_init(void)
{
    const uint32_t page_addresses[2] = {CONFIG_ADDRESS_A, CONFIG_ADDRESS_B};
    active_slot = YTG_SETTINGS_SLOT_NONE;
    active_generation = 0U;
    for (uint8_t page = 0U; page < 2U; ++page) {
        const struct config_record *records =
            (const struct config_record *)page_addresses[page];
        for (uint8_t slot = 0U; slot < CONFIG_RECORDS_PER_PAGE; ++slot) {
            const struct config_record *record = &records[slot];
            if (record_valid(record) != 0U &&
                (active_slot == YTG_SETTINGS_SLOT_NONE ||
                 settings_journal_generation_is_newer(record->generation,
                                                       active_generation) != 0U)) {
                current = *record;
                active_generation = record->generation;
                active_slot = page == 0U ? YTG_SETTINGS_SLOT_A :
                                           YTG_SETTINGS_SLOT_B;
            }
        }
    }
    if (active_slot != YTG_SETTINGS_SLOT_NONE) {
        loaded_from_flash = 1U;
    } else if (legacy_record_valid((const struct config_record_v1 *)CONFIG_ADDRESS_B)) {
        const struct config_record_v1 *legacy =
            (const struct config_record_v1 *)CONFIG_ADDRESS_B;
        set_defaults();
        current.effect = legacy->effect;
        current.value = legacy->value;
        current.speed = legacy->speed;
        current.reactive_mode = legacy->reactive_mode;
        current.off_level = legacy->off_level;
        current.on_level = legacy->on_level;
        current.lighting_mode = legacy->lighting_mode;
        active_generation = 0U;
        loaded_from_flash = 1U;
    } else {
        set_defaults();
        active_generation = 0U;
        loaded_from_flash = 0U;
    }
    save_failure_count = 0;

    lighting_apply_settings(current.effect, current.value, current.speed,
                            current.reactive_mode, current.off_level,
                            current.on_level);
    lighting_set_mode(current.lighting_mode);
    strip_apply_settings(current.strip_effect, current.strip_brightness,
                         current.strip_speed, current.strip_red,
                         current.strip_green, current.strip_blue,
                         current.strip_color_order, current.strip_pixel_count);
}

void config_set_lighting(uint8_t effect, uint8_t value, uint8_t speed,
                         uint8_t reactive_mode, uint8_t off_level,
                         uint8_t on_level)
{
    current.effect = effect;
    current.value = value;
    current.speed = speed;
    current.reactive_mode = reactive_mode != 0U ? 1U : 0U;
    current.off_level = off_level;
    current.on_level = on_level;
}

void config_set_lighting_mode(uint8_t mode)
{
    if (mode < 3U) {
        current.lighting_mode = mode;
    }
}

void config_set_strip(uint8_t effect, uint8_t brightness, uint8_t speed,
                      uint8_t red, uint8_t green, uint8_t blue,
                      uint8_t color_order, uint8_t pixel_count)
{
    current.strip_effect = effect <= 3U ? effect : 0U;
    current.strip_brightness = brightness;
    current.strip_speed = speed;
    current.strip_red = red;
    current.strip_green = green;
    current.strip_blue = blue;
    current.strip_color_order = color_order != 0U ? 1U : 0U;
    current.strip_pixel_count = (pixel_count >= 1U && pixel_count <= STRIP_MAX_PIXELS) ?
                                pixel_count : 16U;
}

static void flash_wait(void)
{
    while ((FLASH_SR & (FLASH_SR_BSY | FLASH_SR_CFGBSY)) != 0U) {
    }
}

static uint8_t flash_failed(void)
{
    return (FLASH_SR & FLASH_SR_ERROR_MASK) != 0U;
}

static uint32_t find_erased_record(uint32_t page_address)
{
    const uint8_t slot = settings_journal_find_erased(
        (const uint32_t *)page_address,
        (uint8_t)(sizeof(struct config_record) / sizeof(uint32_t)),
        CONFIG_RECORDS_PER_PAGE);
    return slot == YTG_SETTINGS_NO_RECORD ? 0U :
           page_address + (uint32_t)slot * sizeof(struct config_record);
}

uint8_t config_save(void)
{
    struct config_record candidate = current;
    enum ytg_settings_slot target_slot = active_slot;
    uint32_t target_address;
    uint32_t target_page;
    uint8_t erase_target = 0U;

    if (target_slot == YTG_SETTINGS_SLOT_NONE) {
        target_slot = YTG_SETTINGS_SLOT_A;
    }
    target_page = target_slot == YTG_SETTINGS_SLOT_A ?
                  CONFIG_PAGE_A : CONFIG_PAGE_B;
    target_address = find_erased_record(target_slot == YTG_SETTINGS_SLOT_A ?
                                        CONFIG_ADDRESS_A : CONFIG_ADDRESS_B);
    if (target_address == 0U) {
        target_slot = settings_journal_target(target_slot);
        target_page = target_slot == YTG_SETTINGS_SLOT_A ?
                      CONFIG_PAGE_A : CONFIG_PAGE_B;
        target_address = target_slot == YTG_SETTINGS_SLOT_A ?
                         CONFIG_ADDRESS_A : CONFIG_ADDRESS_B;
        erase_target = 1U;
    }
    candidate.generation = settings_journal_next_generation(active_generation);
    candidate.crc32 = record_crc(&candidate);

    /* Do not begin a flash erase while an acknowledgement/input frame is
       still leaving USART1. */
    hardware_uart_drain();
    irq_disable();
    flash_wait();
    if ((FLASH_CR & FLASH_CR_LOCK) != 0U) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
    if ((FLASH_CR & FLASH_CR_LOCK) != 0U) {
        irq_enable();
        goto failed;
    }

    if (erase_target != 0U) {
        FLASH_SR = FLASH_SR_EOP | FLASH_SR_ERROR_MASK;
        FLASH_CR = (FLASH_CR & ~FLASH_CR_PNB_MASK) |
                   FLASH_CR_PER | (target_page << 3);
        FLASH_CR |= FLASH_CR_STRT;
        flash_wait();
        FLASH_CR &= ~FLASH_CR_PER;
        if (flash_failed() != 0U) {
            FLASH_CR |= FLASH_CR_LOCK;
            irq_enable();
            goto failed;
        }
    }

    const uint32_t *source = (const uint32_t *)&candidate;
    volatile uint32_t *destination = (volatile uint32_t *)target_address;
    for (uint8_t i = 0; i < sizeof(candidate) / 8U; ++i) {
        FLASH_SR = FLASH_SR_EOP | FLASH_SR_ERROR_MASK;
        FLASH_CR |= FLASH_CR_PG;
        destination[i * 2U] = source[i * 2U];
        instruction_sync_barrier();
        destination[i * 2U + 1U] = source[i * 2U + 1U];
        flash_wait();
        FLASH_CR &= ~FLASH_CR_PG;
        if (flash_failed() != 0U) {
            FLASH_CR |= FLASH_CR_LOCK;
            irq_enable();
            goto failed;
        }
    }
    FLASH_CR |= FLASH_CR_LOCK;
    irq_enable();

    if (record_valid((const struct config_record *)target_address) == 0U) {
        goto failed;
    }
    current = candidate;
    active_slot = target_slot;
    active_generation = candidate.generation;
    loaded_from_flash = 1U;
    return 1U;

failed:
    if (save_failure_count != UINT16_MAX) {
        ++save_failure_count;
    }
    return 0U;
}

uint32_t config_generation(void)
{
    return active_generation;
}

uint8_t config_was_loaded(void)
{
    return loaded_from_flash;
}

uint16_t config_save_failures(void)
{
    return save_failure_count;
}
