/**
 * @file    starter_control.c
 * @brief   Starter inhibition control module (FR-011).
 *
 * @details On the first cycle in STATE_BLOCKED, inhibition is activated
 *          immediately and cmd_pending is set to trigger an initial CAN
 *          transmission. While STATE_BLOCKED persists, cmd_pending is
 *          reasserted every RETRANSMIT_BLOCK_TIMEOUT_CYCLES cycles (5 s)
 *          to drive periodic retransmission of message 0x400.
 */

#include "starter_control.h"
#include "actuator_iface.h"
#include "reb/reb_params.h"
#include <string.h>

void starter_init(starter_ctx_t *ctx)
{
    (void)memset(ctx, 0, sizeof(*ctx));
}

void starter_step(starter_ctx_t *ctx,
                  reb_state_t state,
                  actuator_output_t *out)
{
    if (state == STATE_BLOCKED) {
        if (!ctx->inhibit_active) {
            ctx->inhibit_active   = true;
            ctx->retransmit_timer = 0U;
            ctx->cmd_pending      = true;
        } else {
            ctx->retransmit_timer++;
            if (ctx->retransmit_timer >= (uint32_t)RETRANSMIT_BLOCK_TIMEOUT_CYCLES) {
                ctx->retransmit_timer = 0U;
                ctx->cmd_pending      = true;
            } else {
                ctx->cmd_pending = false;
            }
        }
        act_set_starter_inhibit(true, out);
    } else {
        if (ctx->inhibit_active) {
            ctx->inhibit_active   = false;
            ctx->retransmit_timer = 0U;
            ctx->cmd_pending      = false;
            act_set_starter_inhibit(false, out);
        }
    }
}

void starter_release(starter_ctx_t *ctx, actuator_output_t *out)
{
    ctx->inhibit_active   = false;
    ctx->retransmit_timer = 0U;
    ctx->cmd_pending      = false;
    act_set_starter_inhibit(false, out);
}
