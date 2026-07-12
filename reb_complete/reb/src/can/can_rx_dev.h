#ifndef REB_CAN_RX_H
#define REB_CAN_RX_H

#include <stdbool.h>

#include "can_codec.h"
#include "can_frame.h"
#include "can_ids.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes returned by CAN RX functions.
 */
typedef enum
{
    CAN_RX_STATUS_OK = 0,
    CAN_RX_STATUS_NULL_POINTER,
    CAN_RX_STATUS_INVALID_FRAME,
    CAN_RX_STATUS_UNKNOWN_ID,
    CAN_RX_STATUS_DIRECTION_MISMATCH,
    CAN_RX_STATUS_INVALID_DLC,
    CAN_RX_STATUS_DECODE_ERROR
} can_rx_status_t;

/**
 * @brief Container for all decoded RX messages relevant to REB.
 *
 * Only one message is valid at a time, indicated by msg_id.
 */
typedef struct
{
    can_msg_id_t msg_id;
    union
    {
        can_reb_cmd_t reb_cmd;
        can_tcu_to_reb_t tcu_to_reb;
        can_vehicle_state_t vehicle_state;
        can_bcm_intrusion_status_t bcm_intrusion;
        can_panel_auth_cmd_t panel_auth;
        can_panel_cancel_cmd_t panel_cancel;
        
    } data;
} can_rx_message_t;

/**
 * @brief Check whether a message is expected as RX by REB.
 *
 * @param msg_id Logical message identifier
 * @return true if the message direction is RX
 */
bool can_rx_is_message_supported(can_msg_id_t msg_id);

/**
 * @brief Decode an incoming CAN frame into a typed RX message.
 *
 * @param frame Pointer to received CAN frame
 * @param out_msg Pointer to decoded output message
 * @return CAN RX status code
 */
can_rx_status_t can_rx_process_frame(
    const can_frame_t *frame,
    can_rx_message_t *out_msg);

#ifdef __cplusplus
}
#endif

#endif /* REB_CAN_RX_H */