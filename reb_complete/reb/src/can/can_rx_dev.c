#include "can_rx_dev.h"

#include <stddef.h>
#include <string.h>

/**
 * @brief Convert codec status to RX status.
 */
static can_rx_status_t can_rx_map_codec_status(can_codec_status_t status)
{
    switch (status)
    {
        case CAN_CODEC_STATUS_OK:
            return CAN_RX_STATUS_OK;

        case CAN_CODEC_STATUS_NULL_POINTER:
            return CAN_RX_STATUS_NULL_POINTER;

        case CAN_CODEC_STATUS_INVALID_FRAME:
            return CAN_RX_STATUS_INVALID_FRAME;

        case CAN_CODEC_STATUS_UNKNOWN_MESSAGE:
            return CAN_RX_STATUS_UNKNOWN_ID;

        case CAN_CODEC_STATUS_INVALID_DLC:
            return CAN_RX_STATUS_INVALID_DLC;

        case CAN_CODEC_STATUS_DIRECTION_MISMATCH:
            return CAN_RX_STATUS_DIRECTION_MISMATCH;

        case CAN_CODEC_STATUS_VALUE_OUT_OF_RANGE:
        default:
            return CAN_RX_STATUS_DECODE_ERROR;
    }
}

bool can_rx_is_message_supported(can_msg_id_t msg_id)
{
    const can_msg_desc_t *desc;

    desc = can_ids_get_desc(msg_id);
    if (desc == NULL)
    {
        return false;
    }

    return (desc->direction == CAN_DIRECTION_RX);
}

can_rx_status_t can_rx_process_frame(
    const can_frame_t *frame,
    can_rx_message_t *out_msg)
{
    const can_msg_desc_t *desc;
    can_codec_status_t codec_status;

    if ((frame == NULL) || (out_msg == NULL))
    {
        return CAN_RX_STATUS_NULL_POINTER;
    }

    if (!can_frame_is_valid(frame))
    {
        return CAN_RX_STATUS_INVALID_FRAME;
    }

    desc = can_ids_from_can_id(frame->id, frame->id_type);
    if (desc == NULL)
    {
        return CAN_RX_STATUS_UNKNOWN_ID;
    }

    if (desc->direction != CAN_DIRECTION_RX)
    {
        return CAN_RX_STATUS_DIRECTION_MISMATCH;
    }

    if (frame->dlc != desc->dlc)
    {
        return CAN_RX_STATUS_INVALID_DLC;
    }

    (void)memset(out_msg, 0, sizeof(*out_msg));
    out_msg->msg_id = desc->msg_id;

    switch (desc->msg_id)
    {
        case CAN_MSG_REB_CMD:
            codec_status = can_codec_decode_reb_cmd(
                frame,
                &out_msg->data.reb_cmd);
            return can_rx_map_codec_status(codec_status);

        case CAN_MSG_TCU_TO_REB:
            codec_status = can_codec_decode_tcu_to_reb(
                frame,
                &out_msg->data.tcu_to_reb);
            return can_rx_map_codec_status(codec_status);

        case CAN_MSG_VEHICLE_STATE:
            codec_status = can_codec_decode_vehicle_state(
                frame,
                &out_msg->data.vehicle_state);
            return can_rx_map_codec_status(codec_status);

        case CAN_MSG_BCM_INTRUSION_STATUS:
            codec_status = can_codec_decode_bcm_intrusion_status(
                frame,
                &out_msg->data.bcm_intrusion);
            return can_rx_map_codec_status(codec_status);

        case CAN_MSG_PANEL_AUTH_CMD:
            codec_status = can_codec_decode_panel_auth_cmd(
                frame,
                &out_msg->data.panel_auth);
            return can_rx_map_codec_status(codec_status);

        case CAN_MSG_PANEL_CANCEL_CMD:
            codec_status = can_codec_decode_panel_cancel_cmd(
                frame,
                &out_msg->data.panel_cancel);
            return can_rx_map_codec_status(codec_status);

        case CAN_MSG_REB_STATUS:
        case CAN_MSG_REB_DERATE_CMD:
        case CAN_MSG_REB_PREVENT_START:
        case CAN_MSG_REB_GPS_REQUEST:
        case CAN_MSG_INVALID:
        default:
            return CAN_RX_STATUS_DIRECTION_MISMATCH;
    }
}