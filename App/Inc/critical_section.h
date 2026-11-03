/** @author southpaw | Project application code. */
/**
 * @file    critical_section.h
 * @brief   Interrupt-mask helpers for shared monitor snapshots.
 * @details Used by firmware main context to protect data shared with its ISR.
 *          Restore the saved PRIMASK; sequential host tests use no-op helpers.
 */
#ifndef CRITICAL_SECTION_H
#define CRITICAL_SECTION_H

#include <stdint.h>
#ifndef UNIT_TEST
#include "stm32f4xx.h"
#endif

/** @brief Save interrupt mask and prevent ISR preemption of a shared snapshot.
 * @return Prior PRIMASK on target, zero on host.
 * @note Context: any. Single Cortex-M4 core; does not mask NMI or faults, which
 * must not access the monitor. Host tests are sequential; no host locking.
 */
static inline uint32_t critical_section_enter(void)
{
#ifdef UNIT_TEST
    return 0U;
#else
    const uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
#endif
}

/** @brief Restore the saved interrupt mask, preserving nesting safety.
 * @param state Value returned by the matching enter call. @return Nothing.
 * @note Context: any; restore PRIMASK instead of unconditionally enabling IRQs.
 */
static inline void critical_section_exit(uint32_t state)
{
#ifdef UNIT_TEST
    (void)state;
#else
    __set_PRIMASK(state);
#endif
}

#endif
