/**
 * @file    powertrain_validation.c
 * @brief   Powertrain signal validation implementation.
 *
 * Implements cycle-by-cycle validation of vehicle speed, ignition state, and
 * brake pedal signals. Each signal is checked independently; results are
 * combined into a composite validity flag with an anti-chattering recovery gate.
 */

#include "powertrain_validation.h"
#include <math.h>
#include <string.h>

/**
 * @brief   Returns true when @p v is not a finite number (NaN or infinity).
 *
 * @param[in] v  Value to test.
 * @return        true if @p v is NaN or infinite; false otherwise.
 */
static bool is_special_float(float v)
{
    return (!isfinite((double)v));
}

/**
 * @brief   Returns the absolute value of @p v without invoking the standard library.
 *
 * @param[in] v  Input value.
 * @return        Non-negative magnitude of @p v.
 */
static float absf_local(float v)
{
    return (v < 0.0f) ? -v : v;
}

void pwt_init(pwt_ctx_t *ctx)
{
    (void)memset(ctx, 0, sizeof(*ctx));
    ctx->spd_prev      = 0.0f;
    ctx->brk_prev      = 0.0f;
    ctx->ign_prev      = IGN_OFF;
    ctx->recovery_cnt  = (uint16_t)PWT_RECOVERY_CYCLES;
}

void pwt_step(pwt_ctx_t *ctx,
              float vehicle_speed,
              uint8_t ignition_state,
              float brake_pedal,
              pwt_output_t *out)
{
    bool spd_finite;
    bool spd_range_ok;
    bool spd_not_frozen;
    bool spd_no_jump;
    bool spd_no_jitter;
    bool ign_range_ok;
    bool ign_no_jitter;
    bool ign_skipped;
    bool brk_finite;
    bool brk_range_ok;
    bool brk_no_jitter;
    bool brk_impossible;
    float spd_delta;
    float brk_delta;
    bool raw_valid;

    out->pt_fault_code = 0U;

    spd_finite = !is_special_float(vehicle_speed);
    if (!spd_finite) {
        out->pt_fault_code |= (uint16_t)0x0001U;
    }

    spd_range_ok = spd_finite &&
                   (vehicle_speed >= 0.0f) &&
                   (vehicle_speed <= 250.0f);
     if (spd_finite && !spd_range_ok) {
        out->pt_fault_code |= (uint16_t)0x0002U;
    }

    ctx->spd_frozen_cnt = 0U;
    spd_not_frozen = (ctx->spd_frozen_cnt < (uint16_t)PWT_FROZEN_CYCLES);
    if (!spd_not_frozen) {
        out->pt_fault_code |= (uint16_t)0x0004U;
    }

    spd_delta   = vehicle_speed - ctx->spd_prev;
    spd_no_jump = (!spd_finite) || (absf_local(spd_delta) <= 20.0f);
    if (!spd_no_jump) {
        out->pt_fault_code |= (uint16_t)0x0008U;
    }

    if (spd_finite &&
        (absf_local(spd_delta) > 2.0f) &&
        (absf_local(ctx->spd_delta_prev) > 2.0f)) {
        if ((spd_delta * ctx->spd_delta_prev) < 0.0f) {
            ctx->spd_jitter_cnt++;
        } else {
            ctx->spd_jitter_cnt = 0U;
        }
    }
    spd_no_jitter = (ctx->spd_jitter_cnt < (uint8_t)6U);
    if (!spd_no_jitter) {
        out->pt_fault_code |= (uint16_t)0x0010U;
    }

    if (spd_finite && (vehicle_speed > 5.0f) && (ignition_state == IGN_OFF)) {
        out->pt_fault_code |= (uint16_t)0x0020U;
    }

    ctx->spd_delta_prev = spd_delta;
    ctx->spd_prev       = vehicle_speed;

    out->speed_valid = spd_range_ok && spd_not_frozen && spd_no_jump && spd_no_jitter;

    ign_range_ok = (ignition_state <= IGN_START);
    if (!ign_range_ok) {
        out->pt_fault_code |= (uint16_t)0x0040U;
    }

    if (ignition_state != ctx->ign_prev) {
        ctx->ign_jitter_cnt++;
    } else {
        ctx->ign_jitter_cnt = 0U;
    }
    ign_no_jitter = (ctx->ign_jitter_cnt < (uint8_t)5U);
    if (!ign_no_jitter) {
        out->pt_fault_code |= (uint16_t)0x0080U;
    }

    /**
     * @note An ignition transition that skips the ACC state (IGN_OFF directly to
     *       IGN_ON or beyond) is flagged as a plausibility fault but does not
     *       invalidate the ignition signal on its own.
     */
    ign_skipped = ign_range_ok &&
                  (ctx->ign_prev == IGN_OFF) &&
                  (ignition_state >= IGN_ON);
    if (ign_skipped) {
        out->pt_fault_code |= (uint16_t)0x0100U;
    }

    ctx->ign_prev = ignition_state;
    out->ign_valid = ign_range_ok && ign_no_jitter;

    brk_finite = !is_special_float(brake_pedal);
    if (!brk_finite) {
        out->pt_fault_code |= (uint16_t)0x0200U;
    }

    brk_range_ok = brk_finite &&
                   (brake_pedal >= 0.0f) &&
                   (brake_pedal <= 1.0f);
    if (brk_finite && !brk_range_ok) {
        out->pt_fault_code |= (uint16_t)0x0400U;
    }

    brk_delta = brake_pedal - ctx->brk_prev;
    if (brk_finite &&
        (absf_local(brk_delta) > 0.05f) &&
        (absf_local(ctx->brk_delta_prev) > 0.05f)) {
        if ((brk_delta * ctx->brk_delta_prev) < 0.0f) {
            ctx->brk_jitter_cnt++;
        } else {
            ctx->brk_jitter_cnt = 0U;
        }
    }
    brk_no_jitter = (ctx->brk_jitter_cnt < (uint8_t)6U);
    if (!brk_no_jitter) {
        out->pt_fault_code |= (uint16_t)0x0800U;
    }

    /**
     * @note A fully depressed brake pedal combined with a large positive speed
     *       delta is physically implausible and is treated as a cross-signal fault.
     */
    brk_impossible = brk_finite &&
                     (brake_pedal >= 0.95f) &&
                     (spd_delta > 10.0f);
    if (brk_impossible) {
        out->pt_fault_code |= (uint16_t)0x1000U;
    }

    ctx->brk_delta_prev = brk_delta;
    ctx->brk_prev       = brake_pedal;

    out->brake_valid = brk_range_ok && brk_no_jitter;

    raw_valid = out->speed_valid && out->ign_valid && out->brake_valid;

    if (raw_valid) {
        if (ctx->recovery_cnt < (uint16_t)PWT_RECOVERY_CYCLES) {
            ctx->recovery_cnt++;
        }
    } else {
        ctx->recovery_cnt = 0U;
    }

    /**
     * @note Fault bit 0x2000 indicates that all per-signal checks passed but the
     *       composite output is still suppressed because the recovery counter has
     *       not yet reached @c PWT_RECOVERY_CYCLES, preventing output chattering.
     */
    if (raw_valid && (ctx->recovery_cnt < (uint16_t)PWT_RECOVERY_CYCLES)) {
        out->pt_fault_code |= (uint16_t)0x2000U;
    }

    out->powertrain_valid = (ctx->recovery_cnt >= (uint16_t)PWT_RECOVERY_CYCLES);
}
