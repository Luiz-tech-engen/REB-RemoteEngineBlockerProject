/**
 * @file    can_defs.h
 * @brief   CAN physical identifiers, DLCs, signal constants, and frame structures for the REB system.
 *
 * @details Derived from REB_CAN_DATABASE_v5.dbc and aligned with SRS section 3.1.
 *          Covers all RX and TX messages handled by the REB node.
 *          Message 0x402 (REB_GPS_REQUEST) is not defined in this file.
 */

#ifndef CAN_DEFS_H
#define CAN_DEFS_H

#include <stdint.h>

/**
 * @defgroup can_ids CAN Physical Identifiers
 * @{
 */

#define CAN_ID_TCU_STATUS           0x100U /**< TCU to REB, cyclic ~50 ms     */
#define CAN_ID_TCU_AUTH             0x103U /**< TCU to REB, event-driven       */
#define CAN_ID_REB_CMD              0x200U /**< TCU to REB, event-driven       */
#define CAN_ID_TCU_ACK              0x202U /**< TCU to REB, event-driven       */
#define CAN_ID_ECU_POWERTRAIN       0x105U /**< ECU_Powertrain to REB, 100 ms  */
#define CAN_ID_ECU_SENSOR_BCM       0x110U /**< BCM to REB, 20 ms              */
#define CAN_ID_ECU_PANEL            0x120U /**< ECU_Panel to REB, 50 ms        */
#define CAN_ID_ECU_PANEL_AUTH       0x121U /**< ECU_Panel to REB, event-driven */

#define CAN_ID_REB_STATUS           0x201U /**< REB to TCU, event-driven       */
#define CAN_ID_REB_DERATE_CMD       0x400U /**< REB to ECU_Powertrain, 10 ms   */
#define CAN_ID_REB_TO_BCM           0x401U /**< REB to BCM, event-driven       */

/** @} */

/**
 * @defgroup can_dlcs CAN Data Length Codes
 * @{
 */

#define CAN_DLC_TCU_STATUS          1U
#define CAN_DLC_TCU_AUTH            1U
#define CAN_DLC_REB_CMD             8U
#define CAN_DLC_TCU_ACK             1U
#define CAN_DLC_ECU_POWERTRAIN      5U
#define CAN_DLC_ECU_SENSOR_BCM      5U /**< 4 signal bytes + 1 reserved byte */
#define CAN_DLC_ECU_PANEL           1U
#define CAN_DLC_ECU_PANEL_AUTH      4U
#define CAN_DLC_REB_STATUS          1U
#define CAN_DLC_REB_DERATE_CMD      2U
#define CAN_DLC_REB_TO_BCM          1U

/** @} */

/**
 * @defgroup can_signal_values CAN Signal Enumerated Values
 * @details Values are aligned with the VAL_ definitions in the DBC file.
 * @{
 */

/**
 * @brief Signal auth_block_automatic values for message 0x103 TCU_AUTH.
 */
#define TCU_AUTH_AUTO_PENDING       0U
#define TCU_AUTH_AUTO_CONFIRM_YES   1U
#define TCU_AUTH_AUTO_CONFIRM_NO    2U

/**
 * @brief Signal cmd_sig_ok values for message 0x200 REB_CMD.
 */
#define REB_CMD_SIG_INVALID         0U
#define REB_CMD_SIG_OK              1U

/**
 * @brief Signal ignition_state_out values for message 0x105 ECU_POWERTRAIN.
 */
#define IGN_OFF                     0U
#define IGN_ACC                     1U
#define IGN_ON                      2U
#define IGN_START                   3U

/**
 * @brief Signal tcu_ack values for message 0x202 TCU_ACK.
 */
#define TCU_NO_ACK                  0U
#define TCU_ACK                     1U

/**
 * @brief Signal starterOk values for message 0x400 REB_DERATE_CMD.
 */
#define STARTER_BLOCK_START         0U
#define STARTER_ALLOW_START         1U

/**
 * @brief Signal alert_visual and alert_sonic values for message 0x401 REB_to_BCM.
 */
#define ALERT_OFF                   0U
#define ALERT_ON                    1U
#define ALERT_MUTE                  0U
#define ALERT_SONIC_ON              1U

/**
 * @brief Signal values for message 0x201 REB_STATUS.
 */
#define REB_STATUS_NO_THEFT         0U
#define REB_STATUS_THEFT_DETECTED   1U
#define REB_STATUS_NOT_BLOCKED      0U
#define REB_STATUS_VEHICLE_BLOCKED  1U
#define REB_STATUS_NO_GPS_REQ       0U
#define REB_STATUS_SEND_GPS         1U

/** @} */

