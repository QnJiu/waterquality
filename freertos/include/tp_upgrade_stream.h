#ifndef TP_UPGRADE_STREAM_H
#define TP_UPGRADE_STREAM_H
#include "tp_upgrade_wire.h"

/* One task owns this session and all upgrade/boot operations. Clock values are
 * monotonic milliseconds modulo 2^32; poll or deliver an event at least once
 * per 2^31 ms. Events (including idle polls) must be delivered in timestamp
 * order; drain buffered bytes before polling using the current time. */
typedef struct {
    tp_upgrade_wire wire;
    uint32_t timeout_ms, last_byte_ms;
    bool partial;
} tp_upgrade_stream;

tp_result tp_upgrade_stream_init(tp_upgrade_stream *, const tp_upgrade_commands *,
    void *send_ctx, tp_result (*send)(void *, const uint8_t *, size_t), uint32_t timeout_ms);
/* Receive ordered bytes from a single connection. A gap at least timeout_ms
 * discards the previous partial frame before this byte is interpreted. */
tp_result tp_upgrade_stream_byte(tp_upgrade_stream *, uint8_t byte, uint32_t received_ms);
/* Call on idle, overrun, disconnect, or before accepting a new connection. */
void tp_upgrade_stream_poll(tp_upgrade_stream *, uint32_t now_ms);
void tp_upgrade_stream_reset(tp_upgrade_stream *);
#endif
