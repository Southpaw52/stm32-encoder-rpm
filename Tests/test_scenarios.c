/** @author southpaw | Project application code. */
/**
 * @file    test_scenarios.c
 * @brief   Host motion profiles spanning sampling and telemetry.
 * @details Feeds simulated counters through the monitor, math, and formatter.
 *          Checks net motion and skipped samples; writes UART examples to the build tree.
 */
#include "test_framework.h"

#include "encoder_config.h"
#include "encoder_math.h"
#include "speed_monitor.h"
#include "telemetry.h"

static bool format_pending(speed_monitor_t *monitor, char *line, size_t capacity,
                           uint32_t threshold, const char *expected)
{
    speed_sample_t sample;
    uint32_t missed;
    CHECK(speed_monitor_fetch(monitor, &sample, &missed));
    const encoder_direction_t direction = encoder_direction_from_delta(sample.delta_counts, threshold);
    const int32_t rpm = (direction == ENCODER_DIR_STOP) ? 0
        : encoder_rpm_x100(sample.delta_counts, ENCODER_COUNTS_PER_REV, SPEED_WINDOW_MS);
    CHECK(telemetry_format_sample(line, capacity, &sample, rpm, direction, missed) == strlen(expected));
    CHECK_STRING(line, expected);
    return true;
}

static bool motion_profile_accelerate_constant_stop_reverse_wrap_stop(void)
{
    /* The raw counter wraps modulo 2^32, just like TIM2. A reversal within a
     * window is intentionally observed only as its net displacement.
     */
    const struct {
        int32_t delta;
        const char *expected;
    } profile[] = {
        {100, "t=1s pulses=+100 delta=+100 dir=CW rpm=15.00\r\n"},
        {200, "t=2s pulses=+300 delta=+200 dir=CW rpm=30.00\r\n"},
        {400, "t=3s pulses=+700 delta=+400 dir=CW rpm=60.00\r\n"},
        {400, "t=4s pulses=+1100 delta=+400 dir=CW rpm=60.00\r\n"},
        {0, "t=5s pulses=+1100 delta=0 dir=STOP rpm=0.00\r\n"},
        {-400, "t=6s pulses=+700 delta=-400 dir=CCW rpm=60.00\r\n"},
        {-400, "t=7s pulses=+300 delta=-400 dir=CCW rpm=60.00\r\n"},
        {-300, "t=8s pulses=0 delta=-300 dir=CCW rpm=45.00\r\n"},
        {-1, "t=9s pulses=-1 delta=-1 dir=CCW rpm=0.15\r\n"},
        {-399, "t=10s pulses=-400 delta=-399 dir=CCW rpm=59.85\r\n"},
        {0, "t=11s pulses=-400 delta=0 dir=STOP rpm=0.00\r\n"}
    };
    speed_monitor_t monitor;
    uint32_t raw = 0U;
    speed_monitor_init(&monitor, raw);
    FILE *output = fopen(UART_SAMPLE_OUTPUT_PATH, "wb");
    CHECK(output != NULL);
    bool success = true;
    for (size_t index = 0U; index < sizeof(profile) / sizeof(profile[0]); ++index) {
        /* Signed-to-unsigned is defined modulo 2^32, including negative steps. */
        raw += (uint32_t)profile[index].delta;
        speed_monitor_on_window_elapsed(&monitor, raw);
        char line[TELEMETRY_LINE_MAX];
        if (!format_pending(&monitor, line, sizeof(line), SPEED_STOP_THRESHOLD_COUNTS,
                             profile[index].expected)) {
            success = false;
            break;
        }
        const size_t length = strlen(line);
        if (fwrite(line, 1U, length, output) != length) {
            success = false;
            break;
        }
    }
    const int close_status = fclose(output);
    CHECK(success && close_status == 0);
    return true;
}

static bool direction_change_and_within_window_net_displacement(void)
{
    speed_monitor_t monitor;
    uint32_t raw = 0U;
    char line[TELEMETRY_LINE_MAX];
    speed_monitor_init(&monitor, raw);
    raw += 400U;
    speed_monitor_on_window_elapsed(&monitor, raw);
    CHECK(format_pending(&monitor, line, sizeof(line), 0U,
        "t=1s pulses=+400 delta=+400 dir=CW rpm=60.00\r\n"));
    raw -= 400U;
    speed_monitor_on_window_elapsed(&monitor, raw);
    CHECK(format_pending(&monitor, line, sizeof(line), 0U,
        "t=2s pulses=0 delta=-400 dir=CCW rpm=60.00\r\n"));
    raw += 300U;
    raw -= 100U;
    speed_monitor_on_window_elapsed(&monitor, raw);
    CHECK(format_pending(&monitor, line, sizeof(line), 0U,
        "t=3s pulses=+200 delta=+200 dir=CW rpm=30.00\r\n"));
    return true;
}

static bool busy_consumer_skips_windows_without_losing_position(void)
{
    speed_monitor_t monitor;
    char line[TELEMETRY_LINE_MAX];
    speed_monitor_init(&monitor, 0U);
    speed_monitor_on_window_elapsed(&monitor, 400U);
    CHECK(format_pending(&monitor, line, sizeof(line), 0U,
        "t=1s pulses=+400 delta=+400 dir=CW rpm=60.00\r\n"));
    /* Simulated TX busy: main deliberately does not fetch two windows. */
    speed_monitor_on_window_elapsed(&monitor, 800U);
    speed_monitor_on_window_elapsed(&monitor, 1200U);
    speed_monitor_on_window_elapsed(&monitor, 1200U);
    CHECK(format_pending(&monitor, line, sizeof(line), 0U,
        "t=4s pulses=+1200 delta=0 dir=STOP rpm=0.00 missed=2\r\n"));
    return true;
}

static bool configured_deadband_forces_zero_rpm_but_retains_position(void)
{
    speed_monitor_t monitor;
    char line[TELEMETRY_LINE_MAX];
    speed_monitor_init(&monitor, 0U);
    speed_monitor_on_window_elapsed(&monitor, 3U);
    CHECK(format_pending(&monitor, line, sizeof(line), 3U,
        "t=1s pulses=+3 delta=+3 dir=STOP rpm=0.00\r\n"));
    speed_monitor_on_window_elapsed(&monitor, 0U);
    CHECK(format_pending(&monitor, line, sizeof(line), 3U,
        "t=2s pulses=0 delta=-3 dir=STOP rpm=0.00\r\n"));
    return true;
}

void test_scenarios(void)
{
    RUN_TEST(motion_profile_accelerate_constant_stop_reverse_wrap_stop);
    RUN_TEST(direction_change_and_within_window_net_displacement);
    RUN_TEST(busy_consumer_skips_windows_without_losing_position);
    RUN_TEST(configured_deadband_forces_zero_rpm_but_retains_position);
}
