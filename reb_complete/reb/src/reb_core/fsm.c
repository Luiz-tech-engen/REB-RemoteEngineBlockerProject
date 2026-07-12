/**
 * @file    fsm.c
 * @brief   REB main finite state machine implementation (FR-001 through FR-013).
 *
 * State hierarchy: IDLE → THEFT_CONFIRMED → BLOCKING → BLOCKED.
 * Includes progressive fuel derating ramp, pre-alert window for AUTO-source
 * activations, parked dwell timer, and NVM persistence on every transition.
 */

#include "fsm.h"
#include "reb/reb_params.h"
#include "powertrain_validation.h"
#include <string.h>
#include <stdbool.h>

/**
 * @brief Converts a duration in seconds to the equivalent number of 10 ms cycles.
 * @param seconds  Duration in whole seconds.
 * @returns        Equivalent cycle count (seconds * 100).
 */
static uint32_t sec_to_cycles_u32(uint32_t seconds)
{
    return seconds * (uint32_t)(100U);
}

/**
 * @brief Serializes FSM state and the most recent NVM_LOG_SNAPSHOT_ENTRIES log
 *        records to non-volatile memory and appends an EVT_NVM_WRITE entry to
 *        the event log.
 *
 * Log entries are written in chronological order: entry[0] is the oldest,
 * entry[n-1] is the most recent, using a descending back-index over evlog_get_recent.
 *
 * @param ctx  Pointer to the REB context. Must not be NULL.
 */
