#ifndef REB_CAN_IDS_H
#define REB_CAN_IDS_H

#include <stdbool.h>
#include <stdint.h>

#include "can_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CAN network nodes defined for the REB system.
 */
typedef enum
{
    CAN_NODE_REB = 0,
    CAN_NODE_TCU,
    CAN_NODE_ECU_FUEL,
    CAN_NODE_ECU_MOTOR,
    CAN_NODE_BCM,
    CAN_NODE_PANEL,
    CAN_NODE_VEHICLE
} can_node_t;

/**
 * @brief Message direction relative to REB.
 */
typedef enum
{
    CAN_DIRECTION_RX = 0, /**< Message is received by REB */
    CAN_DIRECTION_TX      /**< Message is transmitted by REB */
} can_direction_t;

/**
 * @brief Logical identifiers for all DBC-aligned CAN messages.
 */
typedef enum
{
    CAN_MSG_INVALID = 0,

    CAN_MSG_REB_CMD,
    CAN_MSG_REB_STATUS,
    CAN_MSG_TCU_TO_REB,
    CAN_MSG_REB_DERATE_CMD,
    CAN_MSG_REB_PREVENT_START,
    CAN_MSG_REB_GPS_REQUEST,

    CAN_MSG_VEHICLE_STATE,
    CAN_MSG_BCM_INTRUSION_STATUS,
    CAN_MSG_PANEL_AUTH_CMD,
    CAN_MSG_PANEL_CANCEL_CMD,

    CAN_MSG_COUNT

} can_msg_id_t;

/**
 * @brief Period value used for event-driven messages.
 */
#define CAN_PERIOD_EVENT_MS    (0U)

/**
 * @brief Descriptor for a known CAN message.
 *
 * This structure centralizes the metadata required to identify
 * and validate a CAN frame according to the project specification.
 */
typedef struct
{
    can_msg_id_t msg_id;          /**< Logical message identifier */
    uint32_t can_id;              /**< Physical CAN identifier */
    can_id_type_t id_type;        /**< Standard or extended identifier */
    uint8_t dlc;                  /**< Expected data length code */
    can_node_t producer;          /**< Node that transmits the message */
    can_direction_t direction;    /**< RX or TX relative to REB */
    uint32_t period_ms;           /**< Expected period in milliseconds */
    uint32_t timeout_ms;          /**< Timeout in milliseconds */
    bool mandatory;               /**< Project policy flag for required monitoring */
} can_msg_desc_t;

/**
 * @brief Get descriptor from logical message ID.
 *
 * @param msg_id Logical message identifier
 * @return Pointer to descriptor, or NULL if not found
 */
const can_msg_desc_t *can_ids_get_desc(can_msg_id_t msg_id);

/**
 * @brief Get descriptor from physical CAN identifier.
 *
 * @param can_id CAN identifier
 * @param id_type Identifier type
 * @return Pointer to descriptor, or NULL if unknown
 */
const can_msg_desc_t *can_ids_from_can_id(uint32_t can_id, can_id_type_t id_type);

/**
 * @brief Check whether a CAN frame ID is known by the system.
 *
 * @param frame Pointer to CAN frame
 * @return true if the frame ID is known, false otherwise
 */
bool can_ids_is_frame_known(const can_frame_t *frame);

/**
 * @brief Check whether a CAN frame DLC matches the expected specification.
 *
 * @param frame Pointer to CAN frame
 * @return true if the DLC is valid, false otherwise
 */
bool can_ids_is_frame_dlc_valid(const can_frame_t *frame);

#ifdef __cplusplus
}
#endif

#endif /* REB_CAN_IDS_H */