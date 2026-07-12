/**
 * @file    reb_params.h
 * @brief   Compile-time constants for the REB system.
 *
 * All literals carry explicit type suffixes
 */

#ifndef REB_PARAMS_H
#define REB_PARAMS_H

/**
 * @defgroup REB_SOLVER Solver / Timing (NFR-DET-001)
 * @{
 */
#define REB_TS_MS               10U      /**< Solver time step in milliseconds (100 Hz).    */
#define REB_TS_S                0.01f    /**< Solver time step in seconds.                  */

/** @} */

/**
 * @defgroup REB_SPEED Speed and Safety Thresholds (FR-010, FR-011, NFR-SAF-001/002)
 * @{
 */
#define V_STOP_KMH              0.5f     /**< Speed threshold for "vehicle stopped" in km/h.           */
#define V_SAFE_KMH              20.0f    /**< Maximum speed at which an abrupt block is permitted (km/h).*/
/** @} */

/**
 * @defgroup REB_TIMERS State Timers (FR-008, FR-010, FR-011, FR-012)
 * @{
 */
#define T_PARKED_S              120U     /**< Dwell time before transitioning to BLOCKED (s).         */

#define T_REVERSAL_S            60U      /**< Pre-block warning window (s).                           */
#define T_MIN_AFTER_CONFIRMED_S 1U       /**< Minimum hold time after THEFT_CONFIRMED for non-AUTO (s).*/

#define T_PARKED_CYCLES         12000U   /**< T_PARKED_S / REB_TS_S in solver cycles.     */

#define T_REVERSAL_CYCLES       6000U    /**< T_REVERSAL_S / REB_TS_S in solver cycles.   */
#define T_MIN_AFTER_CYCLES      100U     /**< T_MIN_AFTER_CONFIRMED_S / REB_TS_S.         */
/** @} */

/**
 * @defgroup REB_DERATE Fuel Derating (FR-009, NFR-SAF-001)
 * @{
 */
#define FUEL_FLOOR_PCT          10U      /**< Minimum fuel derating floor percentage.                   */
#define DERATE_RATE_PCT_S       0.75f    /**< Derating decrement rate in percent per second.            */
#define DERATE_PCT_INIT         100U     /**< Initial derating level at BLOCKING entry (100 = no derate).*/
/** @} */

/**
 * @defgroup REB_PANEL Panel Authentication (FR-004, NFR-SEC-001)
 * @{
 */
#define PANEL_PASSWORD_HASH     0x7C78C98FUL  /**< djb2 hash of the panel PIN (decimal 2088290703). */
#define MAX_AUTH_ATTEMPTS       3U            /**< Wrong attempts permitted before lockout.          */
#define LOCKOUT_DURATION_S      300U          /**< Panel lockout duration in seconds.                */
#define LOCKOUT_CYCLES          30000U        /**< LOCKOUT_DURATION_S / REB_TS_S in solver cycles.  */
/** @} */

/**
 * @defgroup REB_COMMS Network and Communication (FR-005, FR-006)
 * @{
 */
#define MAX_RETRIES             3U       /**< Maximum SMS fallback retransmission attempts.      */
#define ACK_TIMEOUT_CYCLES      500U     /**< ACK_TIMEOUT_S / REB_TS_S in solver cycles.        */
#define STATUS_PERIOD_CYCLES    10U      /**< STATUS_PERIOD_S / REB_TS_S in solver cycles.      */
#define DERATE_CMD_PERIOD_CYCLES 50U     /**< REB_DERATE_CMD transmission period in cycles (0.5 s). */
/** @} */

/**
 * @defgroup REB_SECURITY Anti-Replay Security (NFR-SEC-001)
 * @{
 */
#define NONCE_WINDOW_S          30U      /**< Command nonce acceptance window in seconds.        */
#define NONCE_WINDOW_MS         30000U   /**< NONCE_WINDOW_S in milliseconds.                   */
/** @} */

/**
 * @defgroup REB_SF Sensor Fusion (FR-007)
 * @{
 */
#define SF_W_GLASS              0.6f     /**< Weight assigned to the BCM glass-break sensor.    */
#define SF_W_ACCEL              0.4f     /**< Weight assigned to the accelerometer input.        */
#define SF_THRESH               0.7f     /**< Composite score threshold for theft detection.     */
#define SF_THRESH_HYST_LOW      0.4f     /**< Lower hysteresis threshold for deactivation.       */
#define SF_DEBOUNCE_S           2.0f     /**< Required consecutive detection duration (s).       */
#define SF_DEBOUNCE_CYCLES      200U     /**< SF_DEBOUNCE_S / REB_TS_S in solver cycles.        */
#define ACCEL_MAX               10.0f    /**< Normalisation ceiling for accelerometer input.     */
/** @} */

/**
 * @defgroup REB_STARTER Starter / Powertrain Control (FR-011, NFR-SAF-002)
 * @{
 */
#define RETRANSMIT_BLOCK_TIMEOUT_S      5U   /**< Starter-inhibit retransmission timeout (s).          */
#define RETRANSMIT_BLOCK_TIMEOUT_CYCLES 500U /**< RETRANSMIT_BLOCK_TIMEOUT_S / REB_TS_S in cycles.     */
/** @} */

/**
 * @defgroup REB_LOG Event Log (NFR-INFO-001)
 * @{
 */
#define EVENT_LOG_MAX_ENTRIES   256U     /**< Capacity of the runtime circular event log.        */
#define NVM_LOG_SNAPSHOT_ENTRIES 32U     /**< Number of log entries persisted to NVM.            */
/** @} */





#endif /* REB_PARAMS_H */
