/**
 * @file    actuator_iface.c
 * @brief   Actuator interface layer with fuel derating safety floor (FR-009).
 *
 * act_apply_derate() is the sole function authorized to produce derate_pct_out.
 * It is invoked after all upstream FSM logic so that the FUEL_FLOOR_PCT minimum
 * is enforced independently of failures in other modules (NFR-SAF-001).
 *
 * Acceptance criterion FR-009: zero samples with derate_pct_out < FUEL_FLOOR_PCT
 * when vehicle_speed > V_STOP_KMH and state == STATE_BLOCKING.
 */

#include "actuator_iface.h"
#include "reb/reb_params.h"
#include <string.h>

/**
 * @brief Argument-safe two-operand maximum for uint8_t values.
 *        Each operand is evaluated exactly once.
 */
#define U8_MAX2(a, b)  (((a) > (b)) ? (a) : (b))

void act_init(actuator_output_t *out)
{
    (void)memset(out, 0, sizeof(*out));
    out->derate_pct  = (uint8_t)DERATE_PCT_INIT;
    out->starter_ok  = true;
}

void act_apply_derate(reb_state_t state,
                      float vehicle_speed,
                      uint8_t derate_pct_in,
                      actuator_output_t *out)
{
    bool in_blocking_motion;

    in_blocking_motion = (state == STATE_BLOCKING) &&
                         (vehicle_speed > V_STOP_KMH);

    if (in_blocking_motion) {
        out->derate_pct           = U8_MAX2(derate_pct_in, (uint8_t)FUEL_FLOOR_PCT);
        out->fuel_derating_active = true;
    } else if (state == STATE_BLOCKING) {
        out->derate_pct           = derate_pct_in;
        out->fuel_derating_active = false;
    } else if (state == STATE_BLOCKED) {
        out->derate_pct           = (uint8_t)DERATE_PCT_INIT;
        out->fuel_derating_active = false;
    } else {
        out->derate_pct           = (uint8_t)DERATE_PCT_INIT;
        out->fuel_derating_active = false;
    }
}

void act_set_starter_inhibit(bool inhibit, actuator_output_t *out)
{
    out->starter_inhibit_active = inhibit;
    out->starter_ok             = !inhibit;
}