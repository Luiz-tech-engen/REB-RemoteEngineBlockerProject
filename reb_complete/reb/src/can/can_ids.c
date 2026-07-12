#include "can_ids.h"

#include <stddef.h>

/**
 * @brief Total number of known DBC-aligned CAN messages.
 */
#define CAN_IDS_MESSAGE_COUNT    (10U)

/**
 * @brief Static lookup table for all known CAN messages.
 *
 * The values in this table are derived from the current CAN
 * architecture document and must remain aligned with the DBC.
 */
static const can_msg_desc_t g_can_msg_table[CAN_IDS_MESSAGE_COUNT] =
{
    {
        .msg_id = CAN_MSG_REB_CMD,
        .can_id = 0x200U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_TCU,
        .direction = CAN_DIRECTION_RX,
        .period_ms = CAN_PERIOD_EVENT_MS,
        .timeout_ms = 1000U,
        .mandatory = true
    },
    {
        .msg_id = CAN_MSG_REB_STATUS,
        .can_id = 0x201U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_REB,
        .direction = CAN_DIRECTION_TX,
        .period_ms = 100U,
        .timeout_ms = 300U,
        .mandatory = false
    },
    {
        .msg_id = CAN_MSG_TCU_TO_REB,
        .can_id = 0x300U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_TCU,
        .direction = CAN_DIRECTION_RX,
        .period_ms = CAN_PERIOD_EVENT_MS,
        .timeout_ms = 500U,
        .mandatory = true
    },
    {
        .msg_id = CAN_MSG_REB_DERATE_CMD,
        .can_id = 0x400U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_REB,
        .direction = CAN_DIRECTION_TX,
        .period_ms = 500U,
        .timeout_ms = 1000U,
        .mandatory = false
    },
    {
        .msg_id = CAN_MSG_REB_PREVENT_START,
        .can_id = 0x401U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_REB,
        .direction = CAN_DIRECTION_TX,
        .period_ms = CAN_PERIOD_EVENT_MS,
        .timeout_ms = 1000U,
        .mandatory = false
    },
    {
        .msg_id = CAN_MSG_REB_GPS_REQUEST,
        .can_id = 0x402U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_REB,
        .direction = CAN_DIRECTION_TX,
        .period_ms = CAN_PERIOD_EVENT_MS,
        .timeout_ms = 1000U,
        .mandatory = false
    },
    {
        .msg_id = CAN_MSG_VEHICLE_STATE,
        .can_id = 0x500U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_VEHICLE,
        .direction = CAN_DIRECTION_RX,
        .period_ms = 100U,
        .timeout_ms = 300U,
        .mandatory = true
    },
    {
        .msg_id = CAN_MSG_BCM_INTRUSION_STATUS,
        .can_id = 0x501U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_BCM,
        .direction = CAN_DIRECTION_RX,
        .period_ms = 100U,
        .timeout_ms = 300U,
        .mandatory = false
    },
    {
        .msg_id = CAN_MSG_PANEL_AUTH_CMD,
        .can_id = 0x502U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_PANEL,
        .direction = CAN_DIRECTION_RX,
        .period_ms = CAN_PERIOD_EVENT_MS,
        .timeout_ms = 500U,
        .mandatory = false
    },
    {
        .msg_id = CAN_MSG_PANEL_CANCEL_CMD,
        .can_id = 0x503U,
        .id_type = CAN_ID_TYPE_STANDARD,
        .dlc = 8U,
        .producer = CAN_NODE_PANEL,
        .direction = CAN_DIRECTION_RX,
        .period_ms = CAN_PERIOD_EVENT_MS,
        .timeout_ms = 500U,
        .mandatory = false
    }
};

/**
 * @brief Get message descriptor from logical message ID.
 */
const can_msg_desc_t *can_ids_get_desc(can_msg_id_t msg_id)
{
    size_t i;

    for (i = 0U; i < CAN_IDS_MESSAGE_COUNT; i++)
    {
        if (g_can_msg_table[i].msg_id == msg_id)
        {
            return &g_can_msg_table[i];
        }
    }

    return NULL;
}

/**
 * @brief Get message descriptor from physical CAN ID and ID type.
 */
const can_msg_desc_t *can_ids_from_can_id(uint32_t can_id, can_id_type_t id_type)
{
    size_t i;

    for (i = 0U; i < CAN_IDS_MESSAGE_COUNT; i++)
    {
        if ((g_can_msg_table[i].can_id == can_id) &&
            (g_can_msg_table[i].id_type == id_type))
        {
            return &g_can_msg_table[i];
        }
    }

    return NULL;
}

/**
 * @brief Check whether a frame uses a known CAN identifier.
 */
bool can_ids_is_frame_known(const can_frame_t *frame)
{
    if (frame == NULL)
    {
        return false;
    }

    return (can_ids_from_can_id(frame->id, frame->id_type) != NULL);
}

/**
 * @brief Check whether a frame DLC matches the expected message DLC.
 */
bool can_ids_is_frame_dlc_valid(const can_frame_t *frame)
{
    const can_msg_desc_t *desc;

    if (frame == NULL)
    {
        return false;
    }

    desc = can_ids_from_can_id(frame->id, frame->id_type);
    if (desc == NULL)
    {
        return false;
    }

    return (frame->dlc == desc->dlc);
}