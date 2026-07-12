#include "can_codec.h"

#include <stddef.h>
#include <string.h>

/**
 * @brief Minimum and maximum valid values derived from the DBC.
 */
#define CAN_CODEC_BLOCKED_FLAG_MIN             (0U)
#define CAN_CODEC_BLOCKED_FLAG_MAX             (1U)

#define CAN_CODEC_DERATE_PCT_MIN               (0U)
#define CAN_CODEC_DERATE_PCT_MAX               (100U)

#define CAN_CODEC_SAFETY_FLAG_MIN              (0U)
#define CAN_CODEC_SAFETY_FLAG_MAX              (1U)

#define CAN_CODEC_VEHICLE_SPEED_MIN            (0U)
#define CAN_CODEC_VEHICLE_SPEED_MAX            (65535U)

#define CAN_CODEC_ERROR_CODE_MIN               (0U)
#define CAN_CODEC_ERROR_CODE_MAX               (65535U)

#define CAN_CODEC_RESERVED_MIN                 (0U)
#define CAN_CODEC_RESERVED_MAX                 (65535U)

#define CAN_CODEC_AUTH_TOKEN_LSB_MIN           (0U)
#define CAN_CODEC_AUTH_TOKEN_LSB_MAX           (255U)

#define CAN_CODEC_STATE_ID_ECHO_MIN            (0U)
#define CAN_CODEC_STATE_ID_ECHO_MAX            (255U)

#define CAN_CODEC_FAIL_REASON_MIN              (0U)
#define CAN_CODEC_FAIL_REASON_MAX              (255U)

/**
 * @brief Read an unsigned 16-bit little-endian value from a byte buffer.
 */
static uint16_t can_codec_read_u16_le(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] |
                      ((uint16_t)data[1] << 8U));
}

/**
 * @brief Read an unsigned 32-bit little-endian value from a byte buffer.
 */
static uint32_t can_codec_read_u32_le(const uint8_t *data)
{
    return (uint32_t)((uint32_t)data[0] |
                      ((uint32_t)data[1] << 8U) |
                      ((uint32_t)data[2] << 16U) |
                      ((uint32_t)data[3] << 24U));
}

/**
 * @brief Write an unsigned 16-bit little-endian value into a byte buffer.
 */
static void can_codec_write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)((value >> 8U) & 0xFFU);
}


/**
 * @brief Validate that a received frame matches the expected RX message.
 */
