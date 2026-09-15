#include "board.h"
#include "lighting.h"

#define PWM_STEPS 64U
#define WAD_PWM_STEPS 8U
#define SHORTCUT_TIMEOUT_MS 1500U
#define DIRECT_LIGHT_MASK ((uint16_t)((1U << (BUTTON_MENU_RIGHT + 1U)) - 1U))

enum {
    LIGHT_MODE_NORMAL = 0,
    LIGHT_MODE_DIM = 1,
    LIGHT_MODE_OFF = 2,
    LIGHT_MODE_COUNT = 3
};

static volatile uint8_t mode;
static volatile uint8_t pwm_phase;
static volatile uint8_t brightness[BUTTON_COUNT];
static uint8_t pwm_duty[BUTTON_COUNT];
static uint8_t wad_duty_left;
static uint8_t wad_duty_right;
static uint8_t host_levels[BUTTON_COUNT];
static uint16_t host_levels_mask;
static uint8_t configured_effect;
static uint8_t configured_value;
static uint8_t configured_speed;
static uint8_t configured_reactive;
static uint8_t configured_off_level;
static uint8_t configured_on_level;
static uint16_t shortcut_timer;
static uint8_t mode_changed;

static uint8_t scale_level(uint8_t level)
{
    return (uint8_t)(((uint16_t)level + 3U) >> 2);
}

void lighting_init(void)
{
    mode = LIGHT_MODE_NORMAL;
    pwm_phase = 0;
    wad_duty_left = 0;
    wad_duty_right = 0;
    shortcut_timer = 0;
    mode_changed = 0;
    host_levels_mask = 0;
    configured_effect = 0;
    configured_value = 80U;
    configured_speed = 0;
    configured_reactive = 1U;
    configured_off_level = 80U;
    configured_on_level = 255U;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        brightness[i] = 0;
        pwm_duty[i] = 0;
        host_levels[i] = 0;
    }
}

void lighting_tick_1khz(uint16_t buttons, uint16_t pressed_edges)
{
    const uint16_t test_bit = (uint16_t)1U << BUTTON_TEST;
    const uint16_t service_bit = (uint16_t)1U << BUTTON_SERVICE;

    if ((pressed_edges & test_bit) != 0U) {
        shortcut_timer = SHORTCUT_TIMEOUT_MS;
    }
    if ((pressed_edges & service_bit) != 0U && shortcut_timer != 0U) {
        ++mode;
        if (mode == LIGHT_MODE_COUNT) {
            mode = LIGHT_MODE_NORMAL;
        }
        mode_changed = 1U;
        shortcut_timer = 0;
    } else if (shortcut_timer != 0U) {
        --shortcut_timer;
    }

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        uint8_t requested;
        if (i == BUTTON_WAD_LEFT || i == BUTTON_WAD_RIGHT) {
            /* WAD lamps always follow their physical active-low buttons. */
            requested = scale_level(
                (buttons & ((uint16_t)1U << i)) != 0U ?
                configured_on_level : configured_off_level);
        } else if ((host_levels_mask & ((uint16_t)1U << i)) != 0U) {
            requested = scale_level(host_levels[i]);
        } else if (configured_reactive != 0U) {
            requested = scale_level(((buttons & ((uint16_t)1U << i)) != 0U) ?
                                    configured_on_level : configured_off_level);
        } else {
            requested = scale_level(configured_value);
        }
        if (mode == LIGHT_MODE_NORMAL) {
            brightness[i] = requested;
        } else if (mode == LIGHT_MODE_DIM) {
            brightness[i] = (uint8_t)(requested >> 2);
        } else {
            brightness[i] = 0;
        }
    }
}

void lighting_pwm_tick(void)
{
    uint16_t mask = 0;
    uint8_t wad_phase;

    /* Commit every scalar lamp duty together at the start of a complete
       carrier cycle. Direct WebUI updates must never shorten or extend a
       pulse already in progress. */
    if (pwm_phase == 0U) {
        for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
            pwm_duty[i] = brightness[i];
        }
    }
    wad_phase = pwm_phase & (WAD_PWM_STEPS - 1U);
    /* Reuse the existing 8 kHz tick: eight steps give WADs a 1 kHz
       carrier. Latch duties only at a cycle boundary to avoid splitting
       a pulse when local settings change the requested brightness. */
    if (wad_phase == 0U) {
        wad_duty_left = (uint8_t)((brightness[BUTTON_WAD_LEFT] + 4U) >> 3);
        wad_duty_right = (uint8_t)((brightness[BUTTON_WAD_RIGHT] + 4U) >> 3);
    }
    if (wad_duty_left > wad_phase) mask |= (uint16_t)1U << BUTTON_WAD_LEFT;
    if (wad_duty_right > wad_phase) mask |= (uint16_t)1U << BUTTON_WAD_RIGHT;
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        if (i == BUTTON_WAD_LEFT || i == BUTTON_WAD_RIGHT) continue;
        if (pwm_duty[i] > pwm_phase) {
            mask |= (uint16_t)1U << i;
        }
    }
    hardware_write_button_leds(mask);
    pwm_phase = (uint8_t)((pwm_phase + 1U) & (PWM_STEPS - 1U));
}

uint8_t lighting_mode(void)
{
    return mode;
}

void lighting_apply_settings(uint8_t effect, uint8_t value, uint8_t speed,
                             uint8_t reactive_mode, uint8_t off_level,
                             uint8_t on_level)
{
    configured_effect = effect;
    configured_value = value;
    configured_speed = speed;
    configured_reactive = reactive_mode;
    configured_off_level = off_level;
    configured_on_level = on_level;
    host_levels_mask = 0;
}

void lighting_set_host_levels(const uint8_t levels[12])
{
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        host_levels[i] = levels[i];
    }
    /* Direct control owns only physical button/menu lamps. WADs remain
       reactive, while Test and Service have no host-controlled lamps. */
    host_levels_mask = DIRECT_LIGHT_MASK;
}

void lighting_clear_host_levels(void)
{
    host_levels_mask = 0;
}

void lighting_set_mode(uint8_t new_mode)
{
    if (new_mode < LIGHT_MODE_COUNT) {
        mode = new_mode;
    }
}

uint8_t lighting_take_mode_changed(void)
{
    const uint8_t result = mode_changed;
    mode_changed = 0;
    return result;
}
