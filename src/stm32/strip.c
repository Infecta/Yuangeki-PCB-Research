#include "board.h"
#include "strip.h"

#define STRIP_BYTES_PER_PIXEL 3U
#define STRIP_FRAME_BYTES (STRIP_MAX_PIXELS * STRIP_BYTES_PER_PIXEL)
#define STRIP_FRAME_PERIOD_MS 20U

static uint8_t pixel_frame[STRIP_FRAME_BYTES];
static uint8_t configured_effect;
static uint8_t configured_brightness;
static uint8_t configured_speed;
static uint8_t configured_red;
static uint8_t configured_green;
static uint8_t configured_blue;
static uint8_t configured_color_order;
static uint8_t configured_pixel_count;
static uint8_t frame_divider;
static uint8_t animation_phase;

static uint8_t scale(uint8_t value)
{
    return (uint8_t)(((uint16_t)value * configured_brightness + 127U) / 255U);
}

static void wheel(uint8_t position, uint8_t *red, uint8_t *green, uint8_t *blue)
{
    if (position < 85U) {
        *red = (uint8_t)(255U - position * 3U);
        *green = (uint8_t)(position * 3U);
        *blue = 0;
    } else if (position < 170U) {
        position = (uint8_t)(position - 85U);
        *red = 0;
        *green = (uint8_t)(255U - position * 3U);
        *blue = (uint8_t)(position * 3U);
    } else {
        position = (uint8_t)(position - 170U);
        *red = (uint8_t)(position * 3U);
        *green = 0;
        *blue = (uint8_t)(255U - position * 3U);
    }
}

static void render_frame(void)
{
    uint8_t *destination = pixel_frame;
    for (uint8_t pixel = 0; pixel < configured_pixel_count; ++pixel) {
        uint8_t red = configured_red;
        uint8_t green = configured_green;
        uint8_t blue = configured_blue;
        if (configured_effect == 0U) {
            red = green = blue = 0;
        } else if (configured_effect == 2U) {
            wheel((uint8_t)(animation_phase +
                  ((uint16_t)pixel * 256U) / configured_pixel_count),
                  &red, &green, &blue);
        } else if (configured_effect == 3U) {
            const uint8_t active = (uint8_t)(animation_phase >> 4) % configured_pixel_count;
            if (pixel != active) red = green = blue = 0;
        }
        destination[0] = scale(configured_color_order == 0U ? green : red);
        destination[1] = scale(configured_color_order == 0U ? red : green);
        destination[2] = scale(blue);
        destination += STRIP_BYTES_PER_PIXEL;
    }
    hardware_write_addressable(pixel_frame,
        (uint16_t)configured_pixel_count * STRIP_BYTES_PER_PIXEL);
}

void strip_init(void)
{
    configured_effect = 0;
    configured_brightness = 64U;
    configured_speed = 64U;
    configured_red = 0;
    configured_green = 160U;
    configured_blue = 255U;
    configured_color_order = 0;
    configured_pixel_count = 16U;
    frame_divider = 0;
    animation_phase = 0;
    render_frame();
}

void strip_tick_1khz(void)
{
    /* Addressable pixels retain their value. Static off/solid frames must not
       be retransmitted continuously; doing so only adds UART-blocking time and
       makes any marginal electrical edge look like visible flicker. */
    if (configured_effect < 2U) return;
    if (++frame_divider < STRIP_FRAME_PERIOD_MS) return;
    frame_divider = 0;
    animation_phase = (uint8_t)(animation_phase + 1U + (configured_speed >> 5));
    render_frame();
}

void strip_apply_settings(uint8_t effect, uint8_t brightness, uint8_t speed,
                          uint8_t red, uint8_t green, uint8_t blue,
                          uint8_t color_order, uint8_t pixel_count)
{
    configured_effect = effect <= 3U ? effect : 0U;
    configured_brightness = brightness;
    configured_speed = speed;
    configured_red = red;
    configured_green = green;
    configured_blue = blue;
    configured_color_order = color_order != 0U ? 1U : 0U;
    configured_pixel_count = (pixel_count >= 1U && pixel_count <= STRIP_MAX_PIXELS) ?
                             pixel_count : 16U;
    render_frame();
}

#ifdef YTG_UNIT_TEST
const uint8_t *strip_encoded_for_test(void) { return pixel_frame; }
uint16_t strip_encoded_length_for_test(void) { return STRIP_FRAME_BYTES; }
#endif
