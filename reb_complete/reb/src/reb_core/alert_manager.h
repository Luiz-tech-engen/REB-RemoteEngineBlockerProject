/**
 * @file    alert_manager.h
 * @brief   Visual and audible alert manager for the REB system (FR-013).
 *
 * Activates a 1 Hz intermittent horn, continuous hazard lights, and an HMI
 * alert upon expiry of the FR-012 safety timer.
 */

#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

#include "reb/reb_types.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Runtime context for the alert manager.
 *
 * Must be initialised with alert_mgr_init() before use.
 */
typedef struct {
    bool             alerts_active; /**< True while multimodal alerts are asserted.                 */
    uint32_t         horn_timer;    /**< Cycle counter driving the 1 Hz horn half-period toggle.    */
    bool             horn_state;    /**< Current horn output level: true = ON, false = OFF.         */
    hmi_alert_code_t hmi_code;      /**< Active HMI alert code; HMI_ALERT_NONE when inactive.       */
} alert_ctx_t;

/**
 * @brief Initialises the alert context to its inactive default state.
 * @param ctx  Alert context to initialise; must not be NULL.
 */
void alert_mgr_init(alert_ctx_t *ctx);

/**
 * @brief Activates all multimodal alerts (FR-013).
 *
 * Sets alerts_active, resets the horn timer, starts the horn in the ON state,
 * and sets hmi_code to HMI_ALERT_IMMINENT_BLOCKAGE.
 *
 * @param ctx  Alert context; must not be NULL.
 */
void alert_mgr_start(alert_ctx_t *ctx);

/**
 * @brief Advances the alert manager by one solver cycle.
 *
 * Toggles horn_state every HORN_HALF_PERIOD_CYCLES cycles to produce a 1 Hz
 * square wave. Has no effect on alert outputs when alerts_active is false.
 *
 * @param ctx  Alert context; must not be NULL.
 * @param out  Alert output structure populated each cycle; ignored if NULL.
 */
void alert_mgr_step(alert_ctx_t *ctx, alert_output_t *out);

/**
 * @brief Deactivates all alerts and clears output signals.
 *
 * Called on unblock or successful panel authentication. Both @p ctx and
 * @p out are individually null-checked before access.
 *
 * @param ctx  Alert context; may be NULL.
 * @param out  Alert output structure to clear; may be NULL.
 */
void alert_mgr_stop(alert_ctx_t *ctx, alert_output_t *out);

#endif /* ALERT_MANAGER_H */
