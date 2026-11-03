/** @author southpaw | Project application code. */
/**
 * @file    app.h
 * @brief   Firmware application lifecycle entry points.
 * @details Declares initialization and main-loop work after CubeMX setup.
 *          UART DMA ownership and periodic ISR sampling stay inside the application.
 */
#ifndef APP_H
#define APP_H

/** @brief Validate the timebase, start the encoder, emit banner, then start TIM6.
 * @return Nothing; initialization failure enters Error_Handler.
 * @note Context: main only, once after CubeMX peripheral initialization.
 */
void app_init(void);

/** @brief Send one pending sample when UART DMA is idle, then wait for an IRQ.
 * @return Nothing. TX failures are recorded in debugger-visible statistics.
 * @note Context: main only, repeatedly; must run with interrupts enabled.
 */
void app_run_once(void);

#endif
