#ifndef REB_CAN_CODEC_H
#define REB_CAN_CODEC_H

#include <stdint.h>

#include "can_frame.h"
#include "can_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes returned by CAN codec functions.
 */
typedef enum
{
    CAN_CODEC_STATUS_OK = 0,
    CAN_CODEC_STATUS_NULL_POINTER,
    CAN_CODEC_STATUS_INVALID_FRAME,
    CAN_CODEC_STATUS_UNKNOWN_MESSAGE,
    CAN_CODEC_STATUS_INVALID_DLC,
    CAN_CODEC_STATUS_DIRECTION_MISMATCH,
    CAN_CODEC_STATUS_VALUE_OUT_OF_RANGE
} can_codec_status_t;

/**
 * @brief REB command type values.
 */
typedef enum
{
    CAN_CMD_TYPE_NOP = 0,
    CAN_CMD_TYPE_BLOCK = 1,
    CAN_CMD_TYPE_UNBLOCK = 2,
    CAN_CMD_TYPE_STATUS_REQUEST = 3
} can_cmd_type_t;

/**
 * @brief REB command mode values.
 */
typedef enum
{
    CAN_CMD_MODE_IDLE = 0,
    CAN_CMD_MODE_GRADUAL_DERATE = 1,
    CAN_CMD_MODE_FULL_BLOCK = 2,
    CAN_CMD_MODE_EMERGENCY = 3
} can_cmd_mode_t;

/**
 * @brief REB status code values.
 */
typedef enum
{
    CAN_STATUS_IDLE = 0,
    CAN_STATUS_THEFT_CONFIRMED = 1,
    CAN_STATUS_BLOCKING = 2,
    CAN_STATUS_BLOCKED = 3
} can_status_code_t;

/**
 * @brief TCU command values.
 */
typedef enum
{
    CAN_TCU_CMD_ACK = 0,
    CAN_TCU_CMD_NACK = 1,
    CAN_TCU_CMD_RETRY = 2,
    CAN_TCU_CMD_FAIL = 3
} can_tcu_cmd_t;

/**
 * @brief Derate mode values.
 */
typedef enum
{
    CAN_DERATE_MODE_OFF = 0,
    CAN_DERATE_MODE_GRADUAL_RAMP = 1,
    CAN_DERATE_MODE_STEP = 2,
    CAN_DERATE_MODE_IMMEDIATE = 3
} can_derate_mode_t;

/**
 * @brief Prevent-start command values.
 */
typedef enum
{
    CAN_PREVENT_START_ALLOW = 0,
    CAN_PREVENT_START_BLOCK = 1
} can_prevent_start_t;

/**
 * @brief GPS request values.
 */
typedef enum
{
    CAN_GPS_REQUEST_NO = 0,
    CAN_GPS_REQUEST_YES = 1
} can_gps_request_t;

/**
 * @brief Intrusion level values.
 */
typedef enum
{
    CAN_INTRUSION_NONE = 0,
    CAN_INTRUSION_LOW = 1,
    CAN_INTRUSION_MEDIUM = 2,
    CAN_INTRUSION_HIGH = 3
} can_intrusion_level_t;

/**
 * @brief Authentication method values.
 */
typedef enum
{
    CAN_AUTH_METHOD_UNKNOWN = 0,
    CAN_AUTH_METHOD_LOCAL_BUTTON = 1,
    CAN_AUTH_METHOD_PIN = 2,
    CAN_AUTH_METHOD_SERVICE_OVERRIDE = 3
} can_auth_method_t;

/**
 * @brief Cancel reason values.
 */
typedef enum
{
    CAN_CANCEL_USER = 0,
    CAN_CANCEL_FALSE_ALARM = 1,
    CAN_CANCEL_MAINTENANCE = 2,
    CAN_CANCEL_SAFETY_ABORT = 3
} can_cancel_reason_t;

/**
 * @brief Logical representation of REB_CMD.
 */
typedef struct
{
    can_cmd_type_t cmd_type;
    can_cmd_mode_t cmd_mode;
    uint16_t cmd_nonce;
    uint32_t cmd_timestamp;
} can_reb_cmd_t;

/**
 * @brief Logical representation of REB_STATUS.
 *
 * vehicle_speed_centi_kmh stores the raw physical value in centi-km/h.
 * Example: 1234 means 12.34 km/h.
 */
