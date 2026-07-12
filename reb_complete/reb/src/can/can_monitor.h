#ifndef REB_CAN_MONITOR_H
#define REB_CAN_MONITOR_H

#include <stdbool.h>
#include <stdint.h>

#include "can_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief High-level freshness state of a monitored CAN message.
 */
typedef enum
{
    CAN_MONITOR_MESSAGE_STATE_UNSEEN = 0,
    CAN_MONITOR_MESSAGE_STATE_FRESH,
    CAN_MONITOR_MESSAGE_STATE_STALE
} can_monitor_message_state_t;

/**
 * @brief Status codes returned by CAN monitor functions.
 */
typedef enum
{
    CAN_MONITOR_STATUS_OK = 0,
    CAN_MONITOR_STATUS_NULL_POINTER,
    CAN_MONITOR_STATUS_INVALID_MESSAGE,
    CAN_MONITOR_STATUS_DIRECTION_MISMATCH
} can_monitor_status_t;

/**
 * @brief Runtime monitoring data for a single CAN message.
 */
typedef struct
{
    bool seen;
    uint32_t last_update_ms;
} can_monitor_entry_t;

/**
 * @brief Global CAN monitor context.
 *
 * The table is indexed directly by can_msg_id_t.
 */
typedef struct
{
    can_monitor_entry_t entries[CAN_MSG_COUNT];
} can_monitor_t;

/**
 * @brief Initialize the CAN monitor context.
 *
 * @param monitor Pointer to monitor instance
 */
void can_monitor_init(can_monitor_t *monitor);

/**
 * @brief Record reception of an RX message.
 *
 * @param monitor Pointer to monitor instance
 * @param msg_id Logical message identifier
 * @param now_ms Current system time in milliseconds
 * @return Monitor status code
 */
can_monitor_status_t can_monitor_on_rx(
    can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms);

/**
 * @brief Record transmission of a TX message.
 *
 * @param monitor Pointer to monitor instance
 * @param msg_id Logical message identifier
 * @param now_ms Current system time in milliseconds
 * @return Monitor status code
 */
can_monitor_status_t can_monitor_on_tx(
    can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms);

/**
 * @brief Get current freshness state of a message.
 *
 * @param monitor Pointer to monitor instance
 * @param msg_id Logical message identifier
 * @param now_ms Current system time in milliseconds
 * @param out_state Pointer to output state
 * @return Monitor status code
 */
can_monitor_status_t can_monitor_get_message_state(
    const can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms,
    can_monitor_message_state_t *out_state);

/**
 * @brief Check whether a message has already been seen.
 *
 * @param monitor Pointer to monitor instance
 * @param msg_id Logical message identifier
 * @return true if the message has been seen at least once
 */
bool can_monitor_has_seen(
    const can_monitor_t *monitor,
    can_msg_id_t msg_id);

/**
 * @brief Check whether a mandatory RX message is currently healthy.
 *
 * A mandatory message is considered healthy only if it has been seen
 * and is not stale.
 *
 * @param monitor Pointer to monitor instance
 * @param msg_id Logical message identifier
 * @param now_ms Current system time in milliseconds
 * @return true if healthy, false otherwise
 */
bool can_monitor_is_message_healthy(
    const can_monitor_t *monitor,
    can_msg_id_t msg_id,
    uint32_t now_ms);

/**
 * @brief Check whether all mandatory RX messages are healthy.
 *
 * @param monitor Pointer to monitor instance
 * @param now_ms Current system time in milliseconds
 * @return true if all mandatory RX messages are healthy
 */
bool can_monitor_are_all_mandatory_rx_messages_healthy(
    const can_monitor_t *monitor,
    uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* REB_CAN_MONITOR_H */