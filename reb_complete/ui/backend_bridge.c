#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "reb_main.h"
#include "src/reb_core/nvm.h"
#include "src/reb_core/event_log.h"

#define BRIDGE_LOG_MAX 30U

/**
 * @brief Represents a single event log entry exposed through the bridge API.
 *
 * Captures a state transition event with its timestamp, classification,
 * transition endpoints, originating source, and authentication failure flag.
 */
typedef struct {
    uint32_t ts_ms;
    uint8_t  kind;
    uint8_t  state_from;
    uint8_t  state_to;
    uint8_t  source;
    uint8_t  auth_fail;
} reb_bridge_log_entry_t;

/**
 * @brief Flat input structure used by the bridge layer.
 *
 * All signal types are reduced to primitive C types so that this structure
 * can be populated directly from external test harnesses or serialised
 * data without depending on internal enum or typedef definitions.
 * @c speed_sig_status is cast to @c signal_status_t during copy.
 */
typedef struct {
    float    vehicle_speed_kmh;
    uint16_t engine_rpm;
    uint8_t  ignition_state;
    float    brake_pedal;
    int32_t  speed_sig_status;
    float    accel_peak;
    float    glass_break_flag;
    bool     ip_rx_ok;
    bool     sms_rx_ok;
    bool     auth_blocked_remote;
    uint8_t  auth_block_automatic;
    bool     remote_unblock_remote;
    uint16_t cmd_nonce;
    uint32_t cmd_timestamp_ms;
    bool     cmd_sig_ok;
    bool     tcu_ack;
    bool     cancel_request;
    bool     auth_manual_out;
    uint32_t password_attempt;
    uint32_t sim_time_ms;
} reb_bridge_inputs_t;

/**
 * @brief Flat output structure produced by the bridge layer after each step.
 *
 * Internal enum and typed fields are projected to @c int32_t or @c uint8_t
 * so that consumers do not require knowledge of internal type definitions.
 */
typedef struct {
    uint8_t  derate_pct;
    bool     starter_ok;
    bool     alert_visual;
    bool     alert_sonic;
    int32_t  hmi_alert_code;
    bool     notify_theft;
    bool     notify_blocked;
    bool     gps_send;
    bool     reb_status_tx_due;
    bool     derate_cmd_tx_due;
    int32_t  current_state;
    float    sensor_score;
    bool     pre_block_alert;
    uint32_t reversal_timer_s;
    bool     starter_inhibit_active;
    bool     fuel_derating_active;
    bool     nvm_state_loaded;
    bool     blocked_flag;
    uint8_t  parked;
    bool     derating_active;
    uint8_t  source_trigger_out;
    bool     channel_rx_ok;
    bool     rx_fail;
    int32_t  rx_channel_id;
    bool     panel_password_ok;
    bool     panel_locked_out;
    uint8_t  panel_attempt_count;
    int32_t  block_phase;
    bool     powertrain_valid;
    bool     speed_valid;
    bool     ign_valid;
    bool     brake_valid;
    uint16_t pt_fault_code;
} reb_bridge_outputs_t;

/**
 * @brief Internal state container for a single bridge instance.
 *
 * Wraps the core @c reb_ctx_t together with cached copies of the most
 * recent inputs and outputs. @c has_last_out indicates whether
 * @c last_out contains a valid result from at least one completed step.
 */
typedef struct {
    reb_ctx_t ctx;
    reb_outputs_t last_out;
    reb_inputs_t last_in;
    bool has_last_out;
} reb_bridge_t;

/**
 * @brief Copies bridge input fields into the internal input structure.
 *
 * The destination is zeroed before population to prevent stale field values.
 * @c speed_sig_status is cast from @c int32_t to @c signal_status_t.
 *
 * @param src  Pointer to the bridge input structure; must not be NULL.
 * @param dst  Pointer to the internal input structure to populate; must not be NULL.
 */
static void copy_inputs(const reb_bridge_inputs_t *src, reb_inputs_t *dst)
{
    (void)memset(dst, 0, sizeof(*dst));
    dst->vehicle_speed_kmh = src->vehicle_speed_kmh;
    dst->engine_rpm = src->engine_rpm;
    dst->ignition_state = src->ignition_state;
    dst->brake_pedal = src->brake_pedal;
    dst->speed_sig_status = (signal_status_t)src->speed_sig_status;
    dst->accel_peak = src->accel_peak;
    dst->glass_break_flag = src->glass_break_flag;
    dst->ip_rx_ok = src->ip_rx_ok;
    dst->sms_rx_ok = src->sms_rx_ok;
    dst->auth_blocked_remote = src->auth_blocked_remote;
    dst->auth_block_automatic = src->auth_block_automatic;
    dst->remote_unblock_remote = src->remote_unblock_remote;
    dst->cmd_nonce = src->cmd_nonce;
    dst->cmd_timestamp_ms = src->cmd_timestamp_ms;
    dst->cmd_sig_ok = src->cmd_sig_ok;
    dst->tcu_ack = src->tcu_ack;
    dst->cancel_request = src->cancel_request;
    dst->auth_manual_out = src->auth_manual_out;
    dst->password_attempt = src->password_attempt;
    dst->sim_time_ms = src->sim_time_ms;
}

