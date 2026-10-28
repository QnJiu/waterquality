#ifndef TP_FREERTOS_STAGE_TRANSPORT_H
#define TP_FREERTOS_STAGE_TRANSPORT_H
#include "tp_stage_session.h"
#include "tp_upgrade_stream.h"
#include "FreeRTOS.h"
#include "task.h"
/* One connection at a time. receive is nonblocking: TP_OK yields one byte and its receive time in 1 kHz ticks,
 * TP_BUSY means no buffered data, TP_IO_ERROR means disconnected/error. send must send
 * the whole frame before returning; bounded waits only. finished reports an
 * installation result and may arrange a safe reset. Callbacks run in task
 * context. Receive timestamps must be in arrival order, use the same modulo-2^32
 * clock as the task, and be less than 2^31 ms from each task step. The transport provides
 * authentication/access control as needed. */
typedef struct {
    void *ctx;
    tp_result (*receive)(void *,uint8_t *,uint32_t *received_ms);
    tp_result (*send)(void *,const uint8_t *,size_t);
    void (*finished)(void *,tp_result);
} tp_stage_transport_io;
typedef struct {
    tp_stage_session *session;
    tp_stage_transport_io io;
    tp_upgrade_stream stream;
    tp_upgrade_commands placeholder;
    uint32_t idle_ms, period_ms, last_byte_ms;
    bool pending, installing;
    TaskHandle_t task;
} tp_freertos_stage_transport;
/* Starts before scheduler. Session and callback contexts must outlive task.
 * Use only one owner; never reinitialize an active object. */
tp_result tp_freertos_stage_transport_start(tp_freertos_stage_transport *,tp_stage_session *,
    const tp_stage_transport_io *,uint32_t frame_timeout_ms,uint32_t idle_ms,
    uint32_t period_ms,uint16_t stack_words,UBaseType_t priority);
/* One owner, exposed for deterministic tests. Call regularly in tick order. */
void tp_freertos_stage_transport_step(tp_freertos_stage_transport *,uint32_t now_ms);
#endif
