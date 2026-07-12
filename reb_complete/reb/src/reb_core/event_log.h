/**
 * @file    event_log.h
 * @brief   Circular event log for forensic diagnostics.
 *
 * Provides event codes, record structure, and API for writing and
 * retrieving timestamped event entries. No cryptographic material
 * is stored in any record.
 */

#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include "reb/reb_types.h"
#include "reb/reb_params.h"

#define EVT_STATE_TRANSITION        0x01U
#define EVT_AUTH_FAIL               0x02U
#define EVT_AUTH_OK                 0x03U
#define EVT_PANEL_LOCKOUT           0x04U
#define EVT_SENSOR_THEFT            0x05U
#define EVT_DERATE_ACTIVE           0x06U
#define EVT_STARTER_INHIBIT         0x07U
#define EVT_UNBLOCK                 0x08U
#define EVT_REVERSAL_ABORT          0x09U
#define EVT_REVERSAL_EXPIRE         0x0AU
#define EVT_NVM_WRITE               0x0BU
#define EVT_NVM_RESTORE             0x0CU
#define EVT_SIGNAL_FAULT            0x0DU
#define EVT_CMD_RECEIVED            0x0EU
#define EVT_SPEED_SAFE_STOP         0x0FU
#define EVT_BLOCK_REJECT_SIGNAL     0x10U
#define EVT_BLOCK_REJECT_SPEED      0x11U
#define EVT_RX_SUPERVISION_FAIL     0x12U

/**
 * @brief Runtime context for the circular event log.
 *
 * @var entries  Array of event records forming the circular buffer.
 * @var head     Index of the next write position.
 * @var count    Number of valid entries currently stored.
 */
typedef struct {
    event_record_t entries[EVENT_LOG_MAX_ENTRIES];
    uint16_t       head;
    uint16_t       count;
} event_log_ctx_t;

/**
 * @brief Initialises the event log context to a known zero state.
 *
 * @param ctx  Pointer to the context to initialise. Must not be NULL.
 */
void evlog_init(event_log_ctx_t *ctx);

/**
 * @brief Writes one event record into the circular buffer.
 *
 * Overwrites the oldest entry when the buffer is full.
 *
 * @param ctx        Pointer to the event log context.
 * @param ts_ms      Timestamp in milliseconds.
 * @param code       Event code (EVT_* constant).
 * @param from       State before the event.
 * @param to         State after the event.
 * @param source     Identifier of the subsystem that generated the event.
 * @param auth_fail  Non-zero if the event is associated with an authentication failure.
 */
void evlog_write(event_log_ctx_t *ctx, uint32_t ts_ms,
                 uint8_t code, reb_state_t from, reb_state_t to,
                 uint8_t source, uint8_t auth_fail);

/**
 * @brief Retrieves an entry by sequential insertion index.
 *
 * @param ctx  Pointer to the event log context.
 * @param idx  Zero-based insertion index.
 * @return     Pointer to the record, or NULL if @p idx is out of range.
 */
const event_record_t *evlog_get(const event_log_ctx_t *ctx, uint16_t idx);

/**
 * @brief Retrieves an entry relative to the most recently written record.
 *
 * A @p back_idx of 0 returns the most recent entry; 1 returns the second
 * most recent, and so on.
 *
 * @param ctx       Pointer to the event log context.
 * @param back_idx  Reverse offset from the most recent entry.
 * @return          Pointer to the record, or NULL if @p back_idx >= count.
 */
const event_record_t *evlog_get_recent(const event_log_ctx_t *ctx,
                                        uint16_t back_idx);

/**
 * @brief Returns the number of valid entries in the log.
 *
 * @param ctx  Pointer to the event log context.
 * @return     Entry count, capped at EVENT_LOG_MAX_ENTRIES.
 */
uint16_t evlog_count(const event_log_ctx_t *ctx);



#endif /* EVENT_LOG_H */
