/** @author southpaw | Project application code. */
/**
 * @file    test_encoder_math.c
 * @brief   Host coverage of portable encoder arithmetic.
 * @details Exercises wrap, RPM rounding and limits, direction, and timer validation.
 *          Boundary cases check calculations independently of STM32 hardware.
 */
#include "test_framework.h"

#include "encoder_config.h"
#include "encoder_math.h"
#include <limits.h>

static bool normal_rotation_400_and_10000_counts(void)
{
    CHECK(encoder_rpm_x100(400, ENCODER_COUNTS_PER_REV, SPEED_WINDOW_MS) == 6000);
    CHECK(encoder_rpm_x100(10000, ENCODER_COUNTS_PER_REV, SPEED_WINDOW_MS) == 150000);
    CHECK(encoder_direction_from_delta(400, 0U) == ENCODER_DIR_CW);
    CHECK(encoder_rpm_x100(1, ENCODER_COUNTS_PER_REV, SPEED_WINDOW_MS) == 15);
    return true;
}

static bool reverse_rotation_and_stop(void)
{
    CHECK(encoder_rpm_x100(-400, ENCODER_COUNTS_PER_REV, SPEED_WINDOW_MS) == -6000);
    CHECK(encoder_direction_from_delta(-400, 0U) == ENCODER_DIR_CCW);
    CHECK(encoder_rpm_x100(0, ENCODER_COUNTS_PER_REV, SPEED_WINDOW_MS) == 0);
    CHECK(encoder_direction_from_delta(0, 0U) == ENCODER_DIR_STOP);
    return true;
}

static bool counter_wrap_both_directions_and_sign_boundary(void)
{
    CHECK(encoder_count_delta(UINT32_C(0xFFFFFF00), UINT32_C(0x00000100)) == 512);
    CHECK(encoder_count_delta(UINT32_C(0x00000010), UINT32_C(0xFFFFFFF0)) == -32);
    CHECK(encoder_count_delta(UINT32_C(0x7FFFFFFF), UINT32_C(0x80000000)) == 1);
    CHECK(encoder_count_delta(UINT32_C(0x80000000), UINT32_C(0x7FFFFFFF)) == -1);
    CHECK(encoder_count_delta(0U, UINT32_MAX) == -1);
    CHECK(encoder_count_delta(UINT32_MAX, 0U) == 1);
    CHECK(encoder_count_delta(0U, (uint32_t)INT32_MAX) == INT32_MAX);
    CHECK(encoder_count_delta(0U, UINT32_C(0x80000000)) == INT32_MIN);
    CHECK(encoder_count_delta(UINT32_MAX, UINT32_MAX) == 0);
    return true;
}

static bool rpm_rounding_half_away_from_zero(void)
{
    /* 6,000,000 / 1,024,000 = 5.859375 centi-RPM -> 6. */
    CHECK(encoder_rpm_x100(1, 1024U, 1000U) == 6);
    CHECK(encoder_rpm_x100(-1, 1024U, 1000U) == -6);
    CHECK(encoder_rpm_x100(2, 1024U, 1000U) == 12);
    CHECK(encoder_rpm_x100(3, 1024U, 1000U) == 18);
    /* Exactly +/-1.5 centi-RPM must round away from zero. */
    CHECK(encoder_rpm_x100(1, 4000U, 1000U) == 2);
    CHECK(encoder_rpm_x100(-1, 4000U, 1000U) == -2);
    CHECK(encoder_rpm_x100(1, 12000U, 1000U) == 1);
    CHECK(encoder_rpm_x100(-1, 12000U, 1000U) == -1);
    CHECK(encoder_rpm_x100(1, 12001U, 1000U) == 0);
    CHECK(encoder_rpm_x100(-1, 12001U, 1000U) == 0);
    return true;
}

