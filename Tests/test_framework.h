/** @author southpaw | Project application code. */
/**
 * @file    test_framework.h
 * @brief   Minimal host test assertions and suite declarations.
 * @details Used by the sequential host runner and its portable test suites.
 *          An assertion stops only its current case so later cases still execute.
 */
#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Assertions abort only the current case; test_run prints one status per case. */
#define CHECK(expression) do { \
    if (!(expression)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression); \
        return false; \
    } \
} while (0)

#define CHECK_STRING(actual, expected) CHECK(strcmp((actual), (expected)) == 0)
#define RUN_TEST(function) test_run(#function, (function))

/** @brief Execute and report one case. @param name Case name.
 * @param function Case body. @return Nothing. @note Context: host main only.
 */
void test_run(const char *name, bool (*function)(void));

/** @brief Run math cases. @return Nothing. @note Context: host main only. */
void test_encoder_math(void);
/** @brief Run monitor cases. @return Nothing. @note Context: host main only. */
void test_speed_monitor(void);
/** @brief Run formatter cases. @return Nothing. @note Context: host main only. */
void test_telemetry(void);
/** @brief Run profile simulations. @return Nothing. @note Context: host main only. */
void test_scenarios(void);

#endif
