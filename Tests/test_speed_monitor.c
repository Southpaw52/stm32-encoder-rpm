/** @author southpaw | Project application code. */
/**
 * @file    test_speed_monitor.c
 * @brief   Host coverage of ISR-to-main sample semantics.
 * @details Models sequential window production and main-context sample consumption.
 *          Checks skipped windows, accumulated position, and sequence wrap without IRQs.
 */
#include "test_framework.h"

#include "speed_monitor.h"
#include <limits.h>

static bool no_sample_before_first_window_and_single_consumption(void)
{
    speed_monitor_t monitor;
    speed_sample_t sample = {99U, 99, 99};
    uint32_t missed = 99U;
    speed_monitor_init(&monitor, 100U);
    CHECK(!speed_monitor_fetch(&monitor, &sample, &missed));
    CHECK(sample.sequence == 99U && sample.total_counts == 99 && sample.delta_counts == 99);
    CHECK(missed == 99U);
    speed_monitor_on_window_elapsed(&monitor, 500U);
    CHECK(speed_monitor_fetch(&monitor, &sample, &missed));
    CHECK(sample.sequence == 1U && sample.total_counts == 400 && sample.delta_counts == 400);
    CHECK(missed == 0U);
    CHECK(!speed_monitor_fetch(&monitor, &sample, &missed));
    return true;
}

static bool missed_windows_keep_total_and_latest_delta(void)
{
    speed_monitor_t monitor;
    speed_sample_t sample;
    uint32_t missed;
    speed_monitor_init(&monitor, 0U);
    speed_monitor_on_window_elapsed(&monitor, 400U);
    speed_monitor_on_window_elapsed(&monitor, 800U);
    speed_monitor_on_window_elapsed(&monitor, 700U);
    CHECK(speed_monitor_fetch(&monitor, &sample, &missed));
    CHECK(sample.sequence == 3U && sample.total_counts == 700 && sample.delta_counts == -100);
    CHECK(missed == 2U);
    speed_monitor_on_window_elapsed(&monitor, 700U);
    CHECK(speed_monitor_fetch(&monitor, &sample, &missed));
    CHECK(sample.sequence == 4U && sample.total_counts == 700 && sample.delta_counts == 0);
    CHECK(missed == 0U);
    speed_monitor_on_window_elapsed(&monitor, 900U);
    speed_monitor_on_window_elapsed(&monitor, 1000U);
    CHECK(speed_monitor_fetch(&monitor, &sample, &missed));
    CHECK(sample.total_counts == 1000 && missed == 1U);
    return true;
}

static bool accumulated_position_beyond_int32_and_uint32(void)
{
    speed_monitor_t monitor;
    speed_sample_t sample;
    uint32_t raw = 0U;
    speed_monitor_init(&monitor, raw);
    for (unsigned int index = 0U; index < 3U; ++index) {
        raw += (uint32_t)INT32_MAX;
        speed_monitor_on_window_elapsed(&monitor, raw);
    }
    uint32_t missed;
    CHECK(speed_monitor_fetch(&monitor, &sample, &missed));
    CHECK(sample.total_counts == INT64_C(6442450941));
    CHECK(sample.delta_counts == INT32_MAX && missed == 2U);
    speed_monitor_init(&monitor, 0U);
    raw = 0U;
    for (unsigned int index = 0U; index < 3U; ++index) {
        raw -= (uint32_t)INT32_MAX;
        speed_monitor_on_window_elapsed(&monitor, raw);
    }
    CHECK(speed_monitor_fetch(&monitor, &sample, NULL));
    CHECK(sample.total_counts == -INT64_C(6442450941));
    CHECK(sample.delta_counts == -INT32_MAX);
    return true;
}

static bool sequence_wrap_preserves_ready_and_missed_arithmetic(void)
{
    speed_monitor_t monitor;
    speed_sample_t sample;
    uint32_t missed;
    speed_monitor_init(&monitor, 0U);
    /* Model an already-running system near its 136-year sequence wrap. */
    monitor.latest.sequence = UINT32_MAX - 1U;
    monitor.last_fetched_sequence = UINT32_MAX - 1U;
    speed_monitor_on_window_elapsed(&monitor, 1U);
    speed_monitor_on_window_elapsed(&monitor, 2U);
    CHECK(speed_monitor_fetch(&monitor, &sample, &missed));
    CHECK(sample.sequence == 0U && sample.total_counts == 2 && missed == 1U);
    return true;
}

static bool reinitialization_clears_pending_sample(void)
{
    speed_monitor_t monitor;
    speed_sample_t sample;
    speed_monitor_init(&monitor, 0U);
    speed_monitor_on_window_elapsed(&monitor, 400U);
    speed_monitor_init(&monitor, UINT32_MAX);
    CHECK(!speed_monitor_fetch(&monitor, &sample, NULL));
    speed_monitor_on_window_elapsed(&monitor, 0U);
    CHECK(speed_monitor_fetch(&monitor, &sample, NULL));
    CHECK(sample.sequence == 1U && sample.total_counts == 1 && sample.delta_counts == 1);
    return true;
}

void test_speed_monitor(void)
{
    RUN_TEST(no_sample_before_first_window_and_single_consumption);
    RUN_TEST(missed_windows_keep_total_and_latest_delta);
    RUN_TEST(accumulated_position_beyond_int32_and_uint32);
    RUN_TEST(sequence_wrap_preserves_ready_and_missed_arithmetic);
    RUN_TEST(reinitialization_clears_pending_sample);
}