static can_codec_status_t can_codec_validate_rx_frame(
    const can_frame_t *frame,
    can_msg_id_t expected_msg_id)
{
    const can_msg_desc_t *desc;

    if (frame == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    if (!can_frame_is_valid(frame))
    {
        return CAN_CODEC_STATUS_INVALID_FRAME;
    }

    desc = can_ids_get_desc(expected_msg_id);
    if (desc == NULL)
    {
        return CAN_CODEC_STATUS_UNKNOWN_MESSAGE;
    }

    if (desc->direction != CAN_DIRECTION_RX)
    {
        return CAN_CODEC_STATUS_DIRECTION_MISMATCH;
    }

    if ((frame->id != desc->can_id) || (frame->id_type != desc->id_type))
    {
        return CAN_CODEC_STATUS_UNKNOWN_MESSAGE;
    }

    if (frame->dlc != desc->dlc)
    {
        return CAN_CODEC_STATUS_INVALID_DLC;
    }

    return CAN_CODEC_STATUS_OK;
}

/**
 * @brief Initialize the CAN header for a transmit message.
 */
static can_codec_status_t can_codec_prepare_tx_frame(
    can_msg_id_t msg_id,
    can_frame_t *frame)
{
    const can_msg_desc_t *desc;

    if (frame == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    desc = can_ids_get_desc(msg_id);
    if (desc == NULL)
    {
        return CAN_CODEC_STATUS_UNKNOWN_MESSAGE;
    }

    if (desc->direction != CAN_DIRECTION_TX)
    {
        return CAN_CODEC_STATUS_DIRECTION_MISMATCH;
    }

    can_frame_init(frame);

    frame->id = desc->can_id;
    frame->id_type = desc->id_type;
    frame->frame_type = CAN_FRAME_TYPE_DATA;
    frame->dlc = desc->dlc;

    return CAN_CODEC_STATUS_OK;
}

/**
 * @brief Check whether an unsigned value is inside an inclusive range.
 */
static can_codec_status_t can_codec_validate_range_u32(
    uint32_t value,
    uint32_t min_value,
    uint32_t max_value)
{
    if ((value < min_value) || (value > max_value))
    {
        return CAN_CODEC_STATUS_VALUE_OUT_OF_RANGE;
    }

    return CAN_CODEC_STATUS_OK;
}

can_codec_status_t can_codec_decode_reb_cmd(
    const can_frame_t *frame,
    can_reb_cmd_t *msg)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_validate_rx_frame(frame, CAN_MSG_REB_CMD);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    msg->cmd_type = (can_cmd_type_t)frame->data[0];
    msg->cmd_mode = (can_cmd_mode_t)frame->data[1];
    msg->cmd_nonce = can_codec_read_u16_le(&frame->data[2]);
    msg->cmd_timestamp = can_codec_read_u32_le(&frame->data[4]);

    return CAN_CODEC_STATUS_OK;
}

can_codec_status_t can_codec_decode_tcu_to_reb(
    const can_frame_t *frame,
    can_tcu_to_reb_t *msg)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_validate_rx_frame(frame, CAN_MSG_TCU_TO_REB);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    msg->tcu_cmd = (can_tcu_cmd_t)frame->data[0];
    msg->fail_reason = frame->data[1];
    msg->echo_timestamp = can_codec_read_u32_le(&frame->data[2]);

    return CAN_CODEC_STATUS_OK;
}

can_codec_status_t can_codec_encode_reb_status(
    const can_reb_status_t *msg,
    can_frame_t *frame)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_prepare_tx_frame(CAN_MSG_REB_STATUS, frame);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->status_code,
        (uint32_t)CAN_STATUS_IDLE,
        (uint32_t)CAN_STATUS_BLOCKED);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->blocked_flag,
        CAN_CODEC_BLOCKED_FLAG_MIN,
        CAN_CODEC_BLOCKED_FLAG_MAX);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->vehicle_speed_centi_kmh,
        CAN_CODEC_VEHICLE_SPEED_MIN,
        CAN_CODEC_VEHICLE_SPEED_MAX);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->error_code,
        CAN_CODEC_ERROR_CODE_MIN,
        CAN_CODEC_ERROR_CODE_MAX);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->reserved,
        CAN_CODEC_RESERVED_MIN,
        CAN_CODEC_RESERVED_MAX);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    (void)memset(frame->data, 0, sizeof(frame->data));

    frame->data[0] = (uint8_t)msg->status_code;
    frame->data[1] = msg->blocked_flag;
    can_codec_write_u16_le(&frame->data[2], msg->vehicle_speed_centi_kmh);
    can_codec_write_u16_le(&frame->data[4], msg->error_code);
    can_codec_write_u16_le(&frame->data[6], msg->reserved);

    return CAN_CODEC_STATUS_OK;
}

can_codec_status_t can_codec_encode_reb_derate_cmd(
    const can_reb_derate_cmd_t *msg,
    can_frame_t *frame)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_prepare_tx_frame(CAN_MSG_REB_DERATE_CMD, frame);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->derate_pct,
        CAN_CODEC_DERATE_PCT_MIN,
        CAN_CODEC_DERATE_PCT_MAX);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->derate_mode,
        (uint32_t)CAN_DERATE_MODE_OFF,
        (uint32_t)CAN_DERATE_MODE_IMMEDIATE);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->safety_flag,
        CAN_CODEC_SAFETY_FLAG_MIN,
        CAN_CODEC_SAFETY_FLAG_MAX);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    (void)memset(frame->data, 0, sizeof(frame->data));

    frame->data[0] = msg->derate_pct;
    frame->data[1] = (uint8_t)msg->derate_mode;
    frame->data[2] = msg->safety_flag;

    return CAN_CODEC_STATUS_OK;
}

