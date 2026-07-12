/**
 * @file    fsm.h
 * @brief   Main finite state machine interface (FR-001, FR-002, FR-003).
 *
 * @details State transition sequence:
 *          IDLE → THEFT_CONFIRMED → BLOCKING → BLOCKED → IDLE (on authenticated unlock).
 */

#ifndef FSM_H
#define FSM_H

#include "reb/reb_types.h"
#include "security_manager.h"
#include "panel_auth.h"
#include "sensor_fusion.h"
#include "actuator_iface.h"
#include "starter_control.h"
#include "alert_manager.h"
#include "reversal_window.h"
#include "powertrain_validation.h"
#include "nvm.h"
#include "event_log.h"
#include "src/can/can_rx.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Aggregated context structure for the REB finite state machine.
 *
 * @details Holds the primary FSM state together with all sub-module contexts,
 *          CAN supervision state, and internal cycle counters. A single instance
 *          of this structure represents the complete runtime state of the REB
 *          controller.
 */
typedef struct {
    fsm_ctx_t          fsm;

    sec_ctx_t          sec;
    panel_auth_ctx_t   panel;
    sf_ctx_t           sf;
    pwt_ctx_t          pwt;
    actuator_output_t  act;
    starter_ctx_t      starter;
    alert_ctx_t        alert;
    rw_ctx_t           rw;
    event_log_ctx_t    log;

    can_rx_watchdog_ctx_t rxwd;
    bool               channel_rx_ok;
    bool               rx_fail;
    int32_t            rx_channel_id;

    float              derate_ramp;   /**< Current progressive derate ramp value in the range [0.0, 100.0]. */

    uint32_t           status_timer;  /**< Cycle counter controlling the REB_STATUS transmission interval. */
    uint32_t           derate_timer;  /**< Cycle counter controlling the REB_DERATE transmission interval. */
    bool               signal_fault_locked;
} reb_ctx_t;

/**
 * @brief  Initializes the FSM and all sub-modules, restoring persisted state from NVM when available (NFR-REL-001).
 * @param  ctx  Pointer to the REB context structure to initialize.
 */
void reb_fsm_init(reb_ctx_t *ctx);

/**
 * @brief  Executes one FSM cycle at the fixed sample period of 10 ms.
 * @param  ctx  Pointer to the persistent REB context.
 * @param  in   Pointer to the input snapshot for this cycle.
 * @param  out  Pointer to the output structure populated on return.
 */
void reb_fsm_step(reb_ctx_t *ctx,
                  const reb_inputs_t *in,
                  reb_outputs_t *out);

#endif /* FSM_H */
