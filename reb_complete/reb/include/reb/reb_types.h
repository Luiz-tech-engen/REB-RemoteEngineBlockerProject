/**
 * @file    reb_types.h
 * @brief   Central type definitions, enumerations, and structures for the REB system.
 *
 * All integer types are explicitly sized
 */

#ifndef REB_TYPES_H
#define REB_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "reb_params.h"

/**
 * @brief FSM state enumeration (FR-001, FR-002, FR-003).
 */
typedef enum {
    STATE_IDLE            = 0U, /**< System monitoring; no active theft event.              */
    STATE_THEFT_CONFIRMED = 1U, /**< Theft confirmed; awaiting block action or reversal.    */
    STATE_BLOCKING        = 2U, /**< Progressive fuel derating in progress.                 */
    STATE_BLOCKED         = 3U  /**< Definitive block; starter inhibit active.              */
} reb_state_t;

/**
 * @brief Remote command type (FR-005), aligned with CAN DBC 0x200 VAL_.
 */
typedef enum {
    CMD_NOP            = 0U, /**< No operation.                    */
    CMD_BLOCK          = 1U, /**< Remote block command.            */
    CMD_UNBLOCK        = 2U, /**< Remote unblock command.          */
    CMD_STATUS_REQUEST = 3U  /**< Status request from TCU.         */
} cmd_type_t;

/**
 * @brief Source that triggered or is requesting a block/unblock action (FR-004..007).
 */
typedef enum {
    SOURCE_PANEL  = 0U, /**< Local physical panel (FR-004).              */
    SOURCE_REMOTE = 1U, /**< Remote 4G/5G or SMS channel (FR-005/006).   */
    SOURCE_AUTO   = 2U  /**< Automatic sensor-fusion trigger (FR-007).   */
} activation_source_t;

/**
 * @brief Authentication failure code (NFR-SEC-001).
 *
 * AUTH_OK is retained as the zero value so a zero-initialised context is safe.
 */
typedef enum {
    AUTH_OK             = 0U, /**< Authentication accepted.                          */
    AUTH_SIG_INVALID    = 1U, /**< Command signature invalid (cmd_sig_ok == 0).      */
    AUTH_NONCE_REPLAY   = 2U, /**< Nonce already seen; replay detected.              */
    AUTH_TS_EXPIRED     = 3U  /**< Timestamp outside the 30 s acceptance window.     */
} auth_fail_t;

/**
 * @brief CAN signal validity status (NFR-SAF-002).
 */
typedef enum {
    SIG_VALID   = 0U, /**< Signal present and within freshness limits.   */
    SIG_MISSING = 1U, /**< Signal absent; CAN frame not received.        */
    SIG_TIMEOUT = 2U  /**< Signal received but timestamp has expired.    */
} signal_status_t;

/**
 * @brief Reversal window mode selector (FR-008 and FR-012).
 */
typedef enum {
    RW_MODE_60 = 0U, /**< 60 s cancellation window for SOURCE_AUTO (FR-008).  */
} rw_mode_t;

/**
 * @brief Outcome of a reversal window instance.
 */
typedef enum {
    RW_RUNNING = 0U, /**< Timer still counting; no decision yet.          */
    RW_ABORT   = 1U, /**< Operator cancelled with valid credentials.      */
    RW_EXPIRE  = 2U  /**< Timer expired without cancellation.             */
} rw_result_t;

/**
 * @brief Internal sub-phase of the BLOCKING state, aligned to the Simulink model.
 */
typedef enum {
    BLOCK_PHASE_PREALERT = 0U, /**< Pre-alert: block actuation not yet issued.           */
    BLOCK_PHASE_DERATING  = 1U, /**< Progressive fuel derating active.                   */
    BLOCK_PHASE_PARKED    = 2U  /**< Vehicle stopped; awaiting starter inhibit command.  */
} reb_block_phase_t;

/**
 * @brief Output of the sensor fusion module (FR-007).
 */