static void persist_state(reb_ctx_t *ctx)
{
    nvm_data_t nvm;
    uint16_t   n;
    uint16_t   i;

    (void)memset(&nvm, 0, sizeof(nvm));
    nvm.last_state          = ctx->fsm.state;
    nvm.last_block_phase    = ctx->fsm.block_phase;
    nvm.last_source         = ctx->fsm.source;
    nvm.last_nonce          = ctx->sec.last_nonce;
    nvm.parked_timer        = ctx->fsm.parked_timer;
    nvm.min_after_timer     = ctx->fsm.min_after_timer;
    nvm.derate_ramp         = ctx->derate_ramp;
    nvm.panel_wrong_cnt     = ctx->panel.wrong_cnt;
    nvm.lockout_active      = ctx->panel.lockout_active;
    nvm.lockout_remaining_s = (ctx->panel.lockout_timer / sec_to_cycles_u32(1U));

    n = (ctx->log.count < (uint16_t)NVM_LOG_SNAPSHOT_ENTRIES)
        ? ctx->log.count
        : (uint16_t)NVM_LOG_SNAPSHOT_ENTRIES;
    for (i = 0U; i < n; i++) {
        const event_record_t *rec = evlog_get_recent(&ctx->log,
                                                      (uint16_t)(n - 1U - i));
        if (rec != NULL) {
            nvm.log_entries[i] = *rec;
        }
    }
    nvm.log_head  = n;
    nvm.log_count = ctx->log.count;

    (void)nvm_write_state(&nvm);

    evlog_write(&ctx->log, ctx->fsm.sim_time_ms,
                EVT_NVM_WRITE,
                ctx->fsm.prev_state, ctx->fsm.state,
                (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
}

/**
 * @brief Executes a state transition, writes an EVT_STATE_TRANSITION log entry,
 *        and persists the new state to NVM.
 * @param ctx        REB context.
 * @param new_state  Target state.
 */
static void do_transition(reb_ctx_t *ctx, reb_state_t new_state)
{
    ctx->fsm.prev_state = ctx->fsm.state;
    ctx->fsm.state      = new_state;

    evlog_write(&ctx->log, ctx->fsm.sim_time_ms,
                EVT_STATE_TRANSITION,
                ctx->fsm.prev_state, new_state,
                (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);

    persist_state(ctx);
}

/**
 * @brief Returns true when the powertrain speed signal is in SIG_VALID state.
 * @param ctx  REB context (read-only).
 */
static bool speed_is_valid(const reb_ctx_t *ctx)
{
    return (ctx->fsm.speed_signal_status == SIG_VALID);
}

/**
 * @brief Verifies a remote command using the security manager.
 *
 * On verification failure an EVT_AUTH_FAIL entry is written to the event log
 * by sec_mgr_verify() internally.
 *
 * @param ctx  REB context.
 * @param in   Current step inputs containing nonce, timestamp, and signature flag.
 * @returns    True if the command passes all security checks.
 */
static bool check_remote_cmd(reb_ctx_t *ctx, const reb_inputs_t *in)
{
    auth_fail_t result;
    bool ok;

    ok = sec_mgr_verify(&ctx->sec,
                        in->cmd_nonce,
                        in->cmd_timestamp_ms,
                        in->cmd_sig_ok,
                        in->sim_time_ms,
                        &result);
    if (!ok) {
        evlog_write(&ctx->log, in->sim_time_ms,
                    EVT_AUTH_FAIL,
                    ctx->fsm.state, ctx->fsm.state,
                    (uint8_t)SOURCE_REMOTE, (uint8_t)result);
    }
    return ok;
}

/**
 * @brief Advances the fuel derating ramp by one step and enforces the minimum floor.
 *
 * Decrements derate_ramp by DERATE_RATE_PCT_S * REB_TS_S per call and clamps
 * the result to FUEL_FLOOR_PCT from below.
 *
 * @param ctx  REB context; derate_ramp is modified in place.
 * @returns    Current derate percentage as uint8_t.
 */
static uint8_t calc_derate_ramp(reb_ctx_t *ctx)
{
    float step = DERATE_RATE_PCT_S * REB_TS_S;

    if (ctx->derate_ramp > step) {
        ctx->derate_ramp -= step;
    } else {
        ctx->derate_ramp = 0.0f;
    }

    if (ctx->derate_ramp < (float)FUEL_FLOOR_PCT) {
        ctx->derate_ramp = (float)FUEL_FLOOR_PCT;
    }

    return (uint8_t)ctx->derate_ramp;
}

/**
 * @brief Resets all timers, sub-module states, and output fields to IDLE defaults.
 *
 * Intended to be called at the start of every entry into STATE_IDLE so that
 * no residual derating, alert, or reversal-window state persists.
 *
 * @param ctx  REB context.
 * @param out  Output structure; relevant fields are cleared.
 */
static void reset_idle_outputs(reb_ctx_t *ctx, reb_outputs_t *out)
{
    ctx->fsm.parked_timer   = 0U;
    ctx->fsm.min_after_timer = 0U;
    ctx->fsm.block_phase    = BLOCK_PHASE_PREALERT;
    ctx->derate_ramp        = (float)DERATE_PCT_INIT;
    ctx->fsm.unblock_requested = false;
    ctx->status_timer       = 0U;
    ctx->derate_timer       = 0U;

    act_init(&ctx->act);
    starter_release(&ctx->starter, &ctx->act);
    alert_mgr_stop(&ctx->alert, NULL);
    rw_init(&ctx->rw);
    sf_init(&ctx->sf);

    out->notify_theft      = false;
    out->notify_blocked    = false;
    out->gps_send          = false;
    out->reb_status_tx_due = false;
    out->derate_cmd_tx_due = false;
    out->pre_block_alert   = false;
    out->reversal_timer_s  = 0U;
    out->alert_sonic       = false;
    out->alert_visual      = false;
    out->hmi_alert_code    = HMI_ALERT_NONE;
}

/**
 * @brief STATE_IDLE entry action: resets all outputs and persists state to NVM.
 * @param ctx  REB context.
 * @param out  Output structure to reset.
 */
static void entry_idle(reb_ctx_t *ctx, reb_outputs_t *out)
{
    reset_idle_outputs(ctx, out);
    persist_state(ctx);
}

/**
 * @brief STATE_IDLE per-step logic.
 *
 * Evaluates panel authentication, remote 4G/5G block commands, SMS fallback
 * block commands, and sensor fusion in that priority order. On any accepted
 * activation, transitions to STATE_THEFT_CONFIRMED and calls entry_theft_confirmed().
 *
 * Remote block via IP channel: rejected with EVT_BLOCK_REJECT_SIGNAL when
 * channel_rx_ok is false, or EVT_BLOCK_REJECT_SPEED when powertrain is invalid.
 *
 * @param ctx  REB context.
 * @param in   Current step inputs.
 * @param out  Output structure updated in place.
 */
static void during_idle(reb_ctx_t *ctx,
                        const reb_inputs_t *in,
                        reb_outputs_t *out)
{
    sf_output_t sf_out;
    bool panel_auth_ok = false;
    bool panel_locked  = false;
    bool request_present;
 

    out->sensor_score = 0.0f;

    uint8_t wrong_before = ctx->panel.wrong_cnt;
    panel_auth_step(&ctx->panel,
                    in->auth_manual_out,
                    in->password_attempt,
                    in->cancel_request,
                    &panel_auth_ok,
                    &panel_locked);

    out->panel_password_ok   = panel_auth_ok;
    out->panel_locked_out     = panel_locked;
    out->panel_attempt_count  = ctx->panel.wrong_cnt;

    if (ctx->panel.wrong_cnt != wrong_before) {
        if (panel_locked) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_PANEL_LOCKOUT,
                        STATE_IDLE, STATE_IDLE,
                        (uint8_t)SOURCE_PANEL, (uint8_t)AUTH_OK);
        } else {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_AUTH_FAIL,
                        STATE_IDLE, STATE_IDLE,
                        (uint8_t)SOURCE_PANEL, (uint8_t)AUTH_SIG_INVALID);
        }
    }

    if (panel_auth_ok) {
        evlog_write(&ctx->log, in->sim_time_ms, EVT_AUTH_OK,
                    STATE_IDLE, STATE_THEFT_CONFIRMED,
                    (uint8_t)SOURCE_PANEL, (uint8_t)AUTH_OK);
        ctx->fsm.source = SOURCE_PANEL;
        do_transition(ctx, STATE_THEFT_CONFIRMED);
        return;
    }

    if (in->auth_blocked_remote) {
        if (!ctx->channel_rx_ok) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_BLOCK_REJECT_SIGNAL,
                        STATE_IDLE, STATE_IDLE,
                        (uint8_t)SOURCE_REMOTE, (uint8_t)AUTH_OK);
        } else if (!speed_is_valid(ctx)) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_CMD_RECEIVED,
                        STATE_IDLE, STATE_IDLE,
                        (uint8_t)SOURCE_REMOTE, (uint8_t)AUTH_OK);
            evlog_write(&ctx->log, in->sim_time_ms, EVT_BLOCK_REJECT_SPEED,
                        STATE_IDLE, STATE_IDLE,
                        (uint8_t)SOURCE_REMOTE, (uint8_t)AUTH_OK);
        } else if (check_remote_cmd(ctx, in)) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_CMD_RECEIVED,
                        STATE_IDLE, STATE_THEFT_CONFIRMED,
                        (uint8_t)SOURCE_REMOTE, (uint8_t)AUTH_OK);
            ctx->fsm.source = SOURCE_REMOTE;
            do_transition(ctx, STATE_THEFT_CONFIRMED);
            return;
        }
    }

    if (!in->ip_rx_ok && in->sms_rx_ok && in->auth_blocked_remote) {
        if (ctx->channel_rx_ok && speed_is_valid(ctx) && check_remote_cmd(ctx, in)) {
            ctx->fsm.source = SOURCE_REMOTE;
            do_transition(ctx, STATE_THEFT_CONFIRMED);
            return;
        }
        evlog_write(&ctx->log, in->sim_time_ms, EVT_BLOCK_REJECT_SIGNAL,
                    STATE_IDLE, STATE_IDLE,
                    (uint8_t)SOURCE_REMOTE, (uint8_t)AUTH_OK);
    }

    sf_step(&ctx->sf, in->accel_peak, in->glass_break_flag, &sf_out);
    out->sensor_score = sf_out.theft_score;

    request_present = (sf_out.theft_detected ||
                       in->auth_manual_out ||
                       in->auth_blocked_remote);

    if (request_present && !out->powertrain_valid) {
        evlog_write(&ctx->log, in->sim_time_ms, EVT_BLOCK_REJECT_SPEED,
                    STATE_IDLE, STATE_IDLE,
                    (uint8_t)SOURCE_AUTO, (uint8_t)AUTH_OK);
    }

    if (speed_is_valid(ctx) && sf_out.theft_detected) {
        evlog_write(&ctx->log, in->sim_time_ms, EVT_SENSOR_THEFT,
                    STATE_IDLE, STATE_THEFT_CONFIRMED,
                    (uint8_t)SOURCE_AUTO, (uint8_t)AUTH_OK);
        ctx->fsm.source = SOURCE_AUTO;
        do_transition(ctx, STATE_THEFT_CONFIRMED);
        return;
    }
}

