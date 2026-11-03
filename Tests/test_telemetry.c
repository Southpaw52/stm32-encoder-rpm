/** @author southpaw | Project application code. */
/**
 * @file    test_telemetry.c
 * @brief   Host coverage of telemetry content and buffer bounds.
 * @details Checks exact banner and sample strings, integer extremes, and capacities.
 *          Canaries and empty-result checks detect overflow or partial-line output.
 */
#include "test_framework.h"

#include "encoder_config.h"
#include "telemetry.h"
#include <limits.h>

static bool banner_exact_and_failure_is_empty(void)
{
    char buffer[TELEMETRY_LINE_MAX];
    const char *expected = "# encoder_rpm fw=1.0.0 ppr=100 cpr=400 window_ms=1000\r\n";
    CHECK(telemetry_format_banner(buffer, sizeof(buffer)) == strlen(expected));
    CHECK_STRING(buffer, expected);
    CHECK(telemetry_format_banner(buffer, strlen(expected)) == 0U);
    CHECK(buffer[0] == '\0');
    CHECK(telemetry_format_banner(NULL, 0U) == 0U);
    return true;
}

static bool sample_exact_specification_lines(void)
{
    char buffer[TELEMETRY_LINE_MAX];
    const struct {
        speed_sample_t sample;
        int32_t rpm;
        encoder_direction_t direction;
        const char *expected;
    } cases[] = {
        {{1U, 400, 400}, 6000, ENCODER_DIR_CW,
         "t=1s pulses=+400 delta=+400 dir=CW rpm=60.00\r\n"},
        {{7U, -1200, -400}, -6000, ENCODER_DIR_CCW,
         "t=7s pulses=-1200 delta=-400 dir=CCW rpm=60.00\r\n"},
        {{9U, 800, 0}, 0, ENCODER_DIR_STOP,
         "t=9s pulses=+800 delta=0 dir=STOP rpm=0.00\r\n"},
        {{10U, 815, 15}, 225, ENCODER_DIR_CW,
         "t=10s pulses=+815 delta=+15 dir=CW rpm=2.25\r\n"},
        {{1U, 0, 0}, 0, ENCODER_DIR_STOP,
         "t=1s pulses=0 delta=0 dir=STOP rpm=0.00\r\n"},
        {{2U, -1, -1}, -1, ENCODER_DIR_CCW,
         "t=2s pulses=-1 delta=-1 dir=CCW rpm=0.01\r\n"},
        {{3U, 10000, 10000}, 150000, ENCODER_DIR_CW,
         "t=3s pulses=+10000 delta=+10000 dir=CW rpm=1500.00\r\n"}
    };
    for (size_t index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        CHECK(telemetry_format_sample(buffer, sizeof(buffer), &cases[index].sample,
              cases[index].rpm, cases[index].direction, 0U) == strlen(cases[index].expected));
        CHECK_STRING(buffer, cases[index].expected);
    }
    return true;
}

static bool formatter_int64_and_int32_extremes_and_missed(void)
{
    char buffer[TELEMETRY_LINE_MAX];
    speed_sample_t sample = {UINT32_MAX, INT64_MIN, INT32_MIN};
    const char *expected = "t=4294967295s pulses=-9223372036854775808 delta=-2147483648"
                           " dir=STOP rpm=21474836.48 missed=4294967295\r\n";
    CHECK(telemetry_format_sample(buffer, sizeof(buffer), &sample, INT32_MIN,
                                  ENCODER_DIR_STOP, UINT32_MAX) == strlen(expected));
    CHECK_STRING(buffer, expected);
    CHECK(strlen(buffer) + 1U == TELEMETRY_WORST_CASE_BYTES);
    sample.total_counts = INT64_MAX;
    sample.delta_counts = INT32_MAX;
    expected = "t=4294967295s pulses=+9223372036854775807 delta=+2147483647"
               " dir=CW rpm=21474836.47 missed=1\r\n";
    CHECK(telemetry_format_sample(buffer, sizeof(buffer), &sample, INT32_MAX,
                                  ENCODER_DIR_CW, 1U) == strlen(expected));
    CHECK_STRING(buffer, expected);
    return true;
}

static bool formatter_all_capacities_exact_fit_and_canaries(void)
{
    speed_sample_t sample = {1U, 400, 400};
    const char *expected = "t=1s pulses=+400 delta=+400 dir=CW rpm=60.00\r\n";
    struct {
        char before;
        char buffer[TELEMETRY_LINE_MAX];
        char after;
    } guarded;
    for (size_t capacity = 0U; capacity <= strlen(expected); ++capacity) {
        memset(&guarded, 'X', sizeof(guarded));
        CHECK(telemetry_format_sample(guarded.buffer, capacity, &sample, 6000,
                                      ENCODER_DIR_CW, 0U) == 0U);
        CHECK(guarded.before == 'X' && guarded.after == 'X');
        CHECK(guarded.buffer[capacity] == 'X');
        CHECK(guarded.buffer[0] == ((capacity == 0U) ? 'X' : '\0'));
    }
    CHECK(telemetry_format_sample(guarded.buffer, strlen(expected) + 1U,
                                  &sample, 6000, ENCODER_DIR_CW, 0U) == strlen(expected));
    CHECK_STRING(guarded.buffer, expected);
    CHECK(telemetry_format_sample(NULL, 0U, &sample, 0, ENCODER_DIR_STOP, 0U) == 0U);
    return true;
}

static bool formatter_unknown_direction_and_optional_missed(void)
{
    char buffer[TELEMETRY_LINE_MAX];
    speed_sample_t sample = {3U, 10, -1};
    const char *expected = "t=3s pulses=+10 delta=-1 dir=? rpm=0.15 missed=2\r\n";
    CHECK(telemetry_format_sample(buffer, sizeof(buffer), &sample, -15,
                                  (encoder_direction_t)99, 2U) == strlen(expected));
    CHECK_STRING(buffer, expected);
    return true;
}

void test_telemetry(void)
{
    RUN_TEST(banner_exact_and_failure_is_empty);
    RUN_TEST(sample_exact_specification_lines);
    RUN_TEST(formatter_int64_and_int32_extremes_and_missed);
    RUN_TEST(formatter_all_capacities_exact_fit_and_canaries);
    RUN_TEST(formatter_unknown_direction_and_optional_missed);
}
