/**
 * @file    security_manager.c
 * @brief   Anti-replay security manager implementation (NFR-SEC-001).
 */

#include "security_manager.h"
#include "reb/reb_params.h"
#include <string.h>

void sec_mgr_init(sec_ctx_t *ctx)
{
    ctx->last_nonce = 0U;
}

bool sec_mgr_verify(sec_ctx_t *ctx,
                    uint16_t nonce,
                    uint32_t timestamp_ms,
                    bool sig_ok,
                    uint32_t current_ms,
                    auth_fail_t *result)
{
    uint32_t delta_ms;

    if (!sig_ok) {
        *result = AUTH_SIG_INVALID;
        return false;
    }

    if ((uint32_t)nonce <= ctx->last_nonce) {
        *result = AUTH_NONCE_REPLAY;
        return false;
    }

    /**
     * Compute the elapsed time between the frame timestamp and the current time.
     * If current_ms < timestamp_ms, the counter has wrapped or the timestamp is
     * from the future; treat the delta as exceeding the window to force rejection.
     */
    if (current_ms >= timestamp_ms) {
        delta_ms = current_ms - timestamp_ms;
    } else {
        delta_ms = NONCE_WINDOW_MS + 1U;
    }

    if (delta_ms > NONCE_WINDOW_MS) {
        *result = AUTH_TS_EXPIRED;
        return false;
    }

    ctx->last_nonce = (uint32_t)nonce;
    *result = AUTH_OK;
    return true;
}
