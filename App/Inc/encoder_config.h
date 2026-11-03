/** @author southpaw | Project application code. */
/**
 * @file    encoder_config.h
 * @brief   Shared encoder, sampling, and telemetry configuration.
 * @details Provides constants to firmware and host tests without HAL dependencies.
 *          Compile-time assertions bound arithmetic inputs and telemetry storage.
 */
#ifndef ENCODER_CONFIG_H
#define ENCODER_CONFIG_H

#include <stdint.h>

#define ENCODER_PPR                    100U  /* Pulses/revolution per channel. */
#define ENCODER_QUADRATURE_FACTOR      4U    /* TI12 counts both edges of A and B. */
#define ENCODER_COUNTS_PER_REV         (ENCODER_PPR * ENCODER_QUADRATURE_FACTOR)
#define SPEED_WINDOW_MS                1000U /* Validated against TIM6 at startup. */
#define SPEED_STOP_THRESHOLD_COUNTS    0U
#define APP_FW_VERSION_STRING          "1.0.0"

#define ENCODER_SECONDS_PER_MINUTE     60U
#define ENCODER_MS_PER_SECOND          1000U
#define ENCODER_CENTI_RPM_SCALE        100U

/* Worst case, including independent formatter arguments:
 * t= + uint32(10) + s                         = 13
 * space + pulses= + signed int64(20)         = 28
 * space + delta= + signed int32(11)          = 18
 * space + dir= + STOP(4)                    =  9
 * space + rpm= + 21474836.48(11)             = 16
 * space + missed= + uint32(10)               = 18
 * CRLF + NUL                                =  3
 * Total = 105 bytes. The requested 96 cannot hold every legal sample.
 */
#define TELEMETRY_WORST_CASE_BYTES      (13U + 28U + 18U + 9U + 16U + 18U + 3U)
#define TELEMETRY_LINE_MAX             105U

_Static_assert(ENCODER_PPR > 0U, "Encoder PPR must be positive");
_Static_assert(ENCODER_QUADRATURE_FACTOR > 0U, "Quadrature factor must be positive");
_Static_assert(ENCODER_PPR <= UINT32_MAX / ENCODER_QUADRATURE_FACTOR,
               "Counts per revolution must fit uint32_t");
_Static_assert(ENCODER_COUNTS_PER_REV > 0U, "Counts per revolution must be positive");
_Static_assert(SPEED_WINDOW_MS > 0U, "Measurement window must be positive");
_Static_assert(TELEMETRY_LINE_MAX >= TELEMETRY_WORST_CASE_BYTES,
               "Telemetry buffer must hold the worst case including NUL");
/* Banner bound: fixed text, version string, and three uint32 decimal fields. */
_Static_assert(TELEMETRY_LINE_MAX >= sizeof("# encoder_rpm fw= ppr= cpr= window_ms=\r\n")
               + sizeof(APP_FW_VERSION_STRING) - 1U + 3U * 10U,
               "Telemetry buffer must also hold the banner");

#endif
