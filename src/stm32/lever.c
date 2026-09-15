#include "lever.h"

/* Median-of-three removes isolated ADC spikes. The adaptive fixed-point EMA
   holds within two ADC counts, moves by 1/8 normally, and catches up by 1/2
   during fast lever movement. */
#define JITTER_HOLD_COUNTS 2U
#define FAST_MOVE_COUNTS 64U

static uint16_t raw_value;
static uint32_t filtered_q8;
static uint16_t history[3];
static uint8_t history_index;

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) { const uint16_t t = a; a = b; b = t; }
    if (b > c) { const uint16_t t = b; b = c; c = t; }
    if (a > b) { const uint16_t t = a; a = b; b = t; }
    return b;
}

void lever_init(uint16_t initial_sample)
{
    raw_value = initial_sample;
    filtered_q8 = (uint32_t)initial_sample << 8;
    history[0] = initial_sample;
    history[1] = initial_sample;
    history[2] = initial_sample;
    history_index = 0;
}

void lever_update(uint16_t sample)
{
    raw_value = sample;
    history[history_index] = sample;
    history_index = (uint8_t)((history_index + 1U) % 3U);
    const uint32_t target = (uint32_t)median3(history[0], history[1], history[2]) << 8;
    const uint32_t difference = target >= filtered_q8 ?
                                target - filtered_q8 : filtered_q8 - target;
    if (difference <= ((uint32_t)JITTER_HOLD_COUNTS << 8)) {
        return;
    }
    const uint8_t shift = difference >= ((uint32_t)FAST_MOVE_COUNTS << 8) ? 1U : 3U;
    if (target >= filtered_q8) {
        filtered_q8 += difference >> shift;
    } else {
        filtered_q8 -= difference >> shift;
    }
}

uint16_t lever_raw(void)
{
    return raw_value;
}

uint16_t lever_filtered(void)
{
    return (uint16_t)((filtered_q8 + 128U) >> 8);
}

uint8_t lever_raw_legacy(void)
{
    return (uint8_t)(raw_value >> 4);
}

uint8_t lever_filtered_legacy(void)
{
    return (uint8_t)(lever_filtered() >> 4);
}
