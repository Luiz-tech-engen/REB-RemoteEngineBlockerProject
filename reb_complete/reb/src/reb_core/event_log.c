/**
 * @file    event_log.c
 * @brief   Circular event log implementation.
 *
 * Maintains a fixed-capacity circular buffer of @c EVENT_LOG_MAX_ENTRIES
 * records. No sensitive or cryptographic data is stored in any field.
 */

#include "event_log.h"
#include <string.h>

void evlog_init(event_log_ctx_t *ctx)
{
    (void)memset(ctx, 0, sizeof(*ctx));
}

void evlog_write(event_log_ctx_t *ctx, uint32_t ts_ms,
                 uint8_t code, reb_state_t from, reb_state_t to,
                 uint8_t source, uint8_t auth_fail)
{
    event_record_t *rec = &ctx->entries[ctx->head];

    rec->timestamp_ms = ts_ms;
    rec->event_code   = code;
    rec->state_from   = (uint8_t)from;
    rec->state_to     = (uint8_t)to;
    rec->source       = source;
    rec->auth_fail    = auth_fail;
    rec->_pad[0] = 0U;
    rec->_pad[1] = 0U;
    rec->_pad[2] = 0U;

    ctx->head = (uint16_t)((ctx->head + 1U) % EVENT_LOG_MAX_ENTRIES);
    if (ctx->count < EVENT_LOG_MAX_ENTRIES) {
        ctx->count++;
    }
}

const event_record_t *evlog_get(const event_log_ctx_t *ctx, uint16_t idx)
{
    if (idx >= ctx->count) {
        return NULL;
    }
    return &ctx->entries[idx % EVENT_LOG_MAX_ENTRIES];
}

const event_record_t *evlog_get_recent(const event_log_ctx_t *ctx,
                                        uint16_t back_idx)
{
    uint16_t pos;
    if (back_idx >= ctx->count) {
        return NULL;
    }
    /**
     * @note @c head points to the next write slot, so @c (head - 1) is the
     *       most recently written entry. The buffer size is added before
     *       subtracting to avoid unsigned underflow.
     */
    pos = (uint16_t)((ctx->head
                      + (uint16_t)EVENT_LOG_MAX_ENTRIES
                      - 1U
                      - back_idx)
                     % (uint16_t)EVENT_LOG_MAX_ENTRIES);
    return &ctx->entries[pos];
}


uint16_t evlog_count(const event_log_ctx_t *ctx)
{
    return ctx->count;
}
