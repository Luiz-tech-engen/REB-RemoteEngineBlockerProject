/**
 * @file    powertrain_validation.h
 * @brief   Powertrain signal validation interface.
 *
 * Validates vehicle speed, ignition state, and brake pedal signals.
 * Produces a per-signal validity flag, a composite fault bitmask,
 * and an anti-chattering recovery counter for the composite valid flag.
 */

#ifndef POWERTRAIN_VALIDATION_H
#define POWERTRAIN_VALIDATION_H

#include "reb/reb_types.h"
#include "reb/reb_params.h"
#include "src/can/can_defs.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief   Persistent state for one powertrain validation context.
 *
 * Must be zero-initialised and then passed to @ref pwt_init before first use.
 * All fields are updated in-place by @ref pwt_step.
 */
typedef struct {
    float    spd_prev;         /**< Vehicle speed from the previous cycle (km/h). */
    float    spd_delta_prev;   /**< Speed delta from the previous cycle, used for jitter detection. */
    float    brk_prev;         /**< Brake pedal position from the previous cycle (0.0–1.0). */
    float    brk_delta_prev;   /**< Brake delta from the previous cycle, used for jitter detection. */
    uint16_t spd_frozen_cnt;   /**< Consecutive cycles for which speed has not changed above threshold. */
    uint8_t  spd_jitter_cnt;   /**< Consecutive sign-alternating speed delta pairs (jitter counter). */
    uint8_t  ign_prev;         /**< Ignition state from the previous cycle. */
    uint8_t  ign_jitter_cnt;   /**< Consecutive cycles in which ignition state has changed. */
    uint8_t  brk_jitter_cnt;   /**< Consecutive sign-alternating brake delta pairs (jitter counter). */
    uint16_t recovery_cnt;     /**< Cycles elapsed since all signals last became simultaneously valid. */
} pwt_ctx_t;

/**
 * @brief   Outputs produced by @ref pwt_step for a single execution cycle.
 *
 * @note    @c powertrain_valid is the gated composite flag; it asserts only after
 *          the recovery counter reaches @c PWT_RECOVERY_CYCLES to suppress chattering.
 *          @c pt_fault_code bits are set independently of the recovery gate.
 */
typedef struct {
    bool     powertrain_valid; /**< Composite validity flag, gated by the recovery counter. */
    bool     speed_valid;      /**< Speed signal passed all per-signal checks. */
    bool     ign_valid;        /**< Ignition signal passed all per-signal checks. */
    bool     brake_valid;      /**< Brake pedal signal passed all per-signal checks. */
    uint16_t pt_fault_code;    /**< Bitmask of active fault conditions; 0 indicates no faults. */
} pwt_output_t;

/**
 * @brief   Initialises a powertrain validation context to its default state.
 *
 * Sets all counters and history values to zero, then positions the recovery
 * counter so that a sufficiently long run of valid inputs immediately produces
 * @c powertrain_valid == true.
 *
 * @param[out] ctx  Pointer to the context to initialise. Must not be NULL.
 */
void pwt_init(pwt_ctx_t *ctx);

/**
 * @brief   Executes one validation cycle for the powertrain signals.
 *
 * Checks each input signal for finiteness, range, freeze, jump, jitter, and
 * cross-signal plausibility. Updates the fault bitmask, per-signal validity
 * flags, and the composite @c powertrain_valid output accordingly.
 *
 * @param[in,out] ctx             Persistent validation context. Must not be NULL.
 * @param[in]     vehicle_speed   Current vehicle speed in km/h. Valid range: [0.0, 250.0].
 * @param[in]     ignition_state  Current ignition state; must be within the range defined
 *                                by the @c IGN_* constants (up to and including @c IGN_START).
 * @param[in]     brake_pedal     Normalised brake pedal position. Valid range: [0.0, 1.0].
 * @param[out]    out             Pointer to the output structure populated by this call.
 *                                Must not be NULL. All fields are overwritten each cycle.
 */
void pwt_step(pwt_ctx_t *ctx,
              float vehicle_speed,
              uint8_t ignition_state,
              float brake_pedal,
              pwt_output_t *out);

#endif /* POWERTRAIN_VALIDATION_H */

/** @brief Number of consecutive frozen-signal cycles required to assert the freeze fault. */
#define PWT_FROZEN_CYCLES   50U

/** @brief Minimum consecutive valid cycles required before @c powertrain_valid is asserted. */
#define PWT_RECOVERY_CYCLES 50U