typedef struct {
    float    theft_score;       /**< Weighted composite score in [0.0, 1.0].            */
    bool     theft_detected;    /**< True when theft_score >= SF_THRESH.                */
    uint16_t debounce_cnt;      /**< Consecutive cycles above threshold (debounce).     */
} sf_output_t;

/**
 * @brief Actuator command outputs (FR-009, FR-010, FR-011).
 */
typedef struct {
    uint8_t  derate_pct;            /**< Fuel pump derating level in [10, 100] percent. */
    bool     starter_ok;            /**< True = starter permitted; false = inhibited.   */
    bool     fuel_derating_active;  /**< Derating is currently applied.                 */
    bool     starter_inhibit_active;/**< Starter inhibit command is asserted.           */
} actuator_output_t;

/**
 * @brief HMI alert code enumeration (FR-013).
 */
typedef enum {
    HMI_ALERT_NONE             = 0U, /**< No active HMI alert.             */
    HMI_ALERT_IMMINENT_BLOCKAGE = 1U /**< Imminent blockage warning.       */
} hmi_alert_code_t;

/**
 * @brief Alert output signals directed to the BCM / HMI layer (FR-013).
 */
typedef struct {
    bool             horn_active;    /**< Intermittent 1 Hz horn active.                */
    bool             hazard_active;  /**< Hazard lights active.                          */
    bool             hmi_alert;      /**< Critical alert present on HMI panel.           */
    hmi_alert_code_t hmi_code;       /**< Textual alert code displayed on HMI.           */
} alert_output_t;

/**
 * @brief CAN status signals mapped to frame 0x201 REB_STATUS (FR-005).
 *
 * Field names correspond directly to DBC signal names.
 */
typedef struct {
    bool notify_theft;   /**< SG_ notify_theft   : bit 0. */
    bool notify_blocked; /**< SG_ notify_blocked : bit 1. */
    bool gps_send;       /**< SG_ gps_send       : bit 2. */
} reb_status_can_t;

/**
 * @brief Single event log record (NFR-INFO-001).
 */
typedef struct {
    uint32_t timestamp_ms;  /**< Simulation timestamp in milliseconds.           */
    uint8_t  event_code;    /**< Event identifier (see event_log.h constants).   */
    uint8_t  state_from;    /**< FSM state at event entry.                       */
    uint8_t  state_to;      /**< FSM state after event processing.               */
    uint8_t  source;        /**< Activation source (activation_source_t).        */
    uint8_t  auth_fail;     /**< Authentication failure code (auth_fail_t).      */
    uint8_t  _pad[3];       /**< Explicit padding struct alignment.  */
} event_record_t;

/**
 * @brief NVM-persisted data block (NFR-REL-001).
 *
 * Restored on power-up to resume the FSM state and anti-replay context.
 * Integrity is verified via the trailing crc32 field.
 */
typedef struct {
    reb_state_t        last_state;          /**< FSM state at last power-down.             */
    reb_block_phase_t  last_block_phase;    /**< Internal block phase at last power-down.  */
    activation_source_t last_source;        /**< Activation source of the last block.      */
    uint32_t           last_nonce;          /**< Last accepted nonce for anti-replay.      */
    uint32_t           parked_timer;        /**< Persisted parked dwell timer value.       */
    uint32_t           min_after_timer;     /**< Persisted post-theft minimum timer.       */
    float              derate_ramp;         /**< Persisted derating ramp level.            */
    uint8_t            panel_wrong_cnt;     /**< Accumulated panel wrong-attempt counter.  */
    bool               lockout_active;      /**< Panel lockout state.                      */
    uint32_t           lockout_remaining_s; /**< Remaining lockout duration in seconds.    */
    event_record_t     log_entries[NVM_LOG_SNAPSHOT_ENTRIES]; /**< Forensic event log snapshot. */
    uint16_t           log_head;            /**< Number of entries stored in snapshot.     */
    uint16_t           log_count;           /**< Cumulative total event count.             */
    uint32_t           crc32;               /**< CRC32 integrity check over the struct.    */
} nvm_data_t;

/**
 * @brief Panel authentication runtime context (FR-004).
 */