can_codec_status_t can_codec_encode_reb_prevent_start(
    const can_reb_prevent_start_t *msg,
    can_frame_t *frame)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_prepare_tx_frame(CAN_MSG_REB_PREVENT_START, frame);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->prevent_start,
        (uint32_t)CAN_PREVENT_START_ALLOW,
        (uint32_t)CAN_PREVENT_START_BLOCK);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->auth_token_lsb,
        CAN_CODEC_AUTH_TOKEN_LSB_MIN,
        CAN_CODEC_AUTH_TOKEN_LSB_MAX);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    (void)memset(frame->data, 0, sizeof(frame->data));

    frame->data[0] = (uint8_t)msg->prevent_start;
    frame->data[1] = msg->auth_token_lsb;

    return CAN_CODEC_STATUS_OK;
}

can_codec_status_t can_codec_encode_reb_gps_request(
    const can_reb_gps_request_t *msg,
    can_frame_t *frame)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_prepare_tx_frame(CAN_MSG_REB_GPS_REQUEST, frame);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->gps_request,
        (uint32_t)CAN_GPS_REQUEST_NO,
        (uint32_t)CAN_GPS_REQUEST_YES);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    status = can_codec_validate_range_u32(
        (uint32_t)msg->state_id_echo,
        CAN_CODEC_STATE_ID_ECHO_MIN,
        CAN_CODEC_STATE_ID_ECHO_MAX);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    (void)memset(frame->data, 0, sizeof(frame->data));

    frame->data[0] = (uint8_t)msg->gps_request;
    frame->data[1] = msg->state_id_echo;

    return CAN_CODEC_STATUS_OK;
}


can_codec_status_t can_codec_decode_vehicle_state(
    const can_frame_t *frame,
    can_vehicle_state_t *msg)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_validate_rx_frame(frame, CAN_MSG_VEHICLE_STATE);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    msg->vehicle_speed_centi_kmh = can_codec_read_u16_le(&frame->data[0]);
    msg->ignition_on = frame->data[2];
    msg->engine_running = frame->data[3];
    msg->engine_rpm = can_codec_read_u16_le(&frame->data[4]);

    return CAN_CODEC_STATUS_OK;
}

can_codec_status_t can_codec_decode_bcm_intrusion_status(
    const can_frame_t *frame,
    can_bcm_intrusion_status_t *msg)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_validate_rx_frame(frame, CAN_MSG_BCM_INTRUSION_STATUS);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    msg->door_open = frame->data[0];
    msg->glass_break = frame->data[1];
    msg->shock_detected = frame->data[2];
    msg->intrusion_level = (can_intrusion_level_t)frame->data[3];

    return CAN_CODEC_STATUS_OK;
}


can_codec_status_t can_codec_decode_panel_auth_cmd(
    const can_frame_t *frame,
    can_panel_auth_cmd_t *msg)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_validate_rx_frame(frame, CAN_MSG_PANEL_AUTH_CMD);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    msg->auth_request = frame->data[0];
    msg->auth_method = (can_auth_method_t)frame->data[1];
    msg->auth_nonce = can_codec_read_u16_le(&frame->data[2]);

    return CAN_CODEC_STATUS_OK;
}

can_codec_status_t can_codec_decode_panel_cancel_cmd(
    const can_frame_t *frame,
    can_panel_cancel_cmd_t *msg)
{
    can_codec_status_t status;

    if (msg == NULL)
    {
        return CAN_CODEC_STATUS_NULL_POINTER;
    }

    status = can_codec_validate_rx_frame(frame, CAN_MSG_PANEL_CANCEL_CMD);
    if (status != CAN_CODEC_STATUS_OK)
    {
        return status;
    }

    msg->cancel_request = frame->data[0];
    msg->cancel_reason = (can_cancel_reason_t)frame->data[1];
    msg->cancel_nonce = can_codec_read_u16_le(&frame->data[2]);

    return CAN_CODEC_STATUS_OK;
}