/**
 * @defgroup can_frame_structs CAN Frame Payload Structures
 * @details Packed representations of each message payload for serialization and deserialization.
 * @{
 */

/**
 * @brief Payload structure for message 0x100 TCU_STATUS (RX).
 */
typedef struct {
    uint8_t ip_rx_ok  : 1;  /**< IP channel reception status, bit 0  */
    uint8_t sms_rx_ok : 1;  /**< SMS channel reception status, bit 1 */
    uint8_t reserved  : 6;
} can_tcu_status_t;

/**
 * @brief Payload structure for message 0x103 TCU_AUTH (RX).
 */
typedef struct {
    uint8_t auth_blocked_remote  : 1; /**< Remote block authorization flag, bit 0   */
    uint8_t auth_block_automatic : 2; /**< Automatic block authorization, bits 1-2  */
    uint8_t remote_unblock_remote: 1; /**< Remote unblock authorization flag, bit 3 */
    uint8_t reserved             : 4;
} can_tcu_auth_t;

/**
 * @brief Payload structure for message 0x200 REB_CMD (RX), 8 bytes.
 */
typedef struct {
    uint16_t cmd_nonce;        /**< 16-bit replay-protection nonce, bytes 0-1          */
    uint32_t cmd_timestamp_ms; /**< Command timestamp in milliseconds, bytes 2-5       */
    uint8_t  cmd_sig_ok  : 1;  /**< Signature validity flag, byte 6 bit 0             */
    uint8_t  reserved    : 7;
    uint8_t  _pad;             /**< Reserved padding byte, byte 7                      */
} can_reb_cmd_t;

/**
 * @brief Payload structure for message 0x105 ECU_POWERTRAIN (RX), 5 bytes.
 */
typedef struct {
    uint8_t  ignition_state  : 2; /**< Ignition state code, byte 0 bits 0-1           */
    uint8_t  reserved        : 6;
    uint16_t engine_rpm_raw;      /**< Engine RPM, scaled by factor 0.25, bytes 1-2   */
    uint16_t vehicle_speed_raw;   /**< Vehicle speed in units of 0.01 km/h, bytes 3-4 */
} can_ecu_powertrain_t;

/**
 * @brief Payload structure for message 0x110 ECU_SENSOR_BCM (RX), 5 bytes.
 */
typedef struct {
    uint16_t accel_peak_raw;       /**< Peak acceleration scaled by 100, bytes 0-1 */
    uint16_t glass_break_flag_raw; /**< Glass break signal scaled by 100, bytes 2-3 */
    uint8_t  _pad;
} can_ecu_bcm_t;

/**
 * @brief Payload structure for message 0x120 ECU_PANEL (RX), 1 byte.
 */
typedef struct {
    uint8_t cancel_request  : 1; /**< Panel cancel request flag, bit 0 */
    uint8_t auth_manual_out : 1; /**< Panel manual authentication flag, bit 1 */
    uint8_t reserved        : 6;
} can_ecu_panel_t;

/**
 * @brief Payload structure for message 0x121 ECU_PANEL_AUTH (RX), 4 bytes.
 */
typedef struct {
    uint32_t password_attempt; /**< 32-bit password hash submitted by the panel */
} can_ecu_panel_auth_t;

/**
 * @brief Payload structure for message 0x202 TCU_ACK (RX), 1 byte.
 */
typedef struct {
    uint8_t tcu_ack  : 1;
    uint8_t reserved : 7;
} can_tcu_ack_t;

/**
 * @brief Payload structure for message 0x201 REB_STATUS (TX), 1 byte.
 */
typedef struct {
    uint8_t notify_theft   : 1; /**< Theft detection notification flag, bit 0          */
    uint8_t notify_blocked : 1; /**< Vehicle blocked notification flag, bit 1          */
    uint8_t gps_send       : 2; /**< GPS transmission request field, bits 2-3          */
    uint8_t reserved       : 4;
} can_reb_status_t;

/**
 * @brief Payload structure for message 0x400 REB_DERATE_CMD (TX), 2 bytes.
 */
typedef struct {
    uint8_t derate_pct; /**< Fuel derate percentage in range [0..100], byte 0 */
    uint8_t starter_ok; /**< Starter permission flag: 0 = blocked, 1 = allowed, byte 1 */
} can_reb_derate_cmd_t;

/**
 * @brief Payload structure for message 0x401 REB_to_BCM (TX), 1 byte.
 */
typedef struct {
    uint8_t alert_visual : 1; /**< Visual alert output: 0 = off, 1 = on, bit 0   */
    uint8_t alert_sonic  : 1; /**< Sonic alert output: 0 = muted, 1 = on, bit 1  */
    uint8_t reserved     : 6;
} can_reb_to_bcm_t;

/** @} */

#endif /* CAN_DEFS_H */
