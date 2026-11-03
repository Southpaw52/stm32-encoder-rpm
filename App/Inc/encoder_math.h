/** @author southpaw | Project application code. */
/**
 * @file    encoder_math.h
 * @brief   Portable counter, speed, direction, and timer calculations.
 * @details Declares pure calculations shared by firmware ISR/main and host tests.
 *          Signed modular deltas and integer RPM keep hardware access separate.
 */
#ifndef ENCODER_MATH_H
#define ENCODER_MATH_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ENCODER_DIR_STOP = 0,
    ENCODER_DIR_CW,
    ENCODER_DIR_CCW
} encoder_direction_t;

/** @brief Compute signed displacement across either 32-bit counter wrap.
 * @param previous Previous raw TIM2 counter. @param current Current raw counter.
 * @return Signed modular change; valid only when the true magnitude is < 2^31.
 * At a one-second window, 2^31 counts/s is far beyond any intended encoder.
 * Exactly half the counter range is ambiguous and maps to INT32_MIN.
 * @note Context: any, including ISR; no division or HAL dependencies.
 */
int32_t encoder_count_delta(uint32_t previous, uint32_t current);

/** @brief Convert signed net displacement to centi-RPM using integer arithmetic.
 * @param delta_counts Net change in a window. @param counts_per_rev Counts/rev.
 * @param window_ms Window duration in milliseconds.
 * @return Signed centi-RPM, rounded half away from zero and saturated to int32;
 * zero if either denominator argument is zero.
 * @note Context: main/host; the 64-bit division is excluded from the ISR path.
 */
int32_t encoder_rpm_x100(int32_t delta_counts, uint32_t counts_per_rev,
                         uint32_t window_ms);

/** @brief Classify direction from net displacement, including INT32_MIN.
 * @param delta_counts Signed net window change. @param stop_threshold Deadband.
 * @return STOP when magnitude <= threshold, otherwise CW (+) or CCW (-).
 * @note Context: any. Direction is net displacement, not last edge direction.
 */
encoder_direction_t encoder_direction_from_delta(int32_t delta_counts,
                                                  uint32_t stop_threshold);

/** @brief Obtain a stable printable direction name.
 * @param dir Direction enum. @return CW, CCW, STOP, or ? for an unknown value.
 * @note Context: any; returned string is immutable and has static lifetime.
 */
const char *encoder_direction_name(encoder_direction_t dir);

/** @brief Check an exact timer period without truncation or overflowing uint64.
 * @param timer_clock_hz Timer input frequency. @param prescaler PSC register.
 * @param period ARR register. @param expected_window_ms Required period in ms.
 * @return True iff (PSC+1)*(ARR+1)*1000 equals window_ms*timer_clock_hz.
 * @note Context: main/host. Overflowing left products cannot equal the right.
 */
bool encoder_timebase_period_matches(uint32_t timer_clock_hz, uint32_t prescaler,
                                      uint32_t period, uint32_t expected_window_ms);

/** @brief Apply the configured STM32F4 APB timer-clock rule.
 * @param pclk_hz APB clock in Hz (within the STM32F407 operating range).
 * @param apb_prescaler_divisor Actual APB divisor, one of 1, 2, 4, 8, 16.
 * @return PCLK for divisor 1, twice PCLK otherwise.
 * @note Context: main/host; caller supplies a valid F407 clock configuration.
 */
uint32_t encoder_apb_timer_clock_hz(uint32_t pclk_hz, uint32_t apb_prescaler_divisor);

#endif