typedef struct {
    uint8_t  wrong_cnt;       /**< Consecutive wrong-attempt counter.                      */
    bool     lockout_active;  /**< Panel is in lockout state.                              */
    uint32_t lockout_timer;   /**< Remaining lockout cycles.                               */
    bool     prev_auth_pulse; /**< Previous-cycle authentication pulse (edge detection).   */
    bool     prev_cancel_req; /**< Previous-cycle cancel request (edge detection).         */
    bool     auth_ok;         /**< Authentication succeeded this cycle.                    */
} panel_auth_ctx_t;

/**
 * @brief Sensor fusion runtime context (FR-007).
 */
typedef struct {
    float    last_score;       /**< Composite theft score from the previous cycle.         */
    uint16_t debounce_cnt;     /**< Consecutive cycles with score above SF_THRESH.         */
    bool     active;           /**< Theft detection currently active.                      */
    uint8_t  _pad;
} sf_ctx_t;

/**
 * @brief Security manager runtime context (NFR-SEC-001).
 */
typedef struct {
    uint32_t last_nonce; /**< Last accepted nonce; used for replay detection. */
} sec_ctx_t;

/**
 * @brief Reversal window runtime context (FR-008, FR-012).
 */
typedef struct {
    rw_mode_t mode;                    /**< Active window mode (60 s).             */
    uint32_t  timer_cycles;            /**< Elapsed cycle count since window activation.   */
    uint32_t  limit_cycles;            /**< Cycle count at which the window expires.       */
    bool      active;                  /**< Window is currently running.                   */
    bool      pre_block_alert_active;  /**< Pre-block alert is asserted.                   */
    bool      blocking_actuation_issued; /**< Block actuation command has been issued.     */
    uint8_t   _pad;
} rw_ctx_t;

/**
 * @brief Primary FSM runtime context (FR-001..003).
 *
 * Persistent across reb_step() calls; must be initialised via reb_fsm_init().
 */
typedef struct {
    reb_state_t        state;              /**< Current FSM state.                          */
    reb_state_t        prev_state;         /**< FSM state in the previous cycle.            */
    activation_source_t source;            /**< Source that activated the current block.    */
    uint32_t           parked_timer;       /**< Parked dwell counter in cycles (FR-010/011).*/
    uint32_t           min_after_timer;    /**< Post-THEFT minimum hold timer (FR-012).     */
    bool               unblock_requested;  /**< Unblock request pending.                    */
    signal_status_t    speed_signal_status;/**< Validity of the vehicle speed signal.       */
    bool               nvm_state_loaded;   /**< FSM state was restored from NVM.            */
    reb_block_phase_t  block_phase;        /**< Current internal block sub-phase.           */
    uint32_t           sim_time_ms;        /**< Simulation wall-clock time in milliseconds. */
    uint8_t            _pad[2];
} fsm_ctx_t;

/**
 * @brief Input structure populated by the CAN layer each cycle.
 *
 * Field groupings correspond to physical CAN frame IDs as noted.
 */
typedef struct {
    float           vehicle_speed_kmh;  /**< Vehicle speed in km/h (frame 0x105).              */
    uint16_t        engine_rpm;         /**< Engine speed in RPM (frame 0x105).                */
    uint8_t         ignition_state;     /**< Ignition position: 0=OFF,1=ACC,2=ON,3=START.      */
    float           brake_pedal;        /**< Normalised brake pedal position [0.0, 1.0].        */
    signal_status_t speed_sig_status;   /**< Speed signal validity (NFR-SAF-002).               */

    float           accel_peak;         /**< Peak acceleration magnitude from BCM (0x110).     */
    float           glass_break_flag;   /**< Glass-break sensor output from BCM (0x110).       */

    bool            ip_rx_ok;           /**< 4G/IP channel available (0x100).                  */
    bool            sms_rx_ok;          /**< SMS fallback channel available (0x100).            */

    bool            auth_blocked_remote;     /**< Remote block command from TCU (0x103).       */
    uint8_t         auth_block_automatic;    /**< Auto-block decision: 0=PENDING,1=YES,2=NO.   */
    bool            remote_unblock_remote;   /**< Remote unblock command from TCU (0x103).     */

    uint16_t        cmd_nonce;          /**< Command nonce for anti-replay (0x200).            */
    uint32_t        cmd_timestamp_ms;   /**< Command timestamp in ms for anti-replay (0x200).  */
    bool            cmd_sig_ok;         /**< Command HMAC/signature valid (0x200).             */

    bool            tcu_ack;            /**< TCU acknowledgement received (0x202).             */

    bool            cancel_request;     /**< Cancellation request from physical panel (0x120). */
    bool            auth_manual_out;    /**< Manual activation from physical panel (0x120).    */

    uint32_t        password_attempt;   /**< djb2 hash of the panel password attempt (0x121).  */

    uint32_t        sim_time_ms;        /**< Simulation wall-clock time in milliseconds.        */
} reb_inputs_t;

