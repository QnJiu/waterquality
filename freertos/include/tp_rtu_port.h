#ifndef TP_RTU_PORT_H
#define TP_RTU_PORT_H
#include "tp_rtu_receiver.h"
/* One instance per UART. All calls on an instance belong to one UART task;
 * ISR/DMA code must queue timestamped events to that task in arrival order. */
typedef struct {
    tp_rtu_receiver receiver;
    void *ctx;
    unsigned port;
    tp_result (*submit)(void *ctx, unsigned port, const uint8_t *frame, size_t len);
    void (*error)(void *ctx, unsigned port, tp_result result);
} tp_rtu_port;
/* submit must copy the frame before returning and must not block. The instance
 * must not be moved after init, since receiver callbacks point to it. */
tp_result tp_rtu_port_init(tp_rtu_port *p, unsigned port, uint32_t baud, uint8_t bits,
    void *ctx, tp_result (*submit)(void *, unsigned, const uint8_t *, size_t),
    void (*error)(void *, unsigned, tp_result));
void tp_rtu_port_byte(tp_rtu_port *p, uint8_t byte, uint32_t completion_us);
void tp_rtu_port_error(tp_rtu_port *p, uint32_t completion_us);
void tp_rtu_port_poll(tp_rtu_port *p, uint32_t now_us);
#endif
