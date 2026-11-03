/** @author southpaw | Project application code. */
/**
 * @file    telemetry.h
 * @brief   Bounded integer-only UART telemetry formatting interface.
 * @details Declares banner and sample formatting for main context and host tests.
 *          Callers own storage; insufficient capacity yields an empty result.
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "encoder_math.h"
#include "speed_monitor.h"
#include <stddef.h>

/** @brief Format the startup banner from encoder_config.h.
 * @param buf Output storage (NULL allowed if size is zero). @param size Capacity.
 * @return Length excluding NUL, or zero and an empty string if it cannot fit.
 * @note Context: main/host. Integer-only; no printf or dynamic allocation.
 */
size_t telemetry_format_banner(char *buf, size_t size);

/** @brief Format a sample as a CRLF-terminated UART line with no truncation.
 * @param buf Output storage (NULL allowed if size is zero). @param size Capacity.
 * @param s Valid sample. @param rpm_x100 Signed centi-RPM, printed as magnitude.
 * @param dir Net direction. @param missed_windows Skipped windows; omit if zero.
 * @return Length excluding NUL, or zero and an empty string if it cannot fit.
 * @note Context: main/host. Supports full int64/int32 ranges. The t field is
 * sequence in seconds for the configured one-second window, not a wall clock.
 */
size_t telemetry_format_sample(char *buf, size_t size, const speed_sample_t *s,
                                int32_t rpm_x100, encoder_direction_t dir,
                                uint32_t missed_windows);

#endif
