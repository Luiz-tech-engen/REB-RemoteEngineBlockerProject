/**
 * @file    alert_manager.c
 * @brief   Implementation of the REB alert manager (FR-013).
 *
 * Horn oscillates at 1 Hz: 50 cycles ON followed by 50 cycles OFF at Ts = 10 ms.
 * FR-013 requires horn and hazard activation within 100 ms of the triggering timer.
 */

#include "alert_manager.h"
#include "reb/reb_params.h"
#include <string.h>

/**
 * @brief Number of solver cycles per horn half-period.
 *
 * Derived from: 1 Hz period = 1000 ms / Ts(10 ms) = 100 cycles; half-period = 50 cycles.
 */
#define HORN_HALF_PERIOD_CYCLES  50U

void alert_mgr_init(alert_ctx_t *ctx)
{
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->hmi_code = HMI_ALERT_NONE;
}

void alert_mgr_start(alert_ctx_t *ctx)
{
    ctx->alerts_active = true;
    ctx->horn_timer    = 0U;
    ctx->horn_state    = true;
    ctx->hmi_code      = HMI_ALERT_IMMINENT_BLOCKAGE;
}

void alert_mgr_step(alert_ctx_t *ctx, alert_output_t *out)
{
    if (out == NULL) {
        return;
    }

    if (!ctx->alerts_active) {
        out->horn_active   = false;
        out->hazard_active = false;
        out->hmi_alert     = false;
        out->hmi_code      = HMI_ALERT_NONE;
        return;
    }

    ctx->horn_timer++;
    if (ctx->horn_timer >= (uint32_t)HORN_HALF_PERIOD_CYCLES) {
        ctx->horn_timer = 0U;
        ctx->horn_state = !ctx->horn_state;
    }

    out->horn_active   = ctx->horn_state;
    out->hazard_active = true;
    out->hmi_alert     = true;
    out->hmi_code      = ctx->hmi_code;
}

void alert_mgr_stop(alert_ctx_t *ctx, alert_output_t *out)
{
    if (ctx != NULL) {
        ctx->alerts_active = false;
        ctx->horn_timer    = 0U;
        ctx->horn_state    = false;
        ctx->hmi_code      = HMI_ALERT_NONE;
    }

    if (out != NULL) {
        out->horn_active   = false;
        out->hazard_active = false;
        out->hmi_alert     = false;
    }
}
