/** @author southpaw | Project application code. */
/**
 * @file    speed_monitor.c
 * @brief   ISR sample publication and main-loop consumption.
 * @details Accumulates every counter delta while retaining only the latest window.
 *          A short interrupt-masked fetch protects the shared multiword snapshot.
 */
#include "speed_monitor.h"

#include "critical_section.h"
#include "encoder_math.h"
#include <stddef.h>

void speed_monitor_init(speed_monitor_t *mon, uint32_t initial_counter)
{
    mon->previous_counter = initial_counter;
    mon->latest.sequence = 0U;
    mon->latest.total_counts = 0;
    mon->latest.delta_counts = 0;
    mon->last_fetched_sequence = 0U;
    mon->ready = false;
}

void speed_monitor_on_window_elapsed(speed_monitor_t *mon, uint32_t counter_now)
{
    const int32_t delta = encoder_count_delta(mon->previous_counter, counter_now);
    mon->previous_counter = counter_now;
    mon->latest.total_counts += delta;
    mon->latest.delta_counts = delta;
    ++mon->latest.sequence;
    mon->ready = true;
}

bool speed_monitor_fetch(speed_monitor_t *mon, speed_sample_t *out,
                         uint32_t *missed_windows)
{
    const uint32_t state = critical_section_enter();
    if (!mon->ready) {
        critical_section_exit(state);
        return false;
    }
    out->sequence = mon->latest.sequence;
    out->total_counts = mon->latest.total_counts;
    out->delta_counts = mon->latest.delta_counts;
    const uint32_t missed = out->sequence - mon->last_fetched_sequence - 1U;
    mon->last_fetched_sequence = out->sequence;
    mon->ready = false;
    critical_section_exit(state);
    if (missed_windows != NULL) {
        *missed_windows = missed;
    }
    return true;
}
