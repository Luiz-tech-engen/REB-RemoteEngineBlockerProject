/**
 * @file    can_rx.c
 * @brief   CAN reception watchdog with IP/SMS fallback channel supervision.
 */

#include "can_rx.h"
#include <string.h>

void can_rx_watchdog_init(can_rx_watchdog_ctx_t *ctx)
{
    (void)memset(ctx, 0, sizeof(*ctx));
}

void can_rx_watchdog_step(can_rx_watchdog_ctx_t *ctx,
                          bool ip_rx_ok,
                          bool sms_rx_ok,
                          uint32_t ts_ms,
                          uint32_t cmd_ack_timeout_ms,
                          uint8_t max_retries,
                          bool *channel_rx_ok,
                          bool *rx_fail,
                          int32_t *rx_channel_id)
{
    if (ip_rx_ok) {
        *channel_rx_ok = true;
        *rx_fail       = false;
        *rx_channel_id = 0;
        ctx->absence_count = 0;
        ctx->timer_count_ms = 0U;
        return;
    }

    if (sms_rx_ok) {
        *channel_rx_ok = true;
        *rx_fail       = false;
        *rx_channel_id = 1;
        ctx->absence_count = 0;
        ctx->timer_count_ms = 0U;
        return;
    }

    *rx_channel_id = 2;
    ctx->timer_count_ms += ts_ms;

    if (ctx->timer_count_ms >= cmd_ack_timeout_ms) {
        ctx->absence_count++;
        ctx->timer_count_ms = 0U;
    }

    if (ctx->absence_count >= (int32_t)max_retries) {
        *channel_rx_ok = false;
        *rx_fail       = true;
    } else {
        *channel_rx_ok = false;
        *rx_fail       = false;
    }
}