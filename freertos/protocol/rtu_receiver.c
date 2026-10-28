#include "tp_rtu_receiver.h"
#include <string.h>
static void finish(tp_rtu_receiver *r) {
    if (r->receiving && !r->invalid && r->length) r->frame(r->ctx, r->buffer, r->length);
    r->receiving = false; r->invalid = false; r->length = 0;
}
tp_result tp_rtu_receiver_init(tp_rtu_receiver *r, uint32_t baud, uint8_t bits,
                                void *ctx, void (*frame)(void *, const uint8_t *, size_t)) {
    if (!r || !frame || baud < 300 || baud > 1000000 || bits < 9 || bits > 12) return TP_INVALID;
    memset(r, 0, sizeof(*r)); r->ctx = ctx; r->frame = frame;
    r->char_us = (uint32_t)(((uint64_t)bits * 1000000u + baud - 1u) / baud);
    if (baud > 19200) { r->gap_1_5_us = 750; r->gap_3_5_us = 1750; }
    else {
        /* Round up so a frame is never closed before the required quiet time. */
        uint64_t unit = (uint64_t)bits * 1000000u;
        r->gap_1_5_us = (uint32_t)((unit * 3u + 2u * baud - 1u) / (2u * baud));
        r->gap_3_5_us = (uint32_t)((unit * 7u + 2u * baud - 1u) / (2u * baud));
    }
    return TP_OK;
}
void tp_rtu_receiver_byte(tp_rtu_receiver *r, uint8_t byte, uint32_t now) {
    if (!r || !r->frame) return;
    if (r->receiving) {
        uint32_t gap = now - r->last_byte_us;
        if (gap >= r->gap_3_5_us + r->char_us) finish(r);
        else if (gap > r->gap_1_5_us + r->char_us) r->invalid = true;
    }
    if (!r->receiving) { r->receiving = true; r->invalid = false; r->length = 0; }
    r->last_byte_us = now;
    if (r->length < sizeof(r->buffer)) r->buffer[r->length++] = byte;
    else r->invalid = true;
}
void tp_rtu_receiver_error(tp_rtu_receiver *r, uint32_t now) {
    if (!r || !r->frame) return;
    if (r->receiving && (uint32_t)(now-r->last_byte_us) >= r->gap_3_5_us + r->char_us) finish(r);
    r->receiving = true; r->invalid = true; r->last_byte_us = now;
}
void tp_rtu_receiver_poll(tp_rtu_receiver *r, uint32_t now) {
    if (r && r->frame && r->receiving && (uint32_t)(now-r->last_byte_us) >= r->gap_3_5_us + r->char_us) finish(r);
}

