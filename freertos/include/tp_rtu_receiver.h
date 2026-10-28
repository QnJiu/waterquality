#ifndef TP_RTU_RECEIVER_H
#define TP_RTU_RECEIVER_H
#include "tp_core.h"
typedef struct {
    void *ctx;
    /* Callback consumes the frame before returning; it cannot retain the pointer. */
    void (*frame)(void *ctx, const uint8_t *frame, size_t length);
    uint8_t buffer[256];
    size_t length;
    uint32_t last_byte_us, char_us, gap_1_5_us, gap_3_5_us;
    bool receiving, invalid;
} tp_rtu_receiver;
/* bits_per_char includes start, data, parity and stop bits. For baud > 19200,
 * Modbus serial line timing uses fixed 750/1750 us. */
tp_result tp_rtu_receiver_init(tp_rtu_receiver *r, uint32_t baud, uint8_t bits_per_char,
                                void *ctx, void (*frame)(void *, const uint8_t *, size_t));
/* Timestamps are byte-completion times. Deliver received bytes before polling
 * at the same timestamp; do not poll while an undelivered UART byte is pending.
 * UART errors call tp_rtu_receiver_error. */
void tp_rtu_receiver_byte(tp_rtu_receiver *r, uint8_t byte, uint32_t now_us);
void tp_rtu_receiver_error(tp_rtu_receiver *r, uint32_t now_us);
void tp_rtu_receiver_poll(tp_rtu_receiver *r, uint32_t now_us);
#endif

