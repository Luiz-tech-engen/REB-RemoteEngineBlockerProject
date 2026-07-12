/**
 * @file    actuator_iface.h
 * @brief   Actuator interface layer with fuel derating safety floor (FR-009, NFR-SAF-001).
 *
 * Operates independently of the FSM to guarantee derate_pct >= FUEL_FLOOR_PCT
 * whenever vehicle speed exceeds V_STOP_KMH and the FSM state is STATE_BLOCKING.
 */

#ifndef ACTUATOR_IFACE_H
#define ACTUATOR_IFACE_H

#include "reb/reb_types.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initializes an actuator output structure to its power-on defaults.
 * @param out  Pointer to the output structure to initialize. Must not be NULL.
 */
void act_init(actuator_output_t *out);

/**
 * @brief Applies the fuel derating safety floor (FR-009).
 *
 * When vehicle_speed > V_STOP_KMH and state == STATE_BLOCKING, the output
 * derate_pct is computed as MAX(derate_pct_in, FUEL_FLOOR_PCT), ensuring
 * that upstream logic cannot reduce derating below the safety threshold while
 * the vehicle is in motion. In STATE_BLOCKED, derating is released because
 * starter inhibit takes over as the primary restraint mechanism.
 *
 * @param state          Current FSM state.
 * @param vehicle_speed  Vehicle speed in km/h.
 * @param derate_pct_in  Derating percentage computed by upstream logic [0..100].
 * @param out            Output structure updated with the corrected derate_pct
 *                       and fuel_derating_active flag.
 */
void act_apply_derate(reb_state_t state,
                      float vehicle_speed,
                      uint8_t derate_pct_in,
                      actuator_output_t *out);

/**
 * @brief Sets the starter inhibit flag in the actuator output (FR-011).
 * @param inhibit  True to inhibit starter engagement; false to permit it.
 * @param out      Output structure to update. Must not be NULL.
 */
void act_set_starter_inhibit(bool inhibit, actuator_output_t *out);

#endif /* ACTUATOR_IFACE_H */