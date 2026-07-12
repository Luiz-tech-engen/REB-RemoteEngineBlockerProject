/**
 * @file    can_v3_adapter.c
 * @brief   Adapter layer between the CAN stack and the REB FSM.
 *
 * @details Translates decoded CAN frames into @c reb_inputs_t fields (RX path)
 *          and maps @c reb_outputs_t fields into @c can_tx_message_t structures (TX path).
 *
 *          Enum alignment between layers:
 *          | reb_state_t (v3)      | can_status_code_t      | Value |
 *          |-----------------------|------------------------|-------|
 *          | STATE_IDLE            | CAN_STATUS_IDLE            | 0 |
 *          | STATE_THEFT_CONFIRMED | CAN_STATUS_THEFT_CONFIRMED | 1 |
 *          | STATE_BLOCKING        | CAN_STATUS_BLOCKING        | 2 |
 *          | STATE_BLOCKED         | CAN_STATUS_BLOCKED         | 3 |
 *
 *          | can_cmd_type_t          | reb_inputs_t field            |
 *          |-------------------------|-------------------------------|
 *          | CAN_CMD_TYPE_NOP      = 0 | no active command           |
 *          | CAN_CMD_TYPE_BLOCK    = 1 | auth_blocked_remote = true  |
 *          | CAN_CMD_TYPE_UNBLOCK  = 2 | remote_unblock_remote = true|
 *          | CAN_CMD_TYPE_STATUS_REQUEST = 3 | no FSM effect         |
 */

#include "can_v3_adapter.h"
#include <string.h>

/**
 * @brief Converts a vehicle speed value from centi-km/h to km/h.
 * @param centi_kmh  Speed in centi-km/h (e.g. 1234 represents 12.34 km/h).
 * @return Speed in km/h as a single-precision float.
 */
static float priv_centi_kmh_to_kmh(uint16_t centi_kmh)
{
    return (float)centi_kmh / 100.0f;
}

/**
 * @brief Maps the ignition and engine state fields of a @c can_vehicle_state_t
 *        to the three-level ignition code expected by the REB FSM.
 *
 * @details Mapping rules (evaluated in priority order):
 *          - engine_running == 1  →  2 (IGN_ON)
 *          - ignition_on    == 1  →  1 (IGN_ACC)
 *          - both zero            →  0 (IGN_OFF)
 *
 *          The @c ignition_on field in @c can_vehicle_state_t is a single bit;
 *          no intermediate states are available from this source.
 *
 * @param vs  Pointer to the vehicle state CAN payload. Must not be NULL.
 * @return    Ignition code: 0 = OFF, 1 = ACC, 2 = ON.
 */
static uint8_t priv_map_ignition(const can_vehicle_state_t *vs)
{
    if (vs->engine_running) {
        return 2U;
    }
    if (vs->ignition_on) {
        return 1U;
    }
    return 0U;
}

/**
 * @brief Performs an incremental merge of a decoded CAN message into the FSM input structure.
 *
 * @details Only the fields carried by the received message ID are updated;
 *          all other fields in @p in remain unchanged. Must be called once per
 *          received frame before invoking @c reb_fsm_step().
 *
 *          @c cmd_sig_ok is inferred from @c cmd_type: any value other than
 *          @c CAN_CMD_TYPE_NOP is treated as a signature-verified command,
 *          because the TCU validates the signature before transmission.
 *
 *          When @c CAN_MSG_VEHICLE_STATE is received, @c speed_sig_status is set
 *          to @c SIG_VALID. A supervisory layer must set it to @c SIG_TIMEOUT
 *          externally if no @c VEHICLE_STATE frame arrives within the expected period.
 *
 *          The panel authentication nonce (@c auth_nonce) is written to @c cmd_nonce
 *          so that the security manager can enforce replay protection on the panel path.
 *
 * @param rx      Pointer to the decoded CAN message. Must not be NULL.
 * @param in      Pointer to the FSM input structure to update in place. Must not be NULL.
 * @param now_ms  Current system timestamp in milliseconds.
 */
