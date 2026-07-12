/**
 * @file    reversal_window.c
 * @brief   Reversal window timer implementation.
 */

#include "reversal_window.h"
#include "reb/reb_params.h"
#include <string.h>

void rw_init(rw_ctx_t *ctx)
{
    (void)memset(ctx, 0, sizeof(*ctx));
}

void rw_start(rw_ctx_t *ctx, rw_mode_t mode)
{
    ctx->mode        = mode;
    ctx->timer_cycles = 0U;
    ctx->active      = true;
    ctx->pre_block_alert_active  = true;
    ctx->blocking_actuation_issued = false;

    ctx->limit_cycles = (uint32_t)T_REVERSAL_CYCLES;
}

rw_result_t rw_step(rw_ctx_t *ctx, bool password_valid)
{
    if (!ctx->active) {
        return RW_RUNNING;
    }

    ctx->timer_cycles++;

    if (password_valid) {
        /**
         * @note Password cancellation is only honoured before a blocking actuation
         *       has been issued. Once issued, the window must run to expiry.
         */
        if (!ctx->blocking_actuation_issued) {
            ctx->active = false;
            ctx->pre_block_alert_active = false;
            return RW_ABORT;
        }
    }

    if (ctx->timer_cycles >= ctx->limit_cycles) {
        ctx->active = false;
        ctx->pre_block_alert_active = false;
        return RW_EXPIRE;
    }

    return RW_RUNNING;
}

void rw_cancel(rw_ctx_t *ctx)
{
    ctx->active = false;
    ctx->pre_block_alert_active = false;
    ctx->timer_cycles = 0U;
}

void rw_set_actuation_issued(rw_ctx_t *ctx)
{
    ctx->blocking_actuation_issued = true;
}

bool rw_is_actuation_issued(const rw_ctx_t *ctx)
{
    return ctx->blocking_actuation_issued;
}

uint32_t rw_remaining_s(const rw_ctx_t *ctx)
{
    uint32_t elapsed_ms;
    uint32_t total_ms;

    if (!ctx->active) {
        return 0U;
    }

    elapsed_ms = ctx->timer_cycles * (uint32_t)REB_TS_MS;
    total_ms   = ctx->limit_cycles  * (uint32_t)REB_TS_MS;

    if (elapsed_ms >= total_ms) {
        return 0U;
    }

    return (total_ms - elapsed_ms) / 1000U;
}
