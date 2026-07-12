/**
 * @file    can_rx.h
 * @brief   CAN reception watchdog with IP/SMS fallback channel supervision.
 *
 * Implements channel availability monitoring equivalent to the
 * Network_Model-TCU-REB-RX/Supervision Counter block.
 */

#ifndef CAN_RX_H
#define CAN_RX_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Persistent state for one watchdog instance.
 *
 * @var can_rx_watchdog_ctx_t::absence_count
 *      Number of consecutive timeout periods without a valid reception.
 * @var can_rx_watchdog_ctx_t::timer_count_ms
 *      Accumulated elapsed time in milliseconds within the current timeout window.
 */
typedef struct {
    int32_t  absence_count;
    uint32_t timer_count_ms;
} can_rx_watchdog_ctx_t;

/**
 * @brief Initializes a watchdog context to its reset state.
 * @param ctx  Pointer to the context to initialize. Must not be NULL.
 */
void can_rx_watchdog_init(can_rx_watchdog_ctx_t *ctx);

/**
 * @brief Advances the watchdog by one simulation step.
 *
 * Evaluates channel availability in priority order: IP first, then SMS.
 * If neither channel is active, accumulates elapsed time and increments
 * the absence counter once per timeout period. When the absence counter
 * reaches @p max_retries, @p rx_fail is asserted and @p channel_rx_ok
 * is cleared.
 *
 * @param ctx                 Persistent watchdog state. Must not be NULL.
 * @param ip_rx_ok            True if the IP channel received a valid frame this step.
 * @param sms_rx_ok           True if the SMS channel received a valid frame this step.
 * @param ts_ms               Elapsed time in milliseconds for this step.
 * @param cmd_ack_timeout_ms  Duration in milliseconds that defines one absence period.
 * @param max_retries         Maximum consecutive absence periods before failure is declared.
 * @param[out] channel_rx_ok  Set to true when at least one channel is active.
 * @param[out] rx_fail        Set to true when absence_count reaches max_retries.
 * @param[out] rx_channel_id  Active channel identifier: 0 = IP, 1 = SMS, 2 = none.
 */
void can_rx_watchdog_step(can_rx_watchdog_ctx_t *ctx,
                          bool ip_rx_ok,
                          bool sms_rx_ok,
                          uint32_t ts_ms,
                          uint32_t cmd_ack_timeout_ms,
                          uint8_t max_retries,
                          bool *channel_rx_ok,
                          bool *rx_fail,
                          int32_t *rx_channel_id);

#endif /* CAN_RX_H */