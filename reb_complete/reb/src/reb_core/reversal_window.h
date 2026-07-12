/**
 * @file    reversal_window.h
 * @brief   Reversal window timers for automatic-trigger actuation sequences.
 *
 * Two window durations are supported: 60 s (RW_MODE_60)
 * Reversal windows apply exclusively to automatic trigger sources; panel and
 * remote trigger sources do not activate these windows.
 */

#ifndef REVERSAL_WINDOW_H
#define REVERSAL_WINDOW_H

#include "reb/reb_types.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief   Initialises a reversal window context to its default state.
 *
 * @param[out] ctx  Pointer to the context to initialise. Must not be NULL.
 */
void rw_init(rw_ctx_t *ctx);

/**
 * @brief   Starts a reversal window in the specified mode.
 *
 * Resets the timer and marks the window as active. The duration limit is
 * derived from the mode: RW_MODE_60 uses 
 *
 * @param[in,out] ctx   Pointer to an initialised context. Must not be NULL.
 * @param[in]     mode  Window duration mode (RW_MODE_60 
 */
void rw_start(rw_ctx_t *ctx, rw_mode_t mode);

/**
 * @brief   Advances the reversal window by one execution cycle.
 *
 * Must be called once per scheduler tick (period = REB_TS_MS).
 * If a valid cancellation password is presented before a blocking actuation
 * has been issued, the window is aborted. Once a blocking actuation has been
 * issued, password cancellation is rejected and the window continues to expiry.
 *
 * @param[in,out] ctx             Pointer to an active context. Must not be NULL.
 * @param[in]     password_valid  true if a valid cancellation password was received this cycle.
 * @return        RW_ABORT   if the window was cancelled by a valid password.
 * @return        RW_EXPIRE  if the timer reached the configured limit.
 * @return        RW_RUNNING otherwise, including when the window is already inactive.
 */
rw_result_t rw_step(rw_ctx_t *ctx, bool password_valid);

/**
 * @brief   Unconditionally deactivates the reversal window and resets the timer.
 *
 * @param[in,out] ctx  Pointer to the context. Must not be NULL.
 */
void rw_cancel(rw_ctx_t *ctx);

/**
 * @brief   Records that a blocking actuation command has been issued.
 *
 * Once called, subsequent password cancellation attempts within @ref rw_step
 * are silently rejected and the window proceeds to expiry.
 *
 * @param[in,out] ctx  Pointer to the context. Must not be NULL.
 */
void rw_set_actuation_issued(rw_ctx_t *ctx);

/**
 * @brief   Returns whether a blocking actuation has been issued for this window.
 *
 * @param[in] ctx  Pointer to the context. Must not be NULL.
 * @return         true if @ref rw_set_actuation_issued has been called; false otherwise.
 */
bool rw_is_actuation_issued(const rw_ctx_t *ctx);

/**
 * @brief   Returns the number of whole seconds remaining in the active window.
 *
 * Intended for use by display or HMI countdown logic. Returns 0 when the
 * window is inactive or when the elapsed time meets or exceeds the limit.
 *
 * @param[in] ctx  Pointer to the context. Must not be NULL.
 * @return         Remaining time in seconds, or 0 if the window is not active.
 */
uint32_t rw_remaining_s(const rw_ctx_t *ctx);

#endif /* REVERSAL_WINDOW_H */
