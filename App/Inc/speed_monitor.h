/** @author southpaw | Project application code. */
/**
 * @file    speed_monitor.h
 * @brief   Latest-window samples shared by ISR and main context.
 * @details Defines the single-producer ISR and single-consumer main interface.
 *          Every window contributes to position even when older samples are skipped.
 */
#ifndef SPEED_MONITOR_H
#define SPEED_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t sequence;     /**< First completed window is 1; wraps modulo 2^32. */
    int64_t total_counts;  /**< Signed position since startup. */
    int32_t delta_counts;  /**< Signed net displacement of the latest window. */
} speed_sample_t;

typedef struct {
    volatile uint32_t previous_counter;
    volatile speed_sample_t latest;
    volatile bool ready;
    uint32_t last_fetched_sequence; /**< Main context only. */
} speed_monitor_t;

/** @brief Initialize a monitor before starting its producer interrupt.
 * @param mon Valid monitor storage. @param initial_counter Initial raw counter.
 * @return Nothing. No sample exists until the first completed window.
 * @note Context: main, with the producer stopped; total starts at zero.
 */
void speed_monitor_init(speed_monitor_t *mon, uint32_t initial_counter);

/** @brief Record one completed window without HAL, division, or formatting.
 * @param mon Initialized monitor. @param counter_now Current raw TIM2 count.
 * @return Nothing. Every delta is accumulated even if main misses samples.
 * @note Context: single producer ISR only. True |delta| must be < 2^31 and
 * accumulated position must fit int64_t (effectively unlimited practical use).
 */
void speed_monitor_on_window_elapsed(speed_monitor_t *mon, uint32_t counter_now);

/** @brief Atomically consume the latest completed window.
 * @param mon Initialized monitor. @param out Valid destination sample.
 * @param missed_windows Optional output for unfetched intervening windows.
 * @return True on a new sample; false leaves output arguments unchanged.
 * @note Context: single main consumer. PRIMASK prevents the sole producer from
 * interrupting the 64-bit copy or ready flag update on this single-core target.
 * Save/restore preserves a caller's already-disabled interrupts. Sequence and
 * missed arithmetic are modulo 2^32; fetch at least once per 2^32 windows.
 */
bool speed_monitor_fetch(speed_monitor_t *mon, speed_sample_t *out,
                         uint32_t *missed_windows);

#endif
