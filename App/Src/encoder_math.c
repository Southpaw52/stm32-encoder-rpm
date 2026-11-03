/** @author southpaw | Project application code. */
/**
 * @file    encoder_math.c
 * @brief   Portable integer encoder and timer calculations.
 * @details Runs without HAL in firmware ISR/main context and host tests.
 *          Explicit range checks handle wrap, rounding, saturation, and overflow.
 */
#include "encoder_math.h"

#include "encoder_config.h"
#include <limits.h>

int32_t encoder_count_delta(uint32_t previous, uint32_t current)
{
    const uint32_t difference = current - previous;
    if (difference <= (uint32_t)INT32_MAX) {
        return (int32_t)difference;
    }
    /* Both casts are representable; no implementation-defined unsigned cast. */
    return -(int32_t)(UINT32_MAX - difference) - 1;
}

int32_t encoder_rpm_x100(int32_t delta_counts, uint32_t counts_per_rev,
                         uint32_t window_ms)
{
    if ((counts_per_rev == 0U) || (window_ms == 0U)) {
        return 0;
    }
    /* Largest numerator is 2^31 * 6,000,000, safely within int64_t. */
    const int64_t numerator = (int64_t)delta_counts * ENCODER_SECONDS_PER_MINUTE
                            * ENCODER_CENTI_RPM_SCALE * ENCODER_MS_PER_SECOND;
    const uint64_t magnitude = (uint64_t)((numerator < 0) ? -numerator : numerator);
    const uint64_t denominator = (uint64_t)counts_per_rev * window_ms;
    uint64_t result = magnitude / denominator;
    const uint64_t remainder = magnitude % denominator;
    /* Compare without doubling the remainder or adding half the denominator. */
    if (remainder >= denominator - remainder) {
        ++result;
    }
    if (numerator < 0) {
        if (result >= (uint64_t)INT32_MAX + 1U) {
            return INT32_MIN;
        }
        return -(int32_t)result;
    }
    return (result > (uint64_t)INT32_MAX) ? INT32_MAX : (int32_t)result;
}

encoder_direction_t encoder_direction_from_delta(int32_t delta_counts,
                                                  uint32_t stop_threshold)
{
    const uint32_t magnitude = (uint32_t)((delta_counts < 0)
                                ? -(int64_t)delta_counts : (int64_t)delta_counts);
    if (magnitude <= stop_threshold) {
        return ENCODER_DIR_STOP;
    }
    return (delta_counts > 0) ? ENCODER_DIR_CW : ENCODER_DIR_CCW;
}

const char *encoder_direction_name(encoder_direction_t dir)
{
    switch (dir) {
    case ENCODER_DIR_STOP: return "STOP";
    case ENCODER_DIR_CW:   return "CW";
    case ENCODER_DIR_CCW:  return "CCW";
    default:              return "?";
    }
}

bool encoder_timebase_period_matches(uint32_t timer_clock_hz, uint32_t prescaler,
                                      uint32_t period, uint32_t expected_window_ms)
{
    const uint64_t prescaler_ticks = (uint64_t)prescaler + 1U;
    const uint64_t period_ticks = (uint64_t)period + 1U;
    if (prescaler_ticks > UINT64_MAX / period_ticks) {
        return false;
    }
    const uint64_t ticks = prescaler_ticks * period_ticks;
    if (ticks > UINT64_MAX / ENCODER_MS_PER_SECOND) {
        return false;
    }
    return ticks * ENCODER_MS_PER_SECOND
        == (uint64_t)expected_window_ms * timer_clock_hz;
}

uint32_t encoder_apb_timer_clock_hz(uint32_t pclk_hz, uint32_t apb_prescaler_divisor)
{
    const uint32_t apb_timer_multiplier = 2U;
    return (apb_prescaler_divisor == 1U) ? pclk_hz : pclk_hz * apb_timer_multiplier;
}
