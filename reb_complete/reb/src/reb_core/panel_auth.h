/**
 * @file    panel_auth.h
 * @brief   Physical panel authentication interface.
 *
 * Provides rising-edge authentication with configurable lockout after
 * repeated failures. Password comparison is performed in constant time.
 */

#ifndef PANEL_AUTH_H
#define PANEL_AUTH_H

#include "reb/reb_types.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialises the authentication context to a known zero state.
 *
 * @param ctx  Pointer to the context to initialise. Must not be NULL.
 */
void panel_auth_init(panel_auth_ctx_t *ctx);

/**
 * @brief Advances the authentication state machine by one cycle.
 *
 * Authentication is evaluated on the rising edge of @p auth_pulse or
 * @p cancel_req. When the wrong password is submitted @c MAX_AUTH_ATTEMPTS
 * times, the context enters lockout for @c LOCKOUT_CYCLES cycles, during
 * which all authentication attempts are suppressed.
 *
 * @param ctx            Pointer to the authentication context.
 * @param auth_pulse     Level signal whose rising edge triggers evaluation.
 * @param password_hash  32-bit hash of the candidate password.
 * @param cancel_req     Level signal whose rising edge triggers cancellation.
 * @param auth_ok_out    Set to @c true when authentication succeeds this cycle.
 * @param locked_out     Set to @c true while the lockout timer is active.
 */
void panel_auth_step(panel_auth_ctx_t *ctx,
                     bool auth_pulse,
                     uint32_t password_hash,
                     bool cancel_req,
                     bool *auth_ok_out,
                     bool *locked_out);

/**
 * @brief Clears the lockout state and resets the failure counter.
 *
 * @param ctx  Pointer to the authentication context.
 */
void panel_auth_reset_lockout(panel_auth_ctx_t *ctx);

#endif /* PANEL_AUTH_H */
