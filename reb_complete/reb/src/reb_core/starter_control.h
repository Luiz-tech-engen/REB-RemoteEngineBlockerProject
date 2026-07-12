/**
 * @file    starter_control.h
 * @brief   Starter inhibition control interface (FR-011, IF-CAN-005).
 *
 * @details Manages activation and periodic retransmission of the
 *          REB_PREVENT_START command (0x403/0x400) while the system
 *          remains in STATE_BLOCKED.
 */

#ifndef STARTER_CONTROL_H
#define STARTER_CONTROL_H

#include "reb/reb_types.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool     inhibit_active;     /**< Indicates whether starter inhibition is currently active. */
    uint32_t retransmit_timer;   /**< Cycle counter tracking elapsed time until the next retransmission. */
    bool     cmd_pending;        /**< Set to true when a CAN transmission must be issued this cycle. */
} starter_ctx_t;

/**
 * @brief  Initializes the starter control context to a known zero state.
 * @param  ctx  Pointer to the context structure to initialize.
 */
void starter_init(starter_ctx_t *ctx);

/**
 * @brief  Executes one cycle of the starter inhibition control logic.
 *
 * @details Activates inhibition immediately upon entry into STATE_BLOCKED and
 *          sets cmd_pending every RETRANSMIT_BLOCK_TIMEOUT_CYCLES cycles for
 *          periodic CAN retransmission. Releases inhibition on the first cycle
 *          after leaving STATE_BLOCKED.
 *
 * @param  ctx    Persistent control context; must be initialized via starter_init().
 * @param  state  Current FSM state used to determine inhibition behavior.
 * @param  out    Actuator output structure updated with the starter inhibit signal.
 */
void starter_step(starter_ctx_t *ctx,
                  reb_state_t state,
                  actuator_output_t *out);

/**
 * @brief  Unconditionally releases starter inhibition and resets all context state.
 * @param  ctx  Pointer to the control context.
 * @param  out  Actuator output structure updated to reflect the released state.
 */
void starter_release(starter_ctx_t *ctx, actuator_output_t *out);

#endif /* STARTER_CONTROL_H */
