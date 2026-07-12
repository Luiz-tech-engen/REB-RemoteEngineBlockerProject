/**
 * @file    can_v3_adapter.h
 * @brief   Adapter interface between the CAN stack and the REB FSM.
 *
 * @details Provides the RX path conversion from @c can_rx_message_t to @c reb_inputs_t
 *          and the TX path conversion from @c reb_outputs_t to @c can_tx_message_t.
 *
 *          Typical integration in the main execution loop:
 * @code
 *   can_frame_t       raw_frame;
 *   can_rx_message_t  rx_msg;
 *   reb_inputs_t      inputs;
 *   reb_outputs_t     outputs;
 *   can_tx_message_t  tx_status, tx_derate;
 *   can_frame_t       out_frame;
 *
 *   // RX path
 *   can_rx_process_frame(&raw_frame, &rx_msg);
 *   can_adapter_rx_to_inputs(&rx_msg, &inputs, now_ms);
 *   reb_fsm_step(&ctx, &inputs, &outputs);
 *
 *   // TX path
 *   if (outputs.reb_status_tx_due) {
 *       can_adapter_outputs_to_tx_status(&outputs, &tx_status);
 *       can_tx_build_frame(&tx_status, &out_frame);
 *   }
 *   if (outputs.derate_cmd_tx_due) {
 *       can_adapter_outputs_to_tx_derate(&outputs, &tx_derate);
 *       can_tx_build_frame(&tx_derate, &out_frame);
 *   }
 * @endcode
 */

#ifndef CAN_V3_ADAPTER_H
#define CAN_V3_ADAPTER_H

#include <stdint.h>

#include "can_rx_dev.h"
#include "can_tx.h"

#include "reb/reb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Performs an incremental merge of a decoded CAN message into the FSM input structure.
 *
 * @details Only the fields carried by the received message ID are updated;
 *          all other fields in @p in remain unchanged. Call once per received
 *          frame before invoking @c reb_fsm_step().
 *
 * @param rx      Pointer to the message decoded by @c can_rx_process_frame(). Must not be NULL.
 * @param in      Pointer to the FSM input structure to update in place. Must not be NULL.
 * @param now_ms  Current system timestamp in milliseconds.
 */
void can_adapter_rx_to_inputs(const can_rx_message_t *rx,
                               reb_inputs_t           *in,
                               uint32_t                now_ms);

/**
 * @brief Populates a TX message structure for the REB status frame (CAN_MSG_REB_STATUS / 0x201).
 *
 * @param out       Pointer to the outputs produced by @c reb_fsm_step(). Must not be NULL.
 * @param tx_status Pointer to the TX message structure to populate. Must not be NULL.
 */
void can_adapter_outputs_to_tx_status(const reb_outputs_t *out,
                                       can_tx_message_t    *tx_status);

/**
 * @brief Populates a TX message structure for the REB derate command frame (CAN_MSG_REB_DERATE_CMD / 0x400).
 *
 * @param out       Pointer to the outputs produced by @c reb_fsm_step(). Must not be NULL.
 * @param tx_derate Pointer to the TX message structure to populate. Must not be NULL.
 */
void can_adapter_outputs_to_tx_derate(const reb_outputs_t *out,
                                       can_tx_message_t    *tx_derate);

/**
 * @brief Populates a TX message structure for the REB prevent-start frame (CAN_MSG_REB_PREVENT_START / 0x401).
 *
 * @param out        Pointer to the outputs produced by @c reb_fsm_step(). Must not be NULL.
 * @param tx_prevent Pointer to the TX message structure to populate. Must not be NULL.
 */
void can_adapter_outputs_to_tx_prevent_start(const reb_outputs_t *out,
                                              can_tx_message_t    *tx_prevent);

#ifdef __cplusplus
}
#endif

#endif /* CAN_V3_ADAPTER_H */
