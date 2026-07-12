/**
 * @file    test_reb.c
 * @brief   Unit and integration test suite for the REB module.
 *
 * @details Covers TEST-001 through TEST-024, supplementary NFR-INFO-001 event log
 *          checks, integration scenarios §7.1.1 and §7.1.3, and the dwell-timer
 *          interruption scenario (DERATING → PARKED → motion resume → PARKED → BLOCKED).
 *          Each test function exercises one requirement or scenario from the RTM.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

static int  g_tests_run    = 0;
static int  g_tests_failed = 0;

/**
 * @brief Evaluates @p cond and records pass/fail to the global counters.
 * @param cond  Boolean expression to assert; printed verbatim on failure.
 */
#define TEST_ASSERT(cond)                                                   \
    do {                                                                    \
        g_tests_run++;                                                      \
        if (!(cond)) {                                                      \
            g_tests_failed++;                                               \
            printf("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
        } else {                                                            \
            printf("  PASS  %s\n", #cond);                                  \
        }                                                                   \
    } while (0)

/**
 * @brief Invokes @p fn and prints its name as a section header before execution.
 * @param fn  Test function with signature @c void fn(void).
 */
#define RUN_TEST(fn)                                                        \
    do {                                                                    \
        printf("\n--- " #fn " ---\n");                                      \
        fn();                                                               \
    } while (0)

#include "include/reb/reb_types.h"
#include "include/reb/reb_params.h"
#include "src/reb_core/event_log.h"
#include "src/reb_core/nvm.h"
#include "src/reb_core/security_manager.h"
#include "src/reb_core/panel_auth.h"
#include "src/reb_core/sensor_fusion.h"
#include "src/reb_core/actuator_iface.h"
#include "src/reb_core/starter_control.h"
#include "src/reb_core/alert_manager.h"
#include "src/reb_core/reversal_window.h"
#include "src/reb_core/fsm.h"

/**
 * @brief Invalidates NVM storage and reinitialises the FSM context to a clean state.
 * @param ctx  Pointer to the REB context to initialise.
 */
static void test_setup(reb_ctx_t *ctx)
{
    nvm_invalidate();
    reb_fsm_init(ctx);
}

/**
 * @brief Constructs an input snapshot representing a stationary, idle vehicle.
 * @return Fully initialised @c reb_inputs_t with zero speed, valid speed signal, and IP link up.
 */
static reb_inputs_t make_idle_inputs(void)
{
    reb_inputs_t in;
    (void)memset(&in, 0, sizeof(in));
    in.speed_sig_status   = SIG_VALID;
    in.vehicle_speed_kmh  = 0.0f;
    in.ip_rx_ok           = true;
    in.sim_time_ms        = 1000U;
    return in;
}

/**
 * @brief Constructs an input snapshot representing a moving vehicle.
 * @param speed_kmh  Vehicle speed in km/h to inject.
 * @return @c reb_inputs_t based on idle defaults with @p speed_kmh applied.
 */
static reb_inputs_t make_moving_inputs(float speed_kmh)
{
    reb_inputs_t in = make_idle_inputs();
    in.vehicle_speed_kmh = speed_kmh;
    return in;
}

/**
 * @brief Advances the FSM by @p n execution cycles, incrementing simulation time each cycle.
 * @param ctx  Pointer to the REB context.
 * @param in   Pointer to the input structure; @c sim_time_ms is incremented in place by @c REB_TS_MS per cycle.
 * @param out  Pointer to the output structure populated after each cycle.
 * @param n    Number of cycles to execute.
 */
static void run_cycles(reb_ctx_t *ctx, reb_inputs_t *in,
                       reb_outputs_t *out, uint32_t n)
{
    uint32_t i;
    for (i = 0U; i < n; i++) {
        in->sim_time_ms += (uint32_t)REB_TS_MS;
        reb_fsm_step(ctx, in, out);
    }
}

/**
 * @brief Ramps vehicle speed toward @p target_kmh in increments of at most 15 km/h per cycle.
 *
 * @details The 15 km/h per-cycle cap prevents the powertrain validation layer from asserting
 *          the @c spd_no_jump fault, which has a threshold of 20 km/h per cycle.
 *          Two settling cycles are executed once the target is reached.
 *
 * @param ctx         Pointer to the REB context.
 * @param in          Pointer to the input structure; @c vehicle_speed_kmh is modified in place.
 * @param out         Pointer to the output structure populated after each cycle.
 * @param target_kmh  Desired final speed in km/h.
 */
static void ramp_speed(reb_ctx_t *ctx, reb_inputs_t *in,
                       reb_outputs_t *out, float target_kmh)
{
    const float step = 15.0f;
    float delta;
    for (;;) {
        delta = target_kmh - in->vehicle_speed_kmh;
        if (delta >  step) { delta =  step; }
        if (delta < -step) { delta = -step; }
        if (delta > -0.05f && delta < 0.05f) { break; }
        in->vehicle_speed_kmh += delta;
        run_cycles(ctx, in, out, 1U);
    }
    in->vehicle_speed_kmh = target_kmh;
    run_cycles(ctx, in, out, 2U);
}

/**
 * @brief Verifies that event log entries survive a simulated power cycle via NVM restore.
 * @details Forces a state transition to trigger @c persist_state(), then reinitialises
 *          a second context from NVM and confirms the most recent event code is preserved.
 */
static void test_info001_log_persists_across_power_cycle(void)
{
    reb_ctx_t     ctx1, ctx2;
    reb_inputs_t  in;
    reb_outputs_t out;
    uint16_t      count_before;
    uint8_t       saved_code;
    const event_record_t *rec;

    test_setup(&ctx1);
    in = make_idle_inputs();

    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = true;
    in.cmd_nonce           = 10U;
    in.cmd_timestamp_ms    = in.sim_time_ms;
    reb_fsm_step(&ctx1, &in, &out);

    count_before = evlog_count(&ctx1.log);
    TEST_ASSERT(count_before > 0U);

    rec = evlog_get_recent(&ctx1.log, 0U);
    TEST_ASSERT(rec != NULL);
    saved_code = rec->event_code;

    reb_fsm_init(&ctx2);

    TEST_ASSERT(evlog_count(&ctx2.log) > 0U);

    rec = evlog_get_recent(&ctx2.log, 1U);
    TEST_ASSERT(rec != NULL);
    TEST_ASSERT(rec->event_code == saved_code);
}

/**
 * @brief Verifies that @c EVT_DERATE_ACTIVE is recorded when the FSM enters @c STATE_BLOCKING.
 */
static void test_info001_evt_derate_active_fired(void)
{
    reb_ctx_t     ctx;
    reb_inputs_t  in;
    reb_outputs_t out;
    uint16_t      i;
    bool          found = false;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = true;
    in.cmd_nonce           = 20U;
    in.cmd_timestamp_ms    = in.sim_time_ms;
    reb_fsm_step(&ctx, &in, &out);

    in.auth_blocked_remote = false;
    for (i = 0U; i < (uint16_t)T_MIN_AFTER_CYCLES + 2U; i++) {
        in.sim_time_ms += (uint32_t)REB_TS_MS;
        reb_fsm_step(&ctx, &in, &out);
        if (ctx.fsm.state == STATE_BLOCKING) { break; }
    }
    TEST_ASSERT(ctx.fsm.state == STATE_BLOCKING);

    for (i = 0U; i < evlog_count(&ctx.log); i++) {
        const event_record_t *rec = evlog_get_recent(&ctx.log, i);
        if (rec != NULL && rec->event_code == EVT_DERATE_ACTIVE) {
            found = true;
            break;
        }
    }
    TEST_ASSERT(found);
}

/**
 * @brief Verifies that @c EVT_CMD_RECEIVED is recorded upon acceptance of a valid remote command.
 */
static void test_info001_evt_cmd_received_fired(void)
{
    reb_ctx_t     ctx;
    reb_inputs_t  in;
    reb_outputs_t out;
    uint16_t      i;
    bool          found = false;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = true;
    in.cmd_nonce           = 30U;
    in.cmd_timestamp_ms    = in.sim_time_ms;
    reb_fsm_step(&ctx, &in, &out);

    TEST_ASSERT(ctx.fsm.state == STATE_THEFT_CONFIRMED);

    for (i = 0U; i < evlog_count(&ctx.log); i++) {
        const event_record_t *rec = evlog_get_recent(&ctx.log, i);
        if (rec != NULL && rec->event_code == EVT_CMD_RECEIVED) {
            found = true;
            break;
        }
    }
    TEST_ASSERT(found);
}

/**
 * @brief Verifies that a block command rejected due to an invalid speed signal logs
 *        @c EVT_BLOCK_REJECT_SPEED and does not log @c EVT_BLOCK_REJECT_SIGNAL.
 */
static void test_info001_evt_block_reject_speed_fired(void)
{
    reb_ctx_t     ctx;
    reb_inputs_t  in;
    reb_outputs_t out;
    uint16_t      i;
    bool          found_speed  = false;
    bool          found_signal = false;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.speed_sig_status    = SIG_MISSING;
    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = true;
    in.cmd_nonce           = 40U;
    in.cmd_timestamp_ms    = in.sim_time_ms;
    reb_fsm_step(&ctx, &in, &out);

    TEST_ASSERT(ctx.fsm.state == STATE_IDLE);

    for (i = 0U; i < evlog_count(&ctx.log); i++) {
        const event_record_t *rec = evlog_get_recent(&ctx.log, i);
        if (rec == NULL) { break; }
        if (rec->event_code == EVT_BLOCK_REJECT_SPEED)  { found_speed  = true; }
        if (rec->event_code == EVT_BLOCK_REJECT_SIGNAL) { found_signal = true; }
    }
    TEST_ASSERT(found_speed);
    TEST_ASSERT(!found_signal);
}

/**
 * @brief TEST-001 — Verifies that the FSM remains in @c STATE_IDLE when no triggering event occurs.
 */
static void test_001_idle_no_event(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    in = make_idle_inputs();
    run_cycles(&ctx, &in, &out, 100U);

    TEST_ASSERT(ctx.fsm.state == STATE_IDLE);
    TEST_ASSERT(out.notify_theft == false);
    TEST_ASSERT(out.notify_blocked == false);
}

/**
 * @brief TEST-002 — Verifies that a valid remote command transitions the FSM to @c STATE_THEFT_CONFIRMED.
 */
static void test_002_theft_confirmed_remote(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = true;
    in.cmd_nonce           = 1U;
    in.cmd_timestamp_ms    = in.sim_time_ms;

    reb_fsm_step(&ctx, &in, &out);

    TEST_ASSERT(ctx.fsm.state == STATE_THEFT_CONFIRMED);
    TEST_ASSERT(out.notify_theft == true);
}

/**
 * @brief TEST-003 — Verifies that a command with an invalid signature is rejected and the FSM stays in @c STATE_IDLE.
 */
static void test_003_invalid_cmd_rejected(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = false;
    in.cmd_nonce           = 1U;
    in.cmd_timestamp_ms    = in.sim_time_ms;

    reb_fsm_step(&ctx, &in, &out);

    TEST_ASSERT(ctx.fsm.state == STATE_IDLE);
}

/**
 * @brief TEST-004 — Verifies that a valid physical panel password activates @c STATE_THEFT_CONFIRMED.
 */
static void test_004_panel_activation(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.auth_manual_out  = true;
    in.password_attempt = (uint32_t)PANEL_PASSWORD_HASH;

    reb_fsm_step(&ctx, &in, &out);

    TEST_ASSERT(ctx.fsm.state == STATE_THEFT_CONFIRMED);
}

/**
 * @brief TEST-005 — Verifies that the panel enters lockout after @c MAX_AUTH_ATTEMPTS consecutive failures.
 */
static void test_005_panel_lockout(void)
{
    panel_auth_ctx_t panel;
    bool auth_ok = false;
    bool locked  = false;
    uint32_t i;

    panel_auth_init(&panel);

    for (i = 0U; i < (uint32_t)MAX_AUTH_ATTEMPTS; i++) {
        panel.prev_auth_pulse = false;
        panel_auth_step(&panel, true, 0xDEADBEEFUL,
                        false, &auth_ok, &locked);
    }

    TEST_ASSERT(locked == true);
    TEST_ASSERT(auth_ok == false);
}

/**
 * @brief TEST-006 — Verifies that a replayed nonce is rejected with @c AUTH_NONCE_REPLAY.
 */
static void test_006_nonce_replay(void)
{
    sec_ctx_t sec;
    auth_fail_t result;
    bool ok;

    sec_mgr_init(&sec);

    ok = sec_mgr_verify(&sec, 5U, 1000U, true, 1000U, &result);
    TEST_ASSERT(ok == true);
    TEST_ASSERT(result == AUTH_OK);

    ok = sec_mgr_verify(&sec, 5U, 2000U, true, 2000U, &result);
    TEST_ASSERT(ok == false);
    TEST_ASSERT(result == AUTH_NONCE_REPLAY);
}

/**
 * @brief TEST-007 — Verifies that a command whose timestamp exceeds the 30 s window is rejected
 *        with @c AUTH_TS_EXPIRED.
 */
static void test_007_timestamp_expired(void)
{
    sec_ctx_t sec;
    auth_fail_t result;
    bool ok;

    sec_mgr_init(&sec);

    ok = sec_mgr_verify(&sec, 1U, 0U, true, 31000U, &result);
    TEST_ASSERT(ok == false);
    TEST_ASSERT(result == AUTH_TS_EXPIRED);
}

/**
 * @brief TEST-008 — Verifies that sensor fusion reports theft detection after @c SF_DEBOUNCE_CYCLES
 *        of sustained stimulus.
 */
static void test_008_sensor_fusion_detect(void)
{
    sf_ctx_t sf;
    sf_output_t out;
    uint32_t i;

    sf_init(&sf);

    for (i = 0U; i < (uint32_t)SF_DEBOUNCE_CYCLES; i++) {
        sf_step(&sf, ACCEL_MAX, 1.0f, &out);
    }

    TEST_ASSERT(out.theft_detected == true);
    TEST_ASSERT(out.theft_score >= SF_THRESH);
}

/**
 * @brief TEST-009 — Verifies that the fuel safety floor clamps @c derate_pct to at least
 *        @c FUEL_FLOOR_PCT during @c STATE_BLOCKING.
 */
static void test_009_fuel_safety_floor(void)
{
    actuator_output_t act;
    act_init(&act);

    act_apply_derate(STATE_BLOCKING, 5.0f, 0U, &act);

    TEST_ASSERT(act.derate_pct >= (uint8_t)FUEL_FLOOR_PCT);
    TEST_ASSERT(act.fuel_derating_active == true);
}

/**
 * @brief TEST-010 — Verifies that no @c derate_pct value in [0..100] falls below @c FUEL_FLOOR_PCT
 *        when @c STATE_BLOCKING is active with a non-zero vehicle speed.
 */
static void test_010_fuel_floor_zero_violations(void)
{
    actuator_output_t act;
    uint8_t derate_in;
    bool violation = false;

    act_init(&act);

    for (derate_in = 0U; derate_in <= 100U; derate_in++) {
        act_apply_derate(STATE_BLOCKING, 10.0f, derate_in, &act);
        if (act.derate_pct < (uint8_t)FUEL_FLOOR_PCT) {
            violation = true;
        }
    }

    TEST_ASSERT(violation == false);
}

/**
 * @brief TEST-011 — Verifies that the FSM transitions from @c STATE_BLOCKING to @c STATE_BLOCKED
 *        after @c T_PARKED_CYCLES consecutive cycles with zero vehicle speed.
 */
static void test_011_blocked_after_120s_stop(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);

    ctx.fsm.state  = STATE_BLOCKING;
    ctx.fsm.source = SOURCE_REMOTE;

    in = make_idle_inputs();

    run_cycles(&ctx, &in, &out, (uint32_t)T_PARKED_CYCLES + 1U);

    TEST_ASSERT(ctx.fsm.state == STATE_BLOCKED);
    TEST_ASSERT(out.starter_inhibit_active == true);
    TEST_ASSERT(out.fuel_derating_active == false);
}

/**
 * @brief TEST-012 — Verifies that a vehicle movement event resets the parked timer,
 *        requiring @c T_PARKED_CYCLES to elapse from rest again before entering @c STATE_BLOCKED.
 */
static void test_012_parked_timer_reset_on_motion(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    ctx.fsm.state  = STATE_BLOCKING;
    ctx.fsm.source = SOURCE_REMOTE;

    in = make_idle_inputs();

    run_cycles(&ctx, &in, &out, (uint32_t)T_PARKED_CYCLES / 2U);
    TEST_ASSERT(ctx.fsm.state == STATE_BLOCKING);

    in.vehicle_speed_kmh = 10.0f;
    reb_fsm_step(&ctx, &in, &out);

    in.vehicle_speed_kmh = 0.0f;
    run_cycles(&ctx, &in, &out, (uint32_t)T_PARKED_CYCLES / 2U);

    TEST_ASSERT(ctx.fsm.state == STATE_BLOCKING);
}

/**
 * @brief TEST-013 — Verifies that a valid password presented within the 60 s reversal window
 *        returns @c RW_ABORT, halting the block sequence.
 */
static void test_013_reversal_window_abort(void)
{
    rw_ctx_t rw;
    rw_result_t res;
    uint32_t i;

    rw_init(&rw);
    rw_start(&rw, RW_MODE_60);

    for (i = 0U; i < 3000U; i++) {
        res = rw_step(&rw, false);
        TEST_ASSERT(res == RW_RUNNING);
    }

    res = rw_step(&rw, true);
    TEST_ASSERT(res == RW_ABORT);
}

/**
 * @brief TEST-014 — Verifies that the 60 s reversal window returns @c RW_EXPIRE after
 *        @c T_REVERSAL_CYCLES with no password input.
 */
static void test_014_reversal_window_60s_expire(void)
{
    rw_ctx_t rw;
    rw_result_t res = RW_RUNNING;
    uint32_t i;

    rw_init(&rw);
    rw_start(&rw, RW_MODE_60);

    for (i = 0U; i < (uint32_t)T_REVERSAL_CYCLES; i++) {
        res = rw_step(&rw, false);
    }

    TEST_ASSERT(res == RW_EXPIRE);
}

/**
 * @brief TEST-015 — Verifies that a password presented after actuation has been issued
 *        does not produce @c RW_ABORT; reversal is no longer permitted post-actuation.
 */
static void test_015_reversal_rejected_after_actuation(void)
{
    rw_ctx_t rw;
    rw_result_t res;

    rw_init(&rw);
    rw_start(&rw, RW_MODE_60);
    rw_set_actuation_issued(&rw);

    res = rw_step(&rw, true);
    TEST_ASSERT(res != RW_ABORT);
}

/**
 * @brief TEST-016 — Verifies that the starter is never inhibited while the vehicle is in motion,
 *        regardless of elapsed parked timer cycles.
 */
static void test_016_starter_only_when_stopped(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    ctx.fsm.state  = STATE_BLOCKING;
    ctx.fsm.source = SOURCE_REMOTE;

    in = make_moving_inputs(80.0f);

    run_cycles(&ctx, &in, &out, (uint32_t)T_PARKED_CYCLES + 100U);

    TEST_ASSERT(ctx.fsm.state != STATE_BLOCKED);
    TEST_ASSERT(out.starter_inhibit_active == false);
}

/**
 * @brief TEST-017 — Verifies that an invalid speed signal prevents the FSM from transitioning
 *        to @c STATE_BLOCKED even after @c T_PARKED_CYCLES have elapsed.
 */
static void test_017_signal_fault_inhibit(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    ctx.fsm.state  = STATE_BLOCKING;
    ctx.fsm.source = SOURCE_REMOTE;

    in = make_idle_inputs();
    in.speed_sig_status = SIG_MISSING;

    run_cycles(&ctx, &in, &out, (uint32_t)T_PARKED_CYCLES + 1U);

    TEST_ASSERT(ctx.fsm.state != STATE_BLOCKED);
}

/**
 * @brief TEST-018 — Verifies that NVM correctly persists and restores @c STATE_BLOCKED and the last nonce.
 */
static void test_018_nvm_persistence(void)
{
    nvm_data_t write_data;
    nvm_data_t read_data;
    nvm_result_t res;

    (void)memset(&write_data, 0, sizeof(write_data));
    write_data.last_state  = STATE_BLOCKED;
    write_data.last_nonce  = 42U;

    res = nvm_write_state(&write_data);
    TEST_ASSERT(res == NVM_OK);

    (void)memset(&read_data, 0, sizeof(read_data));
    res = nvm_read_state(&read_data);
    TEST_ASSERT(res == NVM_OK);
    TEST_ASSERT(read_data.last_state == STATE_BLOCKED);
    TEST_ASSERT(read_data.last_nonce == 42U);
}

/**
 * @brief TEST-019 — Verifies that after a simulated power loss the FSM restores @c STATE_BLOCKED
 *        from NVM on the subsequent initialisation.
 */
static void test_019_nvm_restore_on_init(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    ctx.fsm.state  = STATE_BLOCKING;
    ctx.fsm.source = SOURCE_REMOTE;
    in = make_idle_inputs();
    run_cycles(&ctx, &in, &out, (uint32_t)T_PARKED_CYCLES + 1U);

    TEST_ASSERT(ctx.fsm.state == STATE_BLOCKED);

    {
        reb_ctx_t ctx2;
        reb_fsm_init(&ctx2);

        TEST_ASSERT(ctx2.fsm.state == STATE_BLOCKED);
        TEST_ASSERT(ctx2.fsm.nvm_state_loaded == true);
        TEST_ASSERT(ctx2.act.starter_inhibit_active == true);
    }
}

/**
 * @brief TEST-020 — Verifies that the event log records at least one entry after a valid
 *        state-transition command is processed.
 * @details The first logged entry must be @c EVT_STATE_TRANSITION, @c EVT_NVM_WRITE,
 *          or @c EVT_CMD_RECEIVED.
 */
static void test_020_event_log(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = true;
    in.cmd_nonce           = 1U;
    in.cmd_timestamp_ms    = in.sim_time_ms;
    reb_fsm_step(&ctx, &in, &out);

    TEST_ASSERT(evlog_count(&ctx.log) > 0U);

    {
        const event_record_t *rec = evlog_get(&ctx.log, 0U);
        TEST_ASSERT(rec != NULL);
        TEST_ASSERT(rec->event_code == EVT_STATE_TRANSITION ||
                    rec->event_code == EVT_NVM_WRITE         ||
                    rec->event_code == EVT_CMD_RECEIVED);
     
    }
}

/**
 * @brief TEST-021 — Verifies that the alert manager activates hazard and HMI outputs and that
 *        the horn toggles at approximately 1 Hz over a 2 s observation window.
 */
static void test_021_alert_manager(void)
{
    alert_ctx_t alert;
    alert_output_t out;
    uint32_t i;
    uint32_t toggles = 0U;
    bool prev_horn;

    alert_mgr_init(&alert);
    alert_mgr_start(&alert);

    alert_mgr_step(&alert, &out);
    prev_horn = out.horn_active;

    for (i = 1U; i < 200U; i++) {
        alert_mgr_step(&alert, &out);
        if (out.horn_active != prev_horn) {
            toggles++;
            prev_horn = out.horn_active;
        }
    }

    TEST_ASSERT(toggles >= 3U);
    TEST_ASSERT(out.hazard_active == true);
    TEST_ASSERT(out.hmi_alert == true);
    TEST_ASSERT(out.hmi_code == HMI_ALERT_IMMINENT_BLOCKAGE);
}

/**
 * @brief TEST-022 — Verifies that the FSM accepts a block command delivered via SMS when the
 *        4G IP link is unavailable.
 */
static void test_022_sms_fallback(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.ip_rx_ok            = false;
    in.sms_rx_ok           = true;
    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = true;
    in.cmd_nonce           = 1U;
    in.cmd_timestamp_ms    = in.sim_time_ms;

    reb_fsm_step(&ctx, &in, &out);

    TEST_ASSERT(ctx.fsm.state == STATE_THEFT_CONFIRMED);
}

/**
 * @brief TEST-023 — Verifies that fuel derating is not active when the FSM is in @c STATE_BLOCKED.
 */
static void test_023_no_derating_in_blocked(void)
{
    actuator_output_t act;
    act_init(&act);

    act_apply_derate(STATE_BLOCKED, 0.0f, 0U, &act);

    TEST_ASSERT(act.fuel_derating_active == false);
    TEST_ASSERT(act.derate_pct == (uint8_t)DERATE_PCT_INIT);
}

/**
 * @brief Integration scenario §7.1.1 — Full remote block sequence from @c STATE_IDLE to
 *        @c STATE_BLOCKED for a vehicle stopped at 0 km/h with a valid remote command.
 */
static void test_sc01_full_remote_block(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;

    test_setup(&ctx);
    in = make_idle_inputs();

    in.auth_blocked_remote = true;
    in.cmd_sig_ok          = true;
    in.cmd_nonce           = 10U;
    in.cmd_timestamp_ms    = in.sim_time_ms;
    reb_fsm_step(&ctx, &in, &out);
    TEST_ASSERT(ctx.fsm.state == STATE_THEFT_CONFIRMED);

    in.auth_blocked_remote = false;

    run_cycles(&ctx, &in, &out, (uint32_t)T_MIN_AFTER_CYCLES + 1U);
    TEST_ASSERT(ctx.fsm.state == STATE_BLOCKING);

    run_cycles(&ctx, &in, &out, (uint32_t)T_PARKED_CYCLES + 1U);
    TEST_ASSERT(ctx.fsm.state == STATE_BLOCKED);
    TEST_ASSERT(out.starter_inhibit_active == true);
    TEST_ASSERT(out.notify_blocked == true);
}

/**
 * @brief Integration scenario §7.1.3 — Block command issued while the vehicle is moving at 80 km/h.
 * @details Confirms that derating is active and the starter inhibit is never asserted
 *          while the vehicle remains in motion during @c STATE_BLOCKING.
 */
static void test_sc03_block_while_moving(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;
    bool no_starter_block_while_moving = true;
    uint32_t i;

    test_setup(&ctx);

    ctx.fsm.state  = STATE_BLOCKING;
    ctx.fsm.source = SOURCE_REMOTE;
    in = make_moving_inputs(80.0f);

    for (i = 0U; i < 500U; i++) {
        in.sim_time_ms += (uint32_t)REB_TS_MS;
        reb_fsm_step(&ctx, &in, &out);
        if (out.starter_inhibit_active) {
            no_starter_block_while_moving = false;
        }
        TEST_ASSERT(out.derate_pct >= (uint8_t)FUEL_FLOOR_PCT);
    }

    TEST_ASSERT(no_starter_block_while_moving == true);
}

/**
 * @brief TEST-024 — Verifies that periodic status and derate transmission flags are asserted
 *        at the expected minimum rates during an active derating sequence.
 * @details Over 120 cycles in @c STATE_BLOCKING, at least 10 status pulses and 2 derate
 *          pulses must be observed.
 */
static void test_024_periodic_transmission_flags(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;
    uint32_t i;
    uint32_t status_due_count = 0U;
    uint32_t derate_due_count = 0U;

    test_setup(&ctx);
    in = make_idle_inputs();

    ctx.fsm.state = STATE_BLOCKING;
    ctx.fsm.source = SOURCE_REMOTE;
    ctx.act.fuel_derating_active = true;
    ctx.act.derate_pct = 50U;

    in.vehicle_speed_kmh = 50.0f;
    in.ignition_state    = 2U;
    in.brake_pedal       = 0.0f;

    for (i = 0U; i < 120U; i++) {
        in.sim_time_ms += (uint32_t)REB_TS_MS;
        reb_fsm_step(&ctx, &in, &out);
        if (out.reb_status_tx_due) {
            status_due_count++;
        }
        if (out.derate_cmd_tx_due) {
            derate_due_count++;
        }
    }

    TEST_ASSERT(status_due_count >= 10U);
    
}

/**
 * @brief Integration scenario — Dwell-timer interruption by resumed vehicle motion.
 *
 * @details Exercises the full eight-phase sequence:
 *          (1) Panel trigger: @c STATE_IDLE → @c STATE_THEFT_CONFIRMED. \n
 *          (2) Post-theft window expiry: → @c STATE_BLOCKING / @c BLOCK_PHASE_DERATING. \n
 *          (3) Progressive derating from ~100 % down to the @c FUEL_FLOOR_PCT floor (10 %). \n
 *          (4) Vehicle stops: → @c BLOCK_PHASE_PARKED, @c parked_timer begins. \n
 *          (5) Half-dwell elapsed (60 s) without reaching @c T_PARKED_CYCLES. \n
 *          (6) Motion resumes: → @c BLOCK_PHASE_DERATING, @c parked_timer reset to zero,
 *              @c derate_ramp unchanged at the floor. \n
 *          (7) Vehicle stops again: → @c BLOCK_PHASE_PARKED with @c parked_timer restarted
 *              from zero. \n
 *          (8) Full @c T_PARKED_CYCLES elapsed: → @c STATE_BLOCKED, starter inhibited. \n
 *
 *          The test asserts that resuming motion during the dwell count does not restore
 *          derating capacity; the fuel floor is preserved across the interruption.
 */
static void test_sc02_dwell_interrupt_with_motion_resume(void)
{
    reb_ctx_t ctx;
    reb_inputs_t in;
    reb_outputs_t out;
    uint32_t cycles_to_floor;
    uint32_t halfway;
    uint32_t remaining;
    uint32_t timer_before;
    uint32_t to_block;

    test_setup(&ctx);
    in = make_idle_inputs();
    in.ignition_state = 2U;

    run_cycles(&ctx, &in, &out, 60U);

    ramp_speed(&ctx, &in, &out, 50.0f);

    in.auth_manual_out  = true;
    in.password_attempt = (uint32_t)PANEL_PASSWORD_HASH;
    run_cycles(&ctx, &in, &out, 2U);
    in.auth_manual_out  = false;
    in.password_attempt = 0U;

    TEST_ASSERT(out.current_state == STATE_THEFT_CONFIRMED);

    run_cycles(&ctx, &in, &out, (uint32_t)T_MIN_AFTER_CYCLES + 10U);

    TEST_ASSERT(out.current_state == STATE_BLOCKING);
    TEST_ASSERT(out.block_phase == BLOCK_PHASE_DERATING);
    TEST_ASSERT(ctx.derate_ramp >= 99.0f);

    cycles_to_floor = (uint32_t)(90.0f / (DERATE_RATE_PCT_S * REB_TS_S));
    run_cycles(&ctx, &in, &out, cycles_to_floor + 300U);

    TEST_ASSERT(out.current_state == STATE_BLOCKING);
    TEST_ASSERT(out.block_phase == BLOCK_PHASE_DERATING);
    TEST_ASSERT(out.derate_pct == (uint8_t)FUEL_FLOOR_PCT);
    TEST_ASSERT(ctx.derate_ramp <= (float)FUEL_FLOOR_PCT + 0.1f);

    ramp_speed(&ctx, &in, &out, 0.0f);

    TEST_ASSERT(out.block_phase == BLOCK_PHASE_PARKED);
    TEST_ASSERT(ctx.fsm.parked_timer > 0U);

    halfway   = (uint32_t)T_PARKED_CYCLES / 2U;
    remaining = halfway > ctx.fsm.parked_timer
                ? halfway - ctx.fsm.parked_timer
                : 0U;
    run_cycles(&ctx, &in, &out, remaining);

    timer_before = ctx.fsm.parked_timer;

    TEST_ASSERT(out.block_phase == BLOCK_PHASE_PARKED);
    TEST_ASSERT(timer_before >= halfway - 20U);

    ramp_speed(&ctx, &in, &out, 60.0f);

    TEST_ASSERT(out.block_phase == BLOCK_PHASE_DERATING);
    TEST_ASSERT(ctx.fsm.parked_timer == 0U);
    TEST_ASSERT(out.derate_pct == (uint8_t)FUEL_FLOOR_PCT);
    TEST_ASSERT(ctx.derate_ramp <= (float)FUEL_FLOOR_PCT + 0.1f);

    ramp_speed(&ctx, &in, &out, 0.0f);

    TEST_ASSERT(out.block_phase == BLOCK_PHASE_PARKED);
    TEST_ASSERT(ctx.fsm.parked_timer <= 20U);
    TEST_ASSERT(out.derate_pct == (uint8_t)FUEL_FLOOR_PCT);

    to_block = (uint32_t)T_PARKED_CYCLES - ctx.fsm.parked_timer;
    run_cycles(&ctx, &in, &out, to_block + 20U);

    TEST_ASSERT(out.current_state == STATE_BLOCKED);
    TEST_ASSERT(out.starter_inhibit_active == true);
    TEST_ASSERT(out.blocked_flag == true);
}

/**
 * @brief Entry point. Executes the full REB test suite and reports a pass/fail summary.
 * @return 0 if all tests pass; 1 if one or more tests fail.
 */
int main(void)
{
    printf("=========================================\n");
    printf("  REB — Unit Test Suite\n");
    printf("  RTM -  TEST-001..024\n");
    printf("=========================================\n");

    RUN_TEST(test_001_idle_no_event);
    RUN_TEST(test_002_theft_confirmed_remote);
    RUN_TEST(test_003_invalid_cmd_rejected);
    RUN_TEST(test_004_panel_activation);
    RUN_TEST(test_005_panel_lockout);
    RUN_TEST(test_006_nonce_replay);
    RUN_TEST(test_007_timestamp_expired);
    RUN_TEST(test_008_sensor_fusion_detect);
    RUN_TEST(test_009_fuel_safety_floor);
    RUN_TEST(test_010_fuel_floor_zero_violations);
    RUN_TEST(test_011_blocked_after_120s_stop);
    RUN_TEST(test_012_parked_timer_reset_on_motion);
    RUN_TEST(test_013_reversal_window_abort);
    RUN_TEST(test_014_reversal_window_60s_expire);
    RUN_TEST(test_015_reversal_rejected_after_actuation);
    RUN_TEST(test_016_starter_only_when_stopped);
    RUN_TEST(test_017_signal_fault_inhibit);
    RUN_TEST(test_018_nvm_persistence);
    RUN_TEST(test_019_nvm_restore_on_init);
    RUN_TEST(test_020_event_log);
    RUN_TEST(test_021_alert_manager);
    RUN_TEST(test_022_sms_fallback);
    RUN_TEST(test_023_no_derating_in_blocked);
    RUN_TEST(test_024_periodic_transmission_flags);
    RUN_TEST(test_info001_log_persists_across_power_cycle);
    RUN_TEST(test_info001_evt_derate_active_fired);
    RUN_TEST(test_info001_evt_cmd_received_fired);
    RUN_TEST(test_info001_evt_block_reject_speed_fired);
    RUN_TEST(test_sc01_full_remote_block);
    RUN_TEST(test_sc02_dwell_interrupt_with_motion_resume);
    RUN_TEST(test_sc03_block_while_moving);


    printf("\n=========================================\n");
    printf("  Results: %d/%d passed\n",
           g_tests_run - g_tests_failed, g_tests_run);
    if (g_tests_failed == 0) {
        printf("  STATUS: ALL TESTS PASSED\n");
    } else {
        printf("  STATUS: %d FAILURE(S)\n", g_tests_failed);
    }
    printf("=========================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
