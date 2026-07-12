#include "can_monitor.h"

#include <stddef.h>
#include <string.h>

/**
 * @brief Check whether a logical message identifier is valid.
 */
static bool can_monitor_is_valid_msg_id(can_msg_id_t msg_id)
{
    return ((msg_id > CAN_MSG_INVALID) && (msg_id < CAN_MSG_COUNT));
}

/**
 * @brief Compute elapsed time in milliseconds with wraparound-safe arithmetic.
 */
static uint32_t can_monitor_elapsed_ms(uint32_t now_ms, uint32_t last_update_ms)
{
    return (now_ms - last_update_ms);
}

/**
 * @brief Update monitor entry timestamp for a known message.
 */
static can_monitor_status_t can_monitor_update_entry(
    can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms,
    can_direction_t expected_direction)
{
    const can_msg_desc_t *desc;

    if (monitor == NULL)
    {
        return CAN_MONITOR_STATUS_NULL_POINTER;
    }

    if (!can_monitor_is_valid_msg_id(msg_id))
    {
        return CAN_MONITOR_STATUS_INVALID_MESSAGE;
    }

    desc = can_ids_get_desc(msg_id);
    if (desc == NULL)
    {
        return CAN_MONITOR_STATUS_INVALID_MESSAGE;
    }

    if (desc->direction != expected_direction)
    {
        return CAN_MONITOR_STATUS_DIRECTION_MISMATCH;
    }

    monitor->entries[msg_id].seen = true;
    monitor->entries[msg_id].last_update_ms = now_ms;

    return CAN_MONITOR_STATUS_OK;
}

void can_monitor_init(can_monitor_t *monitor)
{
    if (monitor != NULL)
    {
        (void)memset(monitor, 0, sizeof(*monitor));
    }
}

can_monitor_status_t can_monitor_on_rx(
    can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms)
{
    return can_monitor_update_entry(
        monitor,
        msg_id,
        now_ms,
        CAN_DIRECTION_RX);
}

can_monitor_status_t can_monitor_on_tx(
    can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms)
{
    return can_monitor_update_entry(
        monitor,
        msg_id,
        now_ms,
        CAN_DIRECTION_TX);
}

can_monitor_status_t can_monitor_get_message_state(
    const can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms,
    can_monitor_message_state_t *out_state)
{
    const can_msg_desc_t *desc;
    uint32_t elapsed_ms;

    if ((monitor == NULL) || (out_state == NULL))
    {
        return CAN_MONITOR_STATUS_NULL_POINTER;
    }

    if (!can_monitor_is_valid_msg_id(msg_id))
    {
        return CAN_MONITOR_STATUS_INVALID_MESSAGE;
    }

    desc = can_ids_get_desc(msg_id);
    if (desc == NULL)
    {
        return CAN_MONITOR_STATUS_INVALID_MESSAGE;
    }

    if (!monitor->entries[msg_id].seen)
    {
        *out_state = CAN_MONITOR_MESSAGE_STATE_UNSEEN;
        return CAN_MONITOR_STATUS_OK;
    }

    if (desc->timeout_ms == 0U)
    {
        *out_state = CAN_MONITOR_MESSAGE_STATE_FRESH;
        return CAN_MONITOR_STATUS_OK;
    }

    elapsed_ms = can_monitor_elapsed_ms(
        now_ms,
        monitor->entries[msg_id].last_update_ms);

    if (elapsed_ms > desc->timeout_ms)
    {
        *out_state = CAN_MONITOR_MESSAGE_STATE_STALE;
    }
    else
    {
        *out_state = CAN_MONITOR_MESSAGE_STATE_FRESH;
    }

    return CAN_MONITOR_STATUS_OK;
}

bool can_monitor_has_seen(
    const can_monitor_t *monitor,
    can_msg_id_t msg_id)
{
    if ((monitor == NULL) || !can_monitor_is_valid_msg_id(msg_id))
    {
        return false;
    }

    return monitor->entries[msg_id].seen;
}

bool can_monitor_is_message_healthy(
    const can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms)
{
    const can_msg_desc_t *desc;
    can_monitor_message_state_t state;
    can_monitor_status_t status;

    if ((monitor == NULL) || !can_monitor_is_valid_msg_id(msg_id))
    {
        return false;
    }

    desc = can_ids_get_desc(msg_id);
    if (desc == NULL)
    {
        return false;
    }

    if ((desc->direction != CAN_DIRECTION_RX) || (!desc->mandatory))
    {
        return false;
    }

    status = can_monitor_get_message_state(monitor, msg_id, now_ms, &state);
    if (status != CAN_MONITOR_STATUS_OK)
    {
        return false;
    }

    return (state == CAN_MONITOR_MESSAGE_STATE_FRESH);
}

bool can_monitor_are_all_mandatory_rx_messages_healthy(
    const can_monitor_t *monitor,
    uint32_t now_ms)
{
    can_msg_id_t msg_id;

    if (monitor == NULL)
    {
        return false;
    }

    for (msg_id = (can_msg_id_t)(CAN_MSG_INVALID + 1);
         msg_id < CAN_MSG_COUNT;
         msg_id = (can_msg_id_t)(msg_id + 1))
    {
        const can_msg_desc_t *desc;

        desc = can_ids_get_desc(msg_id);
        if (desc == NULL)
        {
            return false;
        }

        if ((desc->direction == CAN_DIRECTION_RX) && (desc->mandatory))
        {
            if (!can_monitor_is_message_healthy(monitor, msg_id, now_ms))
            {
                return false;
            }
        }
    }

    return true;
}