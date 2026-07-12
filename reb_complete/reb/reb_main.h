/**
 * @file    reb_main.h
 * @brief   Top-level integration interface for the REB (Remote Engine Blocker) system.
 *
 * Exports the three entry points consumed by the CAN layer and the HMI layer:
 * reb_init(), reb_step(), and reb_get_state().
 */

#ifndef REB_MAIN_H
#define REB_MAIN_H

#include "reb/reb_types.h"
#include "src/reb_core/fsm.h"

/**
 * @brief  Initialises the REB system.
 *         Must be called exactly once before entering the main loop.
 * @param  ctx  Persistent system context; must remain valid for the lifetime of the application.
 */
void reb_init(reb_ctx_t *ctx);

/**
 * @brief  Executes one REB processing cycle (Ts = 10 ms).
 *
 * The CAN layer decodes incoming frames into @p in before this call and
 * encodes @p out into outgoing frames after it returns.
 *
 * @param  ctx  Persistent system context shared across calls.
 * @param  in   Decoded inputs provided by the CAN layer for the current cycle.
 * @param  out  Outputs to be encoded by the CAN layer after this call.
 */
void reb_step(reb_ctx_t *ctx,
              const reb_inputs_t *in,
              reb_outputs_t *out);

/**
 * @brief  Returns the current FSM state.
 * @param  ctx  System context (read-only).
 * @return Current @c reb_state_t value. Safe to call from the HMI task concurrently.
 */
reb_state_t reb_get_state(const reb_ctx_t *ctx);

/**
 * @brief  Returns a read-only pointer to the event log.
 * @param  ctx  System context (read-only).
 * @return Pointer to the internal @c event_log_ctx_t; valid as long as @p ctx is valid.
 */
const event_log_ctx_t *reb_get_log(const reb_ctx_t *ctx);

#endif /* REB_MAIN_H */
