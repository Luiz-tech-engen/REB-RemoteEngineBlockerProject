#include "can_tx.h"

#include <stddef.h>

/**
 * @brief Convert codec status to TX status.
 */
static can_tx_status_t can_tx_map_codec_status(can_codec_status_t status)
{
    switch (status)
    {
        case CAN_CODEC_STATUS_OK:
            return CAN_TX_STATUS_OK;

        case CAN_CODEC_STATUS_NULL_POINTER:
            return CAN_TX_STATUS_NULL_POINTER;

        case CAN_CODEC_STATUS_DIRECTION_MISMATCH:
            return CAN_TX_STATUS_DIRECTION_MISMATCH;

        case CAN_CODEC_STATUS_UNKNOWN_MESSAGE:
            return CAN_TX_STATUS_UNSUPPORTED_MESSAGE;

        case CAN_CODEC_STATUS_INVALID_FRAME:
        case CAN_CODEC_STATUS_INVALID_DLC:
        case CAN_CODEC_STATUS_VALUE_OUT_OF_RANGE:
        default:
            return CAN_TX_STATUS_ENCODE_ERROR;
    }
}

bool can_tx_is_message_supported(can_msg_id_t msg_id)
{
    const can_msg_desc_t *desc;

    desc = can_ids_get_desc(msg_id);
    if (desc == NULL)
    {
        return false;
    }

    return (desc->direction == CAN_DIRECTION_TX);
}

can_tx_status_t can_tx_build_frame(
    const can_tx_message_t *msg,
    can_frame_t *out_frame)
{
    can_codec_status_t codec_status;

    if ((msg == NULL) || (out_frame == NULL))
    {
        return CAN_TX_STATUS_NULL_POINTER;
    }

    switch (msg->msg_id)
    {
        case CAN_MSG_REB_STATUS:
            codec_status = can_codec_encode_reb_status(
                &msg->data.reb_status,
                out_frame);
            return can_tx_map_codec_status(codec_status);

        case CAN_MSG_REB_DERATE_CMD:
            codec_status = can_codec_encode_reb_derate_cmd(
                &msg->data.reb_derate_cmd,
                out_frame);
            return can_tx_map_codec_status(codec_status);

        case CAN_MSG_REB_PREVENT_START:
            codec_status = can_codec_encode_reb_prevent_start(
                &msg->data.reb_prevent_start,
                out_frame);
            return can_tx_map_codec_status(codec_status);

        case CAN_MSG_REB_GPS_REQUEST:
            codec_status = can_codec_encode_reb_gps_request(
                &msg->data.reb_gps_request,
                out_frame);
            return can_tx_map_codec_status(codec_status);

        case CAN_MSG_REB_CMD:
        case CAN_MSG_TCU_TO_REB:
            return CAN_TX_STATUS_DIRECTION_MISMATCH;

        case CAN_MSG_INVALID:
        default:
            return CAN_TX_STATUS_UNSUPPORTED_MESSAGE;
    }
}