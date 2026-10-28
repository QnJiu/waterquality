#include "tp_rtu_port.h"
#include <string.h>
static void frame(void *ctx, const uint8_t *data, size_t len) {
    tp_rtu_port *p = ctx;
    tp_result r = p->submit(p->ctx, p->port, data, len);
    if (r != TP_OK && p->error) p->error(p->ctx, p->port, r);
}
tp_result tp_rtu_port_init(tp_rtu_port *p, unsigned port, uint32_t baud, uint8_t bits,
    void *ctx, tp_result (*submit)(void *, unsigned, const uint8_t *, size_t),
    void (*error)(void *, unsigned, tp_result)) {
    if (!p || !submit) return TP_INVALID;
    memset(p, 0, sizeof(*p)); p->port=port; p->ctx=ctx; p->submit=submit; p->error=error;
    tp_result r=tp_rtu_receiver_init(&p->receiver,baud,bits,p,frame);
    if (r != TP_OK) memset(p, 0, sizeof(*p));
    return r;
}
void tp_rtu_port_byte(tp_rtu_port *p, uint8_t byte, uint32_t now) {
    if (p) tp_rtu_receiver_byte(&p->receiver,byte,now);
}
void tp_rtu_port_error(tp_rtu_port *p, uint32_t now) {
    if (!p) return;
    tp_rtu_receiver_error(&p->receiver,now);
    if (p->receiver.frame && p->error) p->error(p->ctx,p->port,TP_IO_ERROR);
}
void tp_rtu_port_overflow(tp_rtu_port *p, uint32_t now) {
    if (!p || !p->receiver.frame) return;
    /* The lost event may have belonged to the buffered frame. */
    p->receiver.receiving = true;
    p->receiver.invalid = true;
    p->receiver.length = 0;
    p->receiver.last_byte_us = now;
    if (p->error) p->error(p->ctx,p->port,TP_IO_ERROR);
}
void tp_rtu_port_poll(tp_rtu_port *p, uint32_t now) {
    if (p) tp_rtu_receiver_poll(&p->receiver,now);
}
