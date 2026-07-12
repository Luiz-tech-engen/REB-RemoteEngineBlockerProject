/**
 * @file    sensor_fusion.h
 * @brief   Sensor fusion interface for automatic theft detection (FR-007).
 */

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include "reb/reb_types.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief  Initializes the sensor fusion context to a known zero state.
 * @param  ctx  Pointer to the context structure to initialize.
 */
void sf_init(sf_ctx_t *ctx);

/**
 * @brief  Executes one sensor fusion cycle.
 *
 * @details Computes a weighted score from the supplied sensor inputs, applies
 *          debounce logic on activation, and hysteresis on deactivation.
 *          Must be called at a fixed 10 ms period (100 Hz).
 *
 * @param  ctx          Persistent fusion context; must be initialized via sf_init().
 * @param  accel_peak   Peak accelerometer magnitude normalized to [0.0, 1.0]
 *                      (DBC scale factor 0.01 applied by the caller).
 * @param  glass_break  Glass-break sensor confidence in [0.0, 1.0].
 * @param  out          Pointer to the output structure populated on return.
 *                      Fields: theft_score, theft_detected, debounce_cnt.
 */
void sf_step(sf_ctx_t *ctx,
             float accel_peak,
             float glass_break,
             sf_output_t *out);

#endif /* SENSOR_FUSION_H */