static bool rpm_saturation_zero_guards_and_large_denominator(void)
{
    CHECK(encoder_rpm_x100(INT32_MAX, 1U, 1U) == INT32_MAX);
    CHECK(encoder_rpm_x100(INT32_MIN, 1U, 1U) == INT32_MIN);
    CHECK(encoder_rpm_x100(INT32_MAX, 6000U, 1000U) == INT32_MAX);
    CHECK(encoder_rpm_x100(INT32_MIN, 6000U, 1000U) == INT32_MIN);
    CHECK(encoder_rpm_x100(400, 0U, 1000U) == 0);
    CHECK(encoder_rpm_x100(-400, 400U, 0U) == 0);
    CHECK(encoder_rpm_x100(1, 0U, 0U) == 0);
    CHECK(encoder_rpm_x100(INT32_MIN, UINT32_MAX, UINT32_MAX) == 0);
    return true;
}

static bool stop_threshold_inclusive_and_int32_min(void)
{
    CHECK(encoder_direction_from_delta(3, 3U) == ENCODER_DIR_STOP);
    CHECK(encoder_direction_from_delta(-3, 3U) == ENCODER_DIR_STOP);
    CHECK(encoder_direction_from_delta(4, 3U) == ENCODER_DIR_CW);
    CHECK(encoder_direction_from_delta(-4, 3U) == ENCODER_DIR_CCW);
    CHECK(encoder_direction_from_delta(INT32_MIN, (uint32_t)INT32_MAX) == ENCODER_DIR_CCW);
    CHECK(encoder_direction_from_delta(INT32_MIN, UINT32_C(0x80000000)) == ENCODER_DIR_STOP);
    CHECK(encoder_direction_from_delta(INT32_MAX, UINT32_MAX) == ENCODER_DIR_STOP);
    return true;
}

static bool direction_names_including_unknown(void)
{
    CHECK_STRING(encoder_direction_name(ENCODER_DIR_CW), "CW");
    CHECK_STRING(encoder_direction_name(ENCODER_DIR_CCW), "CCW");
    CHECK_STRING(encoder_direction_name(ENCODER_DIR_STOP), "STOP");
    CHECK_STRING(encoder_direction_name((encoder_direction_t)99), "?");
    return true;
}

static bool timebase_exact_off_by_one_and_overflow(void)
{
    CHECK(encoder_timebase_period_matches(84000000U, 8399U, 9999U, 1000U));
    CHECK(!encoder_timebase_period_matches(84000000U, 8400U, 9999U, 1000U));
    CHECK(!encoder_timebase_period_matches(84000000U, 8399U, 10000U, 1000U));
    CHECK(!encoder_timebase_period_matches(84000001U, 8399U, 9999U, 1000U));
    CHECK(!encoder_timebase_period_matches(84000000U, 8399U, 9999U, 1001U));
    CHECK(!encoder_timebase_period_matches(0U, 8399U, 9999U, 1000U));
    CHECK(!encoder_timebase_period_matches(84000000U, 8399U, 9999U, 0U));
    CHECK(!encoder_timebase_period_matches(UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX));
    CHECK(!encoder_timebase_period_matches(UINT32_MAX, UINT32_MAX, UINT32_MAX - 1U, UINT32_MAX));
    CHECK(encoder_timebase_period_matches(1000U, 0U, 0U, 1U));
    return true;
}

static bool apb_clock_div1_and_divided(void)
{
    CHECK(encoder_apb_timer_clock_hz(84000000U, 1U) == 84000000U);
    CHECK(encoder_apb_timer_clock_hz(42000000U, 2U) == 84000000U);
    CHECK(encoder_apb_timer_clock_hz(21000000U, 4U) == 42000000U);
    CHECK(encoder_apb_timer_clock_hz(10500000U, 8U) == 21000000U);
    CHECK(encoder_apb_timer_clock_hz(5250000U, 16U) == 10500000U);
    return true;
}

void test_encoder_math(void)
{
    RUN_TEST(normal_rotation_400_and_10000_counts);
    RUN_TEST(reverse_rotation_and_stop);
    RUN_TEST(counter_wrap_both_directions_and_sign_boundary);
    RUN_TEST(rpm_rounding_half_away_from_zero);
    RUN_TEST(rpm_saturation_zero_guards_and_large_denominator);
    RUN_TEST(stop_threshold_inclusive_and_int32_min);
    RUN_TEST(direction_names_including_unknown);
    RUN_TEST(timebase_exact_off_by_one_and_overflow);
    RUN_TEST(apb_clock_div1_and_divided);
}