/**
 * @brief Copies internal output fields into the bridge output structure.
 *
 * The destination is zeroed before population. Enum-typed fields
 * (@c hmi_alert_code, @c current_state, @c block_phase) are cast to
 * @c int32_t for external consumption.
 *
 * @param src  Pointer to the internal output structure; must not be NULL.
 * @param dst  Pointer to the bridge output structure to populate; must not be NULL.
 */
static void copy_outputs(const reb_outputs_t *src, reb_bridge_outputs_t *dst)
{
    (void)memset(dst, 0, sizeof(*dst));
    dst->derate_pct = src->derate_pct;
    dst->starter_ok = src->starter_ok;
    dst->alert_visual = src->alert_visual;
    dst->alert_sonic = src->alert_sonic;
    dst->hmi_alert_code = (int32_t)src->hmi_alert_code;
    dst->notify_theft = src->notify_theft;
    dst->notify_blocked = src->notify_blocked;
    dst->gps_send = src->gps_send;
    dst->reb_status_tx_due = src->reb_status_tx_due;
    dst->derate_cmd_tx_due = src->derate_cmd_tx_due;
    dst->current_state = (int32_t)src->current_state;
    dst->sensor_score = src->sensor_score;
    dst->pre_block_alert = src->pre_block_alert;
    dst->reversal_timer_s = src->reversal_timer_s;
    dst->starter_inhibit_active = src->starter_inhibit_active;
    dst->fuel_derating_active = src->fuel_derating_active;
    dst->nvm_state_loaded = src->nvm_state_loaded;
    dst->blocked_flag = src->blocked_flag;
    dst->parked = src->parked;
    dst->derating_active = src->derating_active;
    dst->source_trigger_out = src->source_trigger_out;
    dst->channel_rx_ok = src->channel_rx_ok;
    dst->rx_fail = src->rx_fail;
    dst->rx_channel_id = src->rx_channel_id;
    dst->panel_password_ok = src->panel_password_ok;
    dst->panel_locked_out = src->panel_locked_out;
    dst->panel_attempt_count = src->panel_attempt_count;
    dst->block_phase = (int32_t)src->block_phase;
    dst->powertrain_valid = src->powertrain_valid;
    dst->speed_valid = src->speed_valid;
    dst->ign_valid = src->ign_valid;
    dst->brake_valid = src->brake_valid;
    dst->pt_fault_code = src->pt_fault_code;
}

/**
 * @brief Allocates and initialises a new bridge instance.
 *
 * Heap-allocates a @c reb_bridge_t, calls @c reb_init on the embedded
 * context, and returns an opaque handle. The caller is responsible for
 * releasing the instance via @c reb_bridge_destroy.
 *
 * @return Opaque pointer to the allocated bridge instance, or NULL on
 *         allocation failure.
 */
void *reb_bridge_create(void)
{
    reb_bridge_t *bridge = (reb_bridge_t *)calloc(1U, sizeof(*bridge));
    if (bridge == NULL) {
        return NULL;
    }
    reb_init(&bridge->ctx);
    bridge->has_last_out = false;
    return (void *)bridge;
}

/**
 * @brief Zeroes and frees a bridge instance.
 *
 * The memory is explicitly zeroed before release to prevent sensitive state
 * from remaining accessible on the heap. Passing NULL is safe and has no effect.
 *
 * @param handle  Opaque pointer previously returned by @c reb_bridge_create.
 */
void reb_bridge_destroy(void *handle)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if (bridge != NULL) {
        (void)memset(bridge, 0, sizeof(*bridge));
        free(bridge);
    }
}

/**
 * @brief Resets the bridge instance to its initial state.
 *
 * Re-initialises the core context and clears cached input/output snapshots.
 * When @p clear_nvm is true, non-volatile state is also invalidated via
 * @c nvm_invalidate before the context is re-initialised.
 *
 * @param handle     Opaque pointer to the bridge instance; NULL is ignored.
 * @param clear_nvm  When true, NVM state is invalidated prior to re-init.
 */
void reb_bridge_reset(void *handle, bool clear_nvm)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if (bridge == NULL) {
        return;
    }
    if (clear_nvm) {
        nvm_invalidate();
    }
    reb_init(&bridge->ctx);
    (void)memset(&bridge->last_out, 0, sizeof(bridge->last_out));
    (void)memset(&bridge->last_in, 0, sizeof(bridge->last_in));
    bridge->has_last_out = false;
}

/**
 * @brief Advances the REB state machine by one simulation step.
 *
 * Converts @p in to the internal input format, calls @c reb_step, caches
 * the resulting outputs, and projects them back into @p out. The function
 * returns without modifying @p out if any pointer argument is NULL.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @param in      Pointer to the inputs for this step; must not be NULL.
 * @param out     Pointer to the output structure to populate; must not be NULL.
 */