/**
 * @brief Output structure filled by the FSM each cycle and consumed by the CAN layer.
 *
 * Field groupings correspond to physical CAN frame IDs as noted.
 */
typedef struct {
    uint8_t  derate_pct;            /**< Fuel pump derating command in [0, 100] percent (0x400). */
    bool     starter_ok;            /**< True = starter permitted; false = inhibited (0x400).    */

    bool     alert_visual;          /**< Hazard lights command (0x401).                          */
    bool     alert_sonic;           /**< Horn command (0x401).                                   */
    hmi_alert_code_t hmi_alert_code; /**< HMI alert code (0x401).                               */

    bool     notify_theft;          /**< Theft notification flag for REB_STATUS (0x201).         */
    bool     notify_blocked;        /**< Blocked notification flag for REB_STATUS (0x201).       */
    bool     gps_send;              /**< GPS request to TCU (0x201).                             */
    bool     reb_status_tx_due;     /**< REB_STATUS transmission window open (100 ms period).    */
    bool     derate_cmd_tx_due;     /**< REB_DERATE_CMD transmission window open (500 ms period).*/

    reb_state_t current_state;      /**< Current FSM state for HMI diagnostics.                  */
    float       sensor_score;       /**< Sensor fusion composite score [0.0, 1.0].               */
    bool        pre_block_alert;    /**< Pre-block alert active.                                  */
    uint32_t    reversal_timer_s;   /**< Remaining reversal window time in seconds.               */
    bool        starter_inhibit_active; /**< Starter inhibit asserted.                           */
    bool        fuel_derating_active;   /**< Fuel derating active.                               */
    bool        nvm_state_loaded;   /**< FSM state was restored from NVM on this power-up.       */

    bool        blocked_flag;       /**< System is in definitive BLOCKED state.                  */
    uint8_t     parked;             /**< Vehicle-parked / parked-sequence indicator.             */
    bool        derating_active;    /**< Derating active (upstream diagnostic mirror).           */
    uint8_t     source_trigger_out; /**< Activation source that triggered the block.             */

    bool        channel_rx_ok;      /**< CAN receive watchdog healthy.                           */
    bool        rx_fail;            /**< CAN receive watchdog failure.                           */
    int32_t     rx_channel_id;      /**< Active RX channel: 0=IP, 1=SMS, 2=none.                */
    bool        panel_password_ok;  /**< Panel password authenticated this cycle.                */
    bool        panel_locked_out;   /**< Panel is in lockout state.                              */
    uint8_t     panel_attempt_count;/**< Accumulated wrong panel attempt count.                  */
    reb_block_phase_t block_phase;  /**< Internal block sub-phase.                               */

    bool        powertrain_valid;    /**< Composite powertrain signal validity.                  */
    bool        speed_valid;         /**< Vehicle speed signal validity.                         */
    bool        ign_valid;           /**< Ignition signal validity.                              */
    bool        brake_valid;         /**< Brake pedal signal validity.                           */
    uint16_t    pt_fault_code;       /**< Powertrain diagnostic fault bitmask.                   */
} reb_outputs_t;

#endif /* REB_TYPES_H */