typedef struct
{
    can_status_code_t status_code;
    uint8_t blocked_flag;
    uint16_t vehicle_speed_centi_kmh;
    uint16_t error_code;
    uint16_t reserved;
} can_reb_status_t;

/**
 * @brief Logical representation of TCU_TO_REB.
 */
typedef struct
{
    can_tcu_cmd_t tcu_cmd;
    uint8_t fail_reason;
    uint32_t echo_timestamp;
} can_tcu_to_reb_t;

/**
 * @brief Logical representation of REB_DERATE_CMD.
 */
typedef struct
{
    uint8_t derate_pct;
    can_derate_mode_t derate_mode;
    uint8_t safety_flag;
} can_reb_derate_cmd_t;

/**
 * @brief Logical representation of REB_PREVENT_START.
 */
typedef struct
{
    can_prevent_start_t prevent_start;
    uint8_t auth_token_lsb;
} can_reb_prevent_start_t;

/**
 * @brief Logical representation of REB_GPS_REQUEST.
 */
typedef struct
{
    can_gps_request_t gps_request;
    uint8_t state_id_echo;
} can_reb_gps_request_t;

/**
 * @brief VEHICLE_STATE message.
 */
typedef struct
{
    uint16_t vehicle_speed_centi_kmh;
    uint8_t ignition_on;
    uint8_t engine_running;
    uint16_t engine_rpm;
} can_vehicle_state_t;

/**
 * @brief BCM_INTRUSION_STATUS message.
 */
typedef struct
{
    uint8_t door_open;
    uint8_t glass_break;
    uint8_t shock_detected;
    can_intrusion_level_t intrusion_level;
} can_bcm_intrusion_status_t;

/**
 * @brief PANEL_AUTH_CMD message.
 */
typedef struct
{
    uint8_t auth_request;
    can_auth_method_t auth_method;
    uint16_t auth_nonce;
} can_panel_auth_cmd_t;

/**
 * @brief PANEL_CANCEL_CMD message.
 */
typedef struct
{
    uint8_t cancel_request;
    can_cancel_reason_t cancel_reason;
    uint16_t cancel_nonce;
} can_panel_cancel_cmd_t;

/**
 * @brief Decode a REB_CMD frame.
 */
can_codec_status_t can_codec_decode_reb_cmd(
    const can_frame_t *frame,
    can_reb_cmd_t *msg);

/**
 * @brief Decode a TCU_TO_REB frame.
 */
can_codec_status_t can_codec_decode_tcu_to_reb(
    const can_frame_t *frame,
    can_tcu_to_reb_t *msg);

/**
 * @brief Encode a REB_STATUS frame.
 */
can_codec_status_t can_codec_encode_reb_status(
    const can_reb_status_t *msg,
    can_frame_t *frame);

/**
 * @brief Encode a REB_DERATE_CMD frame.
 */
can_codec_status_t can_codec_encode_reb_derate_cmd(
    const can_reb_derate_cmd_t *msg,
    can_frame_t *frame);

/**
 * @brief Encode a REB_PREVENT_START frame.
 */
can_codec_status_t can_codec_encode_reb_prevent_start(
    const can_reb_prevent_start_t *msg,
    can_frame_t *frame);

/**
 * @brief Encode a REB_GPS_REQUEST frame.
 */
can_codec_status_t can_codec_encode_reb_gps_request(
    const can_reb_gps_request_t *msg,
    can_frame_t *frame);

/**
 * @brief Decode a VEHICLE_STATE frame.
 */
can_codec_status_t can_codec_decode_vehicle_state(
    const can_frame_t *frame,
    can_vehicle_state_t *msg);

/**
 * @brief Decode a BCM_INTRUSION_STATUS frame.
 */
can_codec_status_t can_codec_decode_bcm_intrusion_status(
    const can_frame_t *frame,
    can_bcm_intrusion_status_t *msg);

/**
 * @brief Decode a PANEL_AUTH_CMD frame.
 */
can_codec_status_t can_codec_decode_panel_auth_cmd(
    const can_frame_t *frame,
    can_panel_auth_cmd_t *msg);

/**
 * @brief Decode a PANEL_CANCEL_CMD frame.
 */
can_codec_status_t can_codec_decode_panel_cancel_cmd(
    const can_frame_t *frame,
    can_panel_cancel_cmd_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* REB_CAN_CODEC_H */