void reb_bridge_step(void *handle, const reb_bridge_inputs_t *in, reb_bridge_outputs_t *out)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    reb_inputs_t c_in;
    if ((bridge == NULL) || (in == NULL) || (out == NULL)) {
        return;
    }
    copy_inputs(in, &c_in);
    reb_step(&bridge->ctx, &c_in, &bridge->last_out);
    bridge->last_in = c_in;
    bridge->has_last_out = true;
    copy_outputs(&bridge->last_out, out);
}

/**
 * @brief Copies the most recently computed outputs without advancing the state machine.
 *
 * Useful for reading the current output snapshot between steps or after
 * a reset. If the bridge has never been stepped, the outputs will reflect
 * zeroed state. Returns without modification if any pointer is NULL.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @param out     Pointer to the output structure to populate; must not be NULL.
 */
void reb_bridge_snapshot(void *handle, reb_bridge_outputs_t *out)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if ((bridge == NULL) || (out == NULL)) {
        return;
    }
    copy_outputs(&bridge->last_out, out);
}

/**
 * @brief Returns the current FSM state as a signed integer.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @return        Current state value cast to @c int32_t, or -1 if @p handle is NULL.
 */
int32_t reb_bridge_state(void *handle)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if (bridge == NULL) {
        return -1;
    }
    return (int32_t)reb_get_state(&bridge->ctx);
}

/**
 * @brief Returns the current simulation time in milliseconds.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @return        Simulation time in ms, or 0 if @p handle is NULL.
 */
uint32_t reb_bridge_sim_time_ms(void *handle)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if (bridge == NULL) {
        return 0U;
    }
    return bridge->ctx.fsm.sim_time_ms;
}

/**
 * @brief Returns the last accepted command nonce from the security context.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @return        Last nonce value, or 0 if @p handle is NULL.
 */
uint32_t reb_bridge_last_nonce(void *handle)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if (bridge == NULL) {
        return 0U;
    }
    return bridge->ctx.sec.last_nonce;
}

/**
 * @brief Returns the cumulative count of wrong panel password attempts.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @return        Wrong attempt count, or 0 if @p handle is NULL.
 */
uint8_t reb_bridge_panel_wrong_cnt(void *handle)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if (bridge == NULL) {
        return 0U;
    }
    return bridge->ctx.panel.wrong_cnt;
}

/**
 * @brief Returns whether the panel is currently in lockout state.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @return        True if lockout is active, false otherwise or if @p handle is NULL.
 */
bool reb_bridge_panel_lockout(void *handle)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if (bridge == NULL) {
        return false;
    }
    return bridge->ctx.panel.lockout_active;
}

/**
 * @brief Returns the current fuel derate ramp value.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @return        Derate ramp as a floating-point value, or 0.0 if @p handle is NULL.
 */
float reb_bridge_derate_ramp(void *handle)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    if (bridge == NULL) {
        return 0.0f;
    }
    return bridge->ctx.derate_ramp;
}

/**
 * @brief Returns the number of entries currently stored in the event log.
 *
 * @param handle  Opaque pointer to the bridge instance.
 * @return        Entry count, or 0 if @p handle is NULL.
 */
uint16_t reb_bridge_log_count(void *handle)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    const event_log_ctx_t *log_ctx;
    if (bridge == NULL) {
        return 0U;
    }
    log_ctx = reb_get_log(&bridge->ctx);
    return evlog_count(log_ctx);
}

/**
 * @brief Retrieves the most recent event log entries into a caller-supplied buffer.
 *
 * If the log contains more entries than @p max_entries, only the most recent
 * @p max_entries records are returned. NULL records returned by @c evlog_get
 * are skipped without consuming a slot in @p out.
 *
 * @param handle       Opaque pointer to the bridge instance.
 * @param out          Destination buffer for log entries; must not be NULL.
 * @param max_entries  Maximum number of entries to copy; must be greater than 0.
 * @return             Number of entries written to @p out, or 0 on invalid arguments
 *                     or empty log.
 */
uint16_t reb_bridge_get_logs(void *handle, reb_bridge_log_entry_t *out, uint16_t max_entries)
{
    reb_bridge_t *bridge = (reb_bridge_t *)handle;
    const event_log_ctx_t *log_ctx;
    uint16_t count;
    uint16_t start;
    uint16_t idx;
    uint16_t written;
    const event_record_t *rec;

    if ((bridge == NULL) || (out == NULL) || (max_entries == 0U)) {
        return 0U;
    }

    log_ctx = reb_get_log(&bridge->ctx);
    count = evlog_count(log_ctx);
    if (count == 0U) {
        return 0U;
    }

    start = (count > max_entries) ? (uint16_t)(count - max_entries) : 0U;
    written = 0U;
    for (idx = start; idx < count; idx++) {
        rec = evlog_get(log_ctx, idx);
        if (rec == NULL) {
            continue;
        }
        out[written].ts_ms = rec->timestamp_ms;
        out[written].kind = rec->event_code;
        out[written].state_from = rec->state_from;
        out[written].state_to = rec->state_to;
        out[written].source = rec->source;
        out[written].auth_fail = rec->auth_fail;
        written++;
        if (written >= max_entries) {
            break;
        }
    }
    return written;
}