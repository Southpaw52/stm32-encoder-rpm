/** @author southpaw | Project application code. */
/**
 * @file    test_main.c
 * @brief   Sequential host test runner and result accounting.
 * @details Invokes the math, monitor, telemetry, and scenario suites on the host.
 *          Reports each case and returns failure if any case fails.
 */
#include "test_framework.h"

#include <stdlib.h>

static unsigned int passed;
static unsigned int failed;

void test_run(const char *name, bool (*function)(void))
{
    const bool success = function();
    if (success) {
        ++passed;
    } else {
        ++failed;
    }
    printf("%s %s\n", success ? "PASS" : "FAIL", name);
}

/** @brief Host test entry. @return Zero iff all cases pass.
 * @note Context: host main only.
 */
int main(void)
{
    test_encoder_math();
    test_speed_monitor();
    test_telemetry();
    test_scenarios();
    printf("Summary: %u passed, %u failed\n", passed, failed);
    return (failed == 0U) ? EXIT_SUCCESS : EXIT_FAILURE;
}
