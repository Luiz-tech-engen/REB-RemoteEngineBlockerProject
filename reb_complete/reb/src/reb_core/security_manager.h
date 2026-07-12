/**
 * @file    security_manager.h
 * @brief   Anti-replay security manager interface (NFR-SEC-001).
 *
 * @details Provides monotonic nonce validation, timestamp window checking,
 *          and signature verification (HMAC stub for MIL-mode simulation).
 */

#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

#include "reb/reb_types.h"
#include "event_log.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Validates a complete REB_CMD frame against replay and integrity checks.
 *
 * @details Performs three sequential checks: signature validity, monotonic nonce
 *          ordering (anti-replay), and timestamp recency within NONCE_WINDOW_MS.
 *          On success, the internal nonce state is advanced. On failure, the
 *          state is not modified and @p result identifies the first failing check.
 *
 * @param  ctx           Persistent security context; must be initialized via sec_mgr_init().
 * @param  nonce         Nonce value extracted from frame 0x200.
 * @param  timestamp_ms  Timestamp in milliseconds extracted from frame 0x200.
 * @param  sig_ok        Signature validity flag supplied by the CAN layer (simulated in MIL).
 * @param  current_ms    Current simulation time in milliseconds.
 * @param  result        Output: set to AUTH_OK on success, or to the specific
 *                       failure code (AUTH_SIG_INVALID, AUTH_NONCE_REPLAY,
 *                       AUTH_TS_EXPIRED) on failure.
 * @return true if all checks pass, false otherwise.
 */
bool sec_mgr_verify(sec_ctx_t *ctx,
                    uint16_t nonce,
                    uint32_t timestamp_ms,
                    bool sig_ok,
                    uint32_t current_ms,
                    auth_fail_t *result);

/**
 * @brief  Initializes the security manager context.
 * @param  ctx  Pointer to the context structure to initialize.
 */
void sec_mgr_init(sec_ctx_t *ctx);

#endif /* SECURITY_MANAGER_H */
