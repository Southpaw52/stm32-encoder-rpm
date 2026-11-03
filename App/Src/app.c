/** @author southpaw | Project application code. */
/**
 * @file    app.c
 * @brief   Encoder startup, periodic sampling, and UART DMA coordination.
 * @details Initializes hardware in main context and captures counters in the TIM6 ISR.
 *          Main owns formatting; UART callbacks release the persistent DMA buffer.
 */
#include "app.h"

#include "main.h"
#include "encoder_config.h"
#include "encoder_math.h"
#include "speed_monitor.h"
#include "telemetry.h"

#define UART_BANNER_TIMEOUT_MS        100U
#define APB_PRESCALER_DIVIDED_BIT      4U
#define APB_PRESCALER_SHIFT_OFFSET     3U

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart2;

typedef struct {
    volatile uint32_t uart_start_errors;
    volatile uint32_t uart_callback_errors;
    volatile uint32_t format_errors;
} app_statistics_t;

static app_statistics_t statistics;
static speed_monitor_t monitor;
static volatile bool uart_tx_busy;
/* Main context only: consumed windows lost to formatting or TX start failure. */
static uint32_t unreported_windows;
/* Static lifetime: DMA owns these bytes until completion/error callback. */
static char tx_buffer[TELEMETRY_LINE_MAX];

/** @brief Decode the configured APB1 clock divisor for timer validation. */
static uint32_t apb1_prescaler_divisor(void)
{
    const uint32_t encoded = (RCC->CFGR & RCC_CFGR_PPRE1_Msk) >> RCC_CFGR_PPRE1_Pos;
    /* PPRE1: 0xx => /1; 100,101,110,111 => /2,/4,/8,/16. */
    return (encoded < APB_PRESCALER_DIVIDED_BIT)
         ? 1U : 1UL << (encoded - APB_PRESCALER_SHIFT_OFFSET);
}

void app_init(void)
{
    const uint32_t timer_clock_hz = encoder_apb_timer_clock_hz(
        HAL_RCC_GetPCLK1Freq(), apb1_prescaler_divisor());
    if (!encoder_timebase_period_matches(timer_clock_hz, htim6.Init.Prescaler,
                                         htim6.Init.Period, SPEED_WINDOW_MS)) {
        Error_Handler();
        return;
    }
#ifdef DEBUG
    /* Preserve the SWD debug session when the main loop enters WFI sleep. */
    HAL_DBGMCU_EnableDBGSleepMode();
#endif
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    if (HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL) != HAL_OK) {
        Error_Handler();
        return;
    }
    uart_tx_busy = false;
    unreported_windows = 0U;
    const size_t banner_length = telemetry_format_banner(tx_buffer, sizeof(tx_buffer));
    if (banner_length == 0U) {
        ++statistics.format_errors;
        Error_Handler();
        return;
    }
    /* Blocking is confined to startup, before the measurement timer starts. */
    if (HAL_UART_Transmit(&huart2, (uint8_t *)tx_buffer, (uint16_t)banner_length,
                           UART_BANNER_TIMEOUT_MS) != HAL_OK) {
        ++statistics.uart_start_errors;
        Error_Handler();
        return;
    }
    /* HAL F4 1.28.3 sets URS before UG in TIM_Base_SetConfig; clear defensively. */
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    /* Keep the counter capture adjacent to TIM6 start so banner-time motion
     * is excluded from the first measurement window; the encoder is running.
     */
    speed_monitor_init(&monitor, __HAL_TIM_GET_COUNTER(&htim2));
    if (HAL_TIM_Base_Start_IT(&htim6) != HAL_OK) {
        Error_Handler();
    }
}

void app_run_once(void)
{
    speed_sample_t sample;
    uint32_t missed_windows;
    /* Do not consume a sample while DMA still owns the TX buffer. */
    if (!uart_tx_busy && speed_monitor_fetch(&monitor, &sample, &missed_windows)) {
        /* missed= counts completed windows never reported on UART: skipped
         * while TX was busy or lost to formatting/TX start failure.
         * Position totals are never affected. Saturate both additions instead
         * of wrapping at UINT32_MAX (practically unreachable).
         */
        const uint32_t reported_missed =
            (missed_windows > UINT32_MAX - unreported_windows) ? UINT32_MAX
            : missed_windows + unreported_windows;
        const encoder_direction_t direction = encoder_direction_from_delta(
            sample.delta_counts, SPEED_STOP_THRESHOLD_COUNTS);
        const int32_t rpm_x100 = (direction == ENCODER_DIR_STOP) ? 0
            : encoder_rpm_x100(sample.delta_counts, ENCODER_COUNTS_PER_REV,
                                SPEED_WINDOW_MS);
        const size_t length = telemetry_format_sample(tx_buffer, sizeof(tx_buffer),
            &sample, rpm_x100, direction, reported_missed);
        if (length == 0U) {
            unreported_windows = (reported_missed == UINT32_MAX) ? UINT32_MAX
                : reported_missed + 1U;
            ++statistics.format_errors;
        } else {
            /* Publish busy before starting DMA: callbacks can only follow start.
             * A failed start clears busy below, so IRQ masking is unnecessary.
             */
            uart_tx_busy = true;
            if (HAL_UART_Transmit_DMA(&huart2, (uint8_t *)tx_buffer,
                                      (uint16_t)length) != HAL_OK) {
                uart_tx_busy = false;
                unreported_windows = (reported_missed == UINT32_MAX) ? UINT32_MAX
                    : reported_missed + 1U;
                ++statistics.uart_start_errors;
            } else {
                unreported_windows = 0U;
            }
        }
    }
    /* SysTick wakes every 1 ms: a sample arriving between the check and WFI
     * is delayed at most 1 ms under the configured, enabled SysTick timebase.
     */
    __WFI();
}

/** @brief Capture a TIM6 window. @param htim HAL timer handle. @return Nothing.
 * @note Context: ISR. Other HAL timer callbacks are intentionally ignored.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        /* CNT is one aligned 32-bit read, atomic on Cortex-M4; TIM2 is 32-bit.
         * A few microseconds of ISR latency jitter in a 1 s window gives
         * ppm-level timing error. No edge interrupts or direction-bit race.
         */
        speed_monitor_on_window_elapsed(&monitor, __HAL_TIM_GET_COUNTER(&htim2));
    }
}

/** @brief Release the DMA TX buffer. @param huart HAL UART handle.
 * @return Nothing. @note Context: USART2 ISR after DMA and UART TC completion.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        uart_tx_busy = false;
    }
}

/** @brief Record a UART error and release TX. @param huart HAL UART handle.
 * @return Nothing. @note Context: UART/DMA ISR; no recursive error telemetry.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        ++statistics.uart_callback_errors;
        uart_tx_busy = false;
    }
}
