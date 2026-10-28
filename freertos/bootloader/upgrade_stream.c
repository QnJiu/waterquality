#include "tp_upgrade_stream.h"
#include <string.h>

static bool expired(const tp_upgrade_stream *s, uint32_t now) {
    return s->partial && (uint32_t)(now - s->last_byte_ms) >= s->timeout_ms;
}
tp_result tp_upgrade_stream_init(tp_upgrade_stream *s, const tp_upgrade_commands *commands,
    void *ctx, tp_result (*send)(void *, const uint8_t *, size_t), uint32_t timeout_ms) {
    if (!s || !timeout_ms || timeout_ms > 0x7fffffffu) return TP_INVALID;
    memset(s, 0, sizeof(*s));
    tp_result result = tp_upgrade_wire_init(&s->wire, commands, ctx, send);
    if (result == TP_OK) s->timeout_ms = timeout_ms;
    return result;
}
void tp_upgrade_stream_reset(tp_upgrade_stream *s) {
    if (!s) return;
    tp_upgrade_wire_reset(&s->wire);
    s->partial = false;
}
void tp_upgrade_stream_poll(tp_upgrade_stream *s, uint32_t now_ms) {
    if (s && s->timeout_ms && expired(s, now_ms)) tp_upgrade_stream_reset(s);
}
tp_result tp_upgrade_stream_byte(tp_upgrade_stream *s, uint8_t byte, uint32_t received_ms) {
    if (!s || !s->timeout_ms) return TP_INVALID;
    tp_upgrade_stream_poll(s, received_ms);
    tp_result result = tp_upgrade_wire_byte(&s->wire, byte);
    /* A delimiter can terminate one frame and begin another. Keep its time so
     * a delayed subsequent byte cannot attach to a stale delimiter. */
    s->partial = s->wire.started;
    s->last_byte_ms = received_ms;
    return result;
}