void can_adapter_rx_to_inputs(const can_rx_message_t *rx,
                               reb_inputs_t           *in,
                               uint32_t                now_ms)
{
    if ((rx == NULL) || (in == NULL)) {
        return;
    }

    in->sim_time_ms = now_ms;

    switch (rx->msg_id)
    {
        case CAN_MSG_REB_CMD:
        {
            const can_reb_cmd_t *cmd = &rx->data.reb_cmd;

            in->cmd_nonce        = cmd->cmd_nonce;
            in->cmd_timestamp_ms = cmd->cmd_timestamp;

            in->cmd_sig_ok = (cmd->cmd_type != CAN_CMD_TYPE_NOP) ? true : false;

            in->auth_blocked_remote  = (cmd->cmd_type == CAN_CMD_TYPE_BLOCK);
            in->remote_unblock_remote = (cmd->cmd_type == CAN_CMD_TYPE_UNBLOCK);
            break;
        }

        case CAN_MSG_TCU_TO_REB:
        {
            in->tcu_ack = (rx->data.tcu_to_reb.tcu_cmd == CAN_TCU_CMD_ACK);
            break;
        }

        case CAN_MSG_VEHICLE_STATE:
        {
            const can_vehicle_state_t *vs = &rx->data.vehicle_state;

            in->vehicle_speed_kmh = priv_centi_kmh_to_kmh(vs->vehicle_speed_centi_kmh);
            in->engine_rpm        = vs->engine_rpm;
            in->ignition_state    = priv_map_ignition(vs);

            in->speed_sig_status = SIG_VALID;
            break;
        }

        case CAN_MSG_BCM_INTRUSION_STATUS:
        {
            const can_bcm_intrusion_status_t *bcm = &rx->data.bcm_intrusion;

            /**
             * Binary intrusion flags are mapped to a normalised float range [0.0, 1.0].
             * For finer granularity, replace with: (float)bcm->intrusion_level / 3.0f
             */
            in->accel_peak       = bcm->shock_detected ? 1.0f : 0.0f;
            in->glass_break_flag = bcm->glass_break    ? 1.0f : 0.0f;
            break;
        }

        case CAN_MSG_PANEL_AUTH_CMD:
        {
            const can_panel_auth_cmd_t *pa = &rx->data.panel_auth;

            in->auth_manual_out = pa->auth_request;
            in->cmd_nonce = pa->auth_nonce;
            break;
        }

        case CAN_MSG_PANEL_CANCEL_CMD:
        {
            in->cancel_request = rx->data.panel_cancel.cancel_request;
            break;
        }

        default:
            break;
    }
}

/**
 * @brief Populates a TX message structure for the REB status frame (0x201).
 *
 * @details The cast from @c reb_state_t to @c can_status_code_t is safe because
 *          both enumerations share identical numeric values for all defined states (0–3).
 *          The @c vehicle_speed_centi_kmh and @c error_code fields are zeroed; they
 *          are not available in this TX path.
 *
 * @param out       Pointer to FSM outputs. Must not be NULL.
 * @param tx_status Pointer to the TX message to populate. Must not be NULL.
 */
void can_adapter_outputs_to_tx_status(const reb_outputs_t *out,
                                       can_tx_message_t    *tx_status)
{
    if ((out == NULL) || (tx_status == NULL)) {
        return;
    }

    tx_status->msg_id = CAN_MSG_REB_STATUS;

    tx_status->data.reb_status.status_code =
        (can_status_code_t)out->current_state;

    tx_status->data.reb_status.blocked_flag          =
        out->notify_blocked ? 1U : 0U;

    tx_status->data.reb_status.vehicle_speed_centi_kmh = 0U;
    tx_status->data.reb_status.error_code               = 0U;
    tx_status->data.reb_status.reserved                 = 0U;
}

/**
 * @brief Populates a TX message structure for the REB derate command frame (0x400).
 *
 * @param out       Pointer to FSM outputs. Must not be NULL.
 * @param tx_derate Pointer to the TX message to populate. Must not be NULL.
 */
void can_adapter_outputs_to_tx_derate(const reb_outputs_t *out,
                                       can_tx_message_t    *tx_derate)
{
    if ((out == NULL) || (tx_derate == NULL)) {
        return;
    }

    tx_derate->msg_id = CAN_MSG_REB_DERATE_CMD;

    tx_derate->data.reb_derate_cmd.derate_pct = out->derate_pct;

    tx_derate->data.reb_derate_cmd.derate_mode =
        out->fuel_derating_active ? CAN_DERATE_MODE_GRADUAL_RAMP
                                  : CAN_DERATE_MODE_OFF;

    /**
     * @c safety_flag encodes starter permission: 1 = start allowed, 0 = start blocked.
     * Corresponds to the starterOk field in REB_DERATE_CMD (DBC, byte 1).
     */
    tx_derate->data.reb_derate_cmd.safety_flag =
        out->starter_ok ? 1U : 0U;
}

/**
 * @brief Populates a TX message structure for the REB prevent-start frame (0x401).
 *
 * @details @c auth_token_lsb is reserved and set to zero in this implementation.
 *
 * @param out         Pointer to FSM outputs. Must not be NULL.
 * @param tx_prevent  Pointer to the TX message to populate. Must not be NULL.
 */
void can_adapter_outputs_to_tx_prevent_start(const reb_outputs_t *out,
                                              can_tx_message_t    *tx_prevent)
{
    if ((out == NULL) || (tx_prevent == NULL)) {
        return;
    }

    tx_prevent->msg_id = CAN_MSG_REB_PREVENT_START;

    tx_prevent->data.reb_prevent_start.prevent_start =
        out->starter_ok ? CAN_PREVENT_START_ALLOW
                        : CAN_PREVENT_START_BLOCK;

    tx_prevent->data.reb_prevent_start.auth_token_lsb = 0U;
}