/**
 * @brief STATE_THEFT_CONFIRMED entry action.
 *
 * For SOURCE_AUTO, start(rw_start).
 * For all other sources, the reversal window is cancelled so that the
 * minimum dwell timer governs the transition to BLOCKING.
 *
 * @param ctx  REB context.
 * @param in   Current step inputs (used for sim_time_ms in log write).
 * @param out  Output structure; notify_theft and gps_send are set.
 */
static void entry_theft_confirmed(reb_ctx_t *ctx, const reb_inputs_t *in,
                                  reb_outputs_t *out)
{
    out->notify_theft     = true;
    out->gps_send         = true;
    ctx->fsm.min_after_timer = 0U;
    ctx->fsm.parked_timer    = 0U;
    ctx->fsm.block_phase     = BLOCK_PHASE_PREALERT;

    if (ctx->fsm.source == SOURCE_AUTO) {
        rw_start(&ctx->rw, RW_MODE_60);
    } else {
        rw_cancel(&ctx->rw);
    }

    evlog_write(&ctx->log, in->sim_time_ms, EVT_STATE_TRANSITION,
                STATE_IDLE, STATE_THEFT_CONFIRMED,
                (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
}

/**
 * @brief STATE_THEFT_CONFIRMED per-step logic.
 *
 * For SOURCE_AUTO: enforces the 
 * activating alerts), evaluates the reversal window, and handles explicit owner
 * confirmation (auth_block_automatic == 1 → advance) or denial (== 2 → abort).
 * An invalid speed signal in this state causes an immediate return to IDLE and
 * sets signal_fault_locked.
 *
 * For SOURCE_PANEL / SOURCE_REMOTE: waits T_MIN_AFTER_CYCLES before transitioning
 * to BLOCKING. An invalid speed signal similarly triggers a return to IDLE.
 *
 * Remote unblock with TCU ACK returns to IDLE from either source path.
 *
 * @param ctx  REB context.
 * @param in   Current step inputs.
 * @param out  Output structure updated in place.
 */
static void during_theft_confirmed(reb_ctx_t *ctx,
                                   const reb_inputs_t *in,
                                   reb_outputs_t *out)
{
    rw_result_t rw_res;
    bool panel_auth_ok = false;
    bool panel_locked  = false;
    bool password_valid = false;
    sf_output_t sf_out;

    out->notify_theft = true;
    out->gps_send     = true;

    panel_auth_step(&ctx->panel,
                    in->auth_manual_out,
                    in->password_attempt,
                    in->cancel_request,
                    &panel_auth_ok,
                    &panel_locked);

    out->panel_password_ok  = panel_auth_ok;
    out->panel_locked_out   = panel_locked;
    out->panel_attempt_count = ctx->panel.wrong_cnt;

    password_valid = panel_auth_ok;

    if (in->remote_unblock_remote && ctx->channel_rx_ok && check_remote_cmd(ctx, in)) {
        if (in->tcu_ack) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_UNBLOCK,
                        STATE_THEFT_CONFIRMED, STATE_IDLE,
                        (uint8_t)SOURCE_REMOTE, (uint8_t)AUTH_OK);
            ctx->fsm.source = SOURCE_REMOTE;
            do_transition(ctx, STATE_IDLE);
            entry_idle(ctx, out);
            return;
        }
    }

    if (ctx->fsm.source == SOURCE_AUTO) {
        if (!speed_is_valid(ctx)) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_SIGNAL_FAULT,
                        STATE_THEFT_CONFIRMED, STATE_IDLE,
                        (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
            ctx->signal_fault_locked = true;
            alert_mgr_stop(&ctx->alert, NULL);
            do_transition(ctx, STATE_IDLE);
            entry_idle(ctx, out);
            return;
        }
        out->pre_block_alert = true;
        alert_mgr_start(&ctx->alert);

        /**
         * auth_block_automatic == 2: explicit owner denial; abort immediately.
         * auth_block_automatic == 1: explicit owner confirmation; advance to BLOCKING.
         */
        if (in->auth_block_automatic == 2U) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_REVERSAL_ABORT,
                        STATE_THEFT_CONFIRMED, STATE_IDLE,
                        (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
            alert_mgr_stop(&ctx->alert, NULL);
            do_transition(ctx, STATE_IDLE);
            entry_idle(ctx, out);
            return;
        }
        if (in->auth_block_automatic == 1U) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_REVERSAL_EXPIRE,
                        STATE_THEFT_CONFIRMED, STATE_BLOCKING,
                        (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
            alert_mgr_stop(&ctx->alert, NULL);
            do_transition(ctx, STATE_BLOCKING);
            return;
        }


        rw_res = rw_step(&ctx->rw, password_valid);
        out->reversal_timer_s = rw_remaining_s(&ctx->rw);

        if (rw_res == RW_ABORT) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_REVERSAL_ABORT,
                        STATE_THEFT_CONFIRMED, STATE_IDLE,
                        (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
            alert_mgr_stop(&ctx->alert, NULL);
            do_transition(ctx, STATE_IDLE);
            entry_idle(ctx, out);
            return;
        }
        if (rw_res == RW_EXPIRE) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_REVERSAL_EXPIRE,
                        STATE_THEFT_CONFIRMED, STATE_BLOCKING,
                        (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
            do_transition(ctx, STATE_BLOCKING);
            return;
        }

        ctx->fsm.min_after_timer++;
    } else {
        if (!speed_is_valid(ctx)) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_SIGNAL_FAULT,
                        STATE_THEFT_CONFIRMED, STATE_IDLE,
                        (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
            ctx->signal_fault_locked = true;
            do_transition(ctx, STATE_IDLE);
            entry_idle(ctx, out);
            return;
        }
        ctx->fsm.min_after_timer++;
        if (ctx->fsm.min_after_timer >= (uint32_t)T_MIN_AFTER_CYCLES) {
            do_transition(ctx, STATE_BLOCKING);
            return;
        }
    }

    sf_step(&ctx->sf, in->accel_peak, in->glass_break_flag, &sf_out);
    out->sensor_score = sf_out.theft_score;
}

