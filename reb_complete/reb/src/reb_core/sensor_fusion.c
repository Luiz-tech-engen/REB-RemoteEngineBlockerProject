/**
 * @file    sensor_fusion.c
 * @brief   Sensor fusion module for automatic theft detection (FR-007).
 *
 * @details Computes a weighted score from accelerometer and glass-break inputs:
 *          score = w_glass * glass_break + w_accel * (accel_peak / accel_max).
 *          Activation requires the score to remain above SF_THRESH for
 *          SF_DEBOUNCE_CYCLES consecutive cycles. Deactivation applies
 *          hysteresis: the output is cleared only when score drops below
 *          SF_THRESH_HYST_LOW.
 */

#include "sensor_fusion.h"
#include "reb/reb_params.h"
#include <string.h>

void sf_init(sf_ctx_t *ctx)
{
    (void)memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief  Clamps a floating-point value to the closed interval [lo, hi].
 * @param  v   Input value.
 * @param  lo  Lower bound.
 * @param  hi  Upper bound.
 * @return Value clamped to [lo, hi].
 */
static float clampf(float v, float lo, float hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

void sf_step(sf_ctx_t *ctx,
             float accel_peak,
             float glass_break,
             sf_output_t *out)
{
    float accel_norm;
    float score;

    accel_norm = clampf(accel_peak / ACCEL_MAX, 0.0f, 1.0f);

    score = (SF_W_GLASS * clampf(glass_break, 0.0f, 1.0f)) +
            (SF_W_ACCEL * accel_norm);
    score = clampf(score, 0.0f, 1.0f);

    ctx->last_score = score;

    if (!ctx->active) {
        if (score >= SF_THRESH) {
            ctx->debounce_cnt++;
            if (ctx->debounce_cnt >= (uint16_t)SF_DEBOUNCE_CYCLES) {
                ctx->active       = true;
                ctx->debounce_cnt = (uint16_t)SF_DEBOUNCE_CYCLES;
            }
        } else {
            ctx->debounce_cnt = 0U;
        }
    } else {
        if (score < SF_THRESH_HYST_LOW) {
            ctx->active       = false;
            ctx->debounce_cnt = 0U;
        }
    }

    out->theft_score    = score;
    out->theft_detected = ctx->active;
    out->debounce_cnt   = ctx->debounce_cnt;
}
