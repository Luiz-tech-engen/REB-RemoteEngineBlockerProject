/**
 * @file    reb_main.c
 * @brief   Implementation of the REB top-level integration interface.
 */

#include "reb_main.h"
#include "src/reb_core/fsm.h"

void reb_init(reb_ctx_t *ctx)
{
    reb_fsm_init(ctx);
}

void reb_step(reb_ctx_t *ctx,
              const reb_inputs_t *in,
              reb_outputs_t *out)
{
    reb_fsm_step(ctx, in, out);
}

reb_state_t reb_get_state(const reb_ctx_t *ctx)
{
    return ctx->fsm.state;
}

const event_log_ctx_t *reb_get_log(const reb_ctx_t *ctx)
{
    return &ctx->log;
}