/**
 * @brief STATE_BLOCKING entry action.
 *
 * Resets the parked timer, sets the block phase to BLOCK_PHASE_DERATING,
 * marks the reversal window as actuation-issued, and logs EVT_DERATE_ACTIVE.
 *
 * @param ctx  REB context.
 */
static void entry_blocking(reb_ctx_t *ctx)
{
    ctx->fsm.parked_timer = 0U;
    ctx->fsm.block_phase  = BLOCK_PHASE_DERATING;
    rw_set_actuation_issued(&ctx->rw);

    evlog_write(&ctx->log, ctx->fsm.sim_time_ms,
                EVT_DERATE_ACTIVE,
                STATE_THEFT_CONFIRMED, STATE_BLOCKING,
                (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
}

/**
 * @brief STATE_BLOCKING per-step logic.
 *
 * While vehicle speed exceeds V_STOP_KMH, advances the progressive derating
 * ramp and keeps block_phase at BLOCK_PHASE_DERATING. Once speed falls to or
 * below V_STOP_KMH, increments the parked dwell timer; when it reaches
 * T_PARKED_CYCLES, transitions to STATE_BLOCKED.
 *
 * An invalid speed signal causes an immediate return to STATE_IDLE and sets
 * signal_fault_locked.
 *
 * Remote unblock (with TCU ACK) or successful panel authentication both
 * abort the blocking sequence and return to STATE_IDLE.
 *
 * Audible alerts are active only for SOURCE_AUTO activations.
 *
 * @param ctx  REB context.
 * @param in   Current step inputs.
 * @param out  Output structure updated in place.
 */
static void during_blocking(reb_ctx_t *ctx,
                             const reb_inputs_t *in,
                             reb_outputs_t *out)
{
    uint8_t derate_raw;
    bool speed_valid;
    float spd;
    bool panel_auth_ok = false;
    bool panel_locked  = false;

    speed_valid = speed_is_valid(ctx);
    spd         = in->vehicle_speed_kmh;

    if (!speed_valid) {
        evlog_write(&ctx->log, in->sim_time_ms, EVT_SIGNAL_FAULT,
                    STATE_BLOCKING, STATE_IDLE,
                    (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
        ctx->signal_fault_locked = true;
        ctx->derate_ramp = (float)DERATE_PCT_INIT;
        act_init(&ctx->act);
        do_transition(ctx, STATE_IDLE);
        entry_idle(ctx, out);
        return;
    }



    if (spd > V_STOP_KMH) {
        ctx->fsm.block_phase = BLOCK_PHASE_DERATING;
        ctx->fsm.parked_timer = 0U;
        derate_raw = calc_derate_ramp(ctx);
        act_apply_derate(STATE_BLOCKING, spd, derate_raw, &ctx->act);
    } else {
        ctx->fsm.block_phase = BLOCK_PHASE_PARKED;
        ctx->fsm.parked_timer++;
    }

    out->derate_pct           = ctx->act.derate_pct;
    out->fuel_derating_active = ctx->act.fuel_derating_active;
    out->block_phase          = ctx->fsm.block_phase;

    if (ctx->fsm.parked_timer >= (uint32_t)T_PARKED_CYCLES) {
        evlog_write(&ctx->log, in->sim_time_ms, EVT_SPEED_SAFE_STOP,
                    STATE_BLOCKING, STATE_BLOCKED,
                    (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
        do_transition(ctx, STATE_BLOCKED);
        return;
    }

    if (ctx->fsm.source == SOURCE_AUTO) {
        alert_mgr_start(&ctx->alert);
    } else {
        alert_mgr_stop(&ctx->alert, NULL);
    }

    if (in->remote_unblock_remote && ctx->channel_rx_ok && check_remote_cmd(ctx, in)) {
        if (in->tcu_ack) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_UNBLOCK,
                        STATE_BLOCKING, STATE_IDLE,
                        (uint8_t)SOURCE_REMOTE, (uint8_t)AUTH_OK);
            ctx->fsm.source = SOURCE_REMOTE;
            starter_release(&ctx->starter, &ctx->act);
            do_transition(ctx, STATE_IDLE);
            entry_idle(ctx, out);
            return;
        }
    }

    panel_auth_step(&ctx->panel,
                    in->auth_manual_out,
                    in->password_attempt,
                    in->cancel_request,
                    &panel_auth_ok,
                    &panel_locked);

    out->panel_password_ok   = panel_auth_ok;
    out->panel_locked_out    = panel_locked;
    out->panel_attempt_count = ctx->panel.wrong_cnt;

    if (panel_auth_ok) {
        evlog_write(&ctx->log, in->sim_time_ms, EVT_UNBLOCK,
                    STATE_BLOCKING, STATE_IDLE,
                    (uint8_t)SOURCE_PANEL, (uint8_t)AUTH_OK);
        ctx->fsm.source = SOURCE_PANEL;
        starter_release(&ctx->starter, &ctx->act);
        do_transition(ctx, STATE_IDLE);
        entry_idle(ctx, out);
        return;
    }

    out->notify_theft    = (ctx->fsm.source == SOURCE_AUTO);
    out->pre_block_alert = (ctx->fsm.source == SOURCE_AUTO);
}

/**
 * @brief STATE_BLOCKED entry action.
 *
 * Releases fuel derating (starter inhibit assumes primary restraint),
 * asserts starter_inhibit_active, sets all relevant output flags, and logs
 * EVT_STARTER_INHIBIT.
 *
 * @param ctx  REB context.
 * @param out  Output structure updated with blocked-state values.
 */
static void entry_blocked(reb_ctx_t *ctx, reb_outputs_t *out)
{
    act_apply_derate(STATE_BLOCKED, 0.0f, (uint8_t)DERATE_PCT_INIT, &ctx->act);
    act_set_starter_inhibit(true, &ctx->act);

    ctx->fsm.block_phase = BLOCK_PHASE_PARKED;

    out->derate_pct             = ctx->act.derate_pct;
    out->starter_ok             = ctx->act.starter_ok;
    out->fuel_derating_active   = false;
    out->starter_inhibit_active = true;
    out->derate_cmd_tx_due      = false;
    out->notify_blocked         = true;
    out->notify_theft           = false;
    out->gps_send               = true;
    out->blocked_flag           = true;
    out->parked                 = 1U;
    out->derating_active        = false;
    ctx->derate_timer           = 0U;

    evlog_write(&ctx->log, ctx->fsm.sim_time_ms, EVT_STARTER_INHIBIT,
                STATE_BLOCKING, STATE_BLOCKED,
                (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
}

/**
 * @brief STATE_BLOCKED per-step logic.
 *
 * Sustains starter inhibit, keeps visual alert active, suppresses audible alert,
 * and evaluates unblock conditions. Remote unblock requires channel availability,
 * command authentication, and TCU acknowledgement. Panel unblock requires a valid
 * password authentication result.
 *
 * @param ctx  REB context.
 * @param in   Current step inputs.
 * @param out  Output structure updated in place.
 */
static void during_blocked(reb_ctx_t *ctx,
                           const reb_inputs_t *in,
                           reb_outputs_t *out)
{
    bool panel_auth_ok = false;
    bool panel_locked  = false;

    starter_step(&ctx->starter, STATE_BLOCKED, &ctx->act);

    out->derate_pct             = ctx->act.derate_pct;
    out->starter_ok             = ctx->act.starter_ok;
    out->fuel_derating_active   = false;
    out->starter_inhibit_active = true;
    out->derate_cmd_tx_due      = false;
    out->notify_blocked         = true;
    out->notify_theft           = false;
    out->blocked_flag           = true;
    out->parked                 = 1U;
    out->derating_active        = false;

    alert_mgr_stop(&ctx->alert, NULL);
    out->alert_visual = true;
    out->alert_sonic   = false;

    if (in->remote_unblock_remote && ctx->channel_rx_ok && check_remote_cmd(ctx, in)) {
        if (in->tcu_ack) {
            evlog_write(&ctx->log, in->sim_time_ms, EVT_UNBLOCK,
                        STATE_BLOCKED, STATE_IDLE,
                        (uint8_t)SOURCE_REMOTE, (uint8_t)AUTH_OK);
            ctx->fsm.source = SOURCE_REMOTE;
            starter_release(&ctx->starter, &ctx->act);
            do_transition(ctx, STATE_IDLE);
            entry_idle(ctx, out);
            return;
        }
    }

    panel_auth_step(&ctx->panel,
                    in->auth_manual_out,
                    in->password_attempt,
                    in->cancel_request,
                    &panel_auth_ok,
                    &panel_locked);

    out->panel_password_ok   = panel_auth_ok;
    out->panel_locked_out    = panel_locked;
    out->panel_attempt_count = ctx->panel.wrong_cnt;

    if (panel_auth_ok) {
        evlog_write(&ctx->log, in->sim_time_ms, EVT_UNBLOCK,
                    STATE_BLOCKED, STATE_IDLE,
                    (uint8_t)SOURCE_PANEL, (uint8_t)AUTH_OK);
        ctx->fsm.source = SOURCE_PANEL;
        starter_release(&ctx->starter, &ctx->act);
        do_transition(ctx, STATE_IDLE);
        entry_idle(ctx, out);
        return;
    }

    out->gps_send = true;
}

/**
 * @brief Initializes the complete REB context and restores persisted state from NVM.
 *
 * All sub-modules are initialized first in their reset state. If NVM contains a
 * valid snapshot, FSM state, block phase, source, nonce, timers, panel counters,
 * derate ramp, and the event log snapshot are all restored. A STATE_BLOCKED
 * restore immediately re-asserts starter inhibit. An EVT_NVM_RESTORE log entry
 * is written after a successful restore.
 *
 * @param ctx  Pointer to the REB context to initialize. Must not be NULL.
 */
void reb_fsm_init(reb_ctx_t *ctx)
{
    nvm_data_t nvm;
    nvm_result_t nvm_res;

    (void)memset(ctx, 0, sizeof(*ctx));

    sec_mgr_init(&ctx->sec);
    panel_auth_init(&ctx->panel);
    sf_init(&ctx->sf);
    pwt_init(&ctx->pwt);
    act_init(&ctx->act);
    starter_init(&ctx->starter);
    alert_mgr_init(&ctx->alert);
    rw_init(&ctx->rw);
    evlog_init(&ctx->log);
    can_rx_watchdog_init(&ctx->rxwd);

    ctx->derate_ramp         = (float)DERATE_PCT_INIT;
    ctx->fsm.state           = STATE_IDLE;
    ctx->fsm.prev_state      = STATE_IDLE;
    ctx->fsm.source          = SOURCE_REMOTE;
    ctx->fsm.block_phase     = BLOCK_PHASE_PREALERT;
    ctx->channel_rx_ok       = true;
    ctx->rx_fail             = false;
    ctx->rx_channel_id       = 0;
    ctx->status_timer        = 0U;
    ctx->derate_timer        = 0U;
    ctx->signal_fault_locked = false;


    nvm_res = nvm_read_state(&nvm);
    if (nvm_res == NVM_OK) {
        uint16_t n;
        uint16_t i;

        ctx->fsm.state           = nvm.last_state;
        ctx->fsm.prev_state      = nvm.last_state;
        ctx->fsm.source          = nvm.last_source;
        ctx->fsm.block_phase     = nvm.last_block_phase;
        ctx->fsm.parked_timer    = nvm.parked_timer;
        ctx->fsm.min_after_timer = nvm.min_after_timer;
        ctx->sec.last_nonce      = nvm.last_nonce;
        ctx->derate_ramp         = nvm.derate_ramp;
        ctx->panel.wrong_cnt     = nvm.panel_wrong_cnt;
        ctx->panel.lockout_active = nvm.lockout_active;
        ctx->panel.lockout_timer  = (uint32_t)(nvm.lockout_remaining_s * 100U);
        ctx->fsm.nvm_state_loaded = true;

        /**
         * Restore event log snapshot in chronological order to maintain
         * forensic continuity across power cycles.
         */
        n = (nvm.log_head < (uint16_t)NVM_LOG_SNAPSHOT_ENTRIES)
            ? nvm.log_head
            : (uint16_t)NVM_LOG_SNAPSHOT_ENTRIES;
        for (i = 0U; i < n; i++) {
            ctx->log.entries[i] = nvm.log_entries[i];
        }
        ctx->log.head  = (uint16_t)(n % (uint16_t)EVENT_LOG_MAX_ENTRIES);
        ctx->log.count = n;

        evlog_write(&ctx->log, 0U, EVT_NVM_RESTORE,
                    STATE_IDLE, nvm.last_state,
                    (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);

        if (ctx->fsm.state == STATE_BLOCKED) {
            act_set_starter_inhibit(true, &ctx->act);
            ctx->starter.inhibit_active = true;
        }
    }

}

/**
 * @brief Executes one FSM step: updates all sub-modules and dispatches to the
 *        active state handler.
 *
 * Processing order per step:
 *  1. CAN reception watchdog (channel availability, rx_fail detection).
 *  2. Powertrain validation (speed, ignition, brake signal integrity).
 *  3. Active state handler (during_*, with optional entry_* on transition).
 *  4. Alert manager step (horn oscillation update).
 *  5. Output assembly: actuator values override individual state outputs;
 *     alert_visual is forced true and alert_sonic false in STATE_BLOCKED.
 *  6. Periodic transmission flags: reb_status_tx_due (STATUS_PERIOD_CYCLES),
 *     derate_cmd_tx_due (DERATE_CMD_PERIOD_CYCLES, only while derating active).
 *
 * @param ctx  REB context carrying all persistent state.
 * @param in   Pointer to the current step's input snapshot. Must not be NULL.
 * @param out  Pointer to the output structure to populate. Must not be NULL.
 */
void reb_fsm_step(reb_ctx_t *ctx,
                  const reb_inputs_t *in,
                  reb_outputs_t *out)
{
    alert_output_t alert_out;
    pwt_output_t pwt_out;
    uint32_t timeout_ms;

    ctx->fsm.sim_time_ms = in->sim_time_ms;

    (void)memset(out, 0, sizeof(*out));
    out->derate_pct           = (uint8_t)DERATE_PCT_INIT;
    out->starter_ok           = !ctx->act.starter_inhibit_active;
    out->current_state        = ctx->fsm.state;
    out->nvm_state_loaded     = ctx->fsm.nvm_state_loaded;
    out->block_phase          = ctx->fsm.block_phase;
    out->reb_status_tx_due    = false;
    out->derate_cmd_tx_due    = false;
    out->hmi_alert_code       = HMI_ALERT_NONE;

    timeout_ms = (uint32_t)ACK_TIMEOUT_CYCLES * (uint32_t)REB_TS_MS;
    can_rx_watchdog_step(&ctx->rxwd,
                         in->ip_rx_ok,
                         in->sms_rx_ok,
                         (uint32_t)REB_TS_MS,
                         timeout_ms,
                         (uint8_t)MAX_RETRIES,
                         &ctx->channel_rx_ok,
                         &ctx->rx_fail,
                         &ctx->rx_channel_id);
    out->channel_rx_ok = ctx->channel_rx_ok;
    out->rx_fail       = ctx->rx_fail;
    out->rx_channel_id = ctx->rx_channel_id;

    if (ctx->rx_fail) {
        evlog_write(&ctx->log, in->sim_time_ms, EVT_RX_SUPERVISION_FAIL,
                    ctx->fsm.state, ctx->fsm.state,
                    (uint8_t)ctx->fsm.source, (uint8_t)AUTH_OK);
    }

    pwt_step(&ctx->pwt,
             in->vehicle_speed_kmh,
             in->ignition_state,
             in->brake_pedal,
             &pwt_out);

    ctx->fsm.speed_signal_status =
        (pwt_out.powertrain_valid && (in->speed_sig_status == SIG_VALID))
            ? SIG_VALID
            : SIG_MISSING;

    out->powertrain_valid = (ctx->fsm.speed_signal_status == SIG_VALID);
    out->speed_valid      = pwt_out.speed_valid;
    out->ign_valid        = pwt_out.ign_valid;
    out->brake_valid      = pwt_out.brake_valid;
    out->pt_fault_code    = pwt_out.pt_fault_code;

    switch (ctx->fsm.state) {
        case STATE_IDLE:
            during_idle(ctx, in, out);
            if (ctx->fsm.state == STATE_THEFT_CONFIRMED) {
                entry_theft_confirmed(ctx, in, out);
            }
            break;

        case STATE_THEFT_CONFIRMED:
            during_theft_confirmed(ctx, in, out);
            if (ctx->fsm.state == STATE_BLOCKING) {
                entry_blocking(ctx);
            }
            break;

        case STATE_BLOCKING:
            during_blocking(ctx, in, out);
            if (ctx->fsm.state == STATE_BLOCKED) {
                entry_blocked(ctx, out);
            }
            break;

        case STATE_BLOCKED:
            during_blocked(ctx, in, out);
            break;

        default:
            ctx->fsm.state = STATE_IDLE;
            entry_idle(ctx, out);
            break;
    }

    (void)memset(&alert_out, 0, sizeof(alert_out));
    alert_mgr_step(&ctx->alert, &alert_out);

    out->alert_sonic    = alert_out.horn_active;
    out->alert_visual   = alert_out.hazard_active;
    out->hmi_alert_code = alert_out.hmi_code;

    if (ctx->fsm.state == STATE_BLOCKED) {
        out->alert_visual = true;
        out->alert_sonic  = false;
    }

    out->derate_pct             = ctx->act.derate_pct;
    out->starter_ok             = ctx->act.starter_ok;
    out->fuel_derating_active   = ctx->act.fuel_derating_active;
    out->starter_inhibit_active = ctx->act.starter_inhibit_active;
    out->current_state          = ctx->fsm.state;
    out->blocked_flag           = (ctx->fsm.state == STATE_BLOCKED);
    out->parked                 = (ctx->fsm.state == STATE_BLOCKED) ? 1U : 0U;
    out->derating_active        = (ctx->fsm.state == STATE_BLOCKING) &&
                                  (ctx->fsm.block_phase == BLOCK_PHASE_DERATING);
    out->source_trigger_out     = (uint8_t)ctx->fsm.source;
    out->notify_blocked         = (ctx->fsm.state == STATE_BLOCKED);
    if (ctx->fsm.state == STATE_IDLE) {
        out->notify_theft = false;
    }
    out->gps_send               = (ctx->fsm.state != STATE_IDLE);
    out->pre_block_alert        = (ctx->fsm.state == STATE_THEFT_CONFIRMED) &&
                                  (ctx->fsm.source == SOURCE_AUTO);
    out->reversal_timer_s       = rw_remaining_s(&ctx->rw);

    out->panel_password_ok      = ctx->panel.auth_ok;
    out->panel_locked_out       = ctx->panel.lockout_active;
    out->panel_attempt_count    = ctx->panel.wrong_cnt;

    if (ctx->fsm.state == STATE_IDLE) {
        out->block_phase = BLOCK_PHASE_PREALERT;
    }

    ctx->status_timer++;
    if (ctx->status_timer >= (uint32_t)STATUS_PERIOD_CYCLES) {
        ctx->status_timer = 0U;
        out->reb_status_tx_due = true;
    }

    if ((ctx->fsm.state == STATE_BLOCKING) && out->fuel_derating_active) {
        ctx->derate_timer++;
        if (ctx->derate_timer >= (uint32_t)DERATE_CMD_PERIOD_CYCLES) {
            ctx->derate_timer = 0U;
            out->derate_cmd_tx_due = true;
        }
    } else {
        ctx->derate_timer = 0U;
    }
}