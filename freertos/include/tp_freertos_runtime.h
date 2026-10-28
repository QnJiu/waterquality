#ifndef TP_FREERTOS_RUNTIME_H
#define TP_FREERTOS_RUNTIME_H
#include "tp_modbus.h"
#include "tp_service.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#define TP_RUNTIME_PORTS 3u
#define TP_RUNTIME_FRAME_MAX 256u
#define TP_RUNTIME_QUEUE_DEPTH 8u
#define TP_RUNTIME_STOP_DEPTH 4u

typedef struct {
    uint16_t length;
    uint8_t port;
    uint8_t bytes[TP_RUNTIME_FRAME_MAX];
} tp_runtime_frame;
typedef struct {
    void *ctx;
    /* Must be nonblocking and copy the response before returning. */
    tp_result (*send)(void *ctx, unsigned port, const uint8_t *data, size_t len);
    /* Returns false if wall time is invalid; never blocks. */
    bool (*rtc_utc)(void *ctx, int64_t *seconds);
    /* Called on failed transmit/enqueue; never blocks. */
    void (*communication_error)(void *ctx, unsigned port);
} tp_runtime_io;
typedef struct {
    tp_service *service;
    tp_runtime_io io;
    tp_modbus ports[TP_RUNTIME_PORTS];
    size_t port_count;
    QueueHandle_t requests, stops, save_requests, save_results;
    tp_store_io storage;
    TaskHandle_t storage_task;
    TaskHandle_t owner;
    uint32_t tick_ms;
} tp_freertos_runtime;
/* Call before scheduler starts. Queues are owned by the runtime. The task stack,
 * FreeRTOS heap and priority are application configuration responsibilities. */
tp_result tp_runtime_init(tp_freertos_runtime *rt, tp_service *service,
    const tp_runtime_io *io, const uint8_t *addresses, size_t port_count, uint32_t tick_ms);
/* Attach before start; cfg is loaded by the caller or a validated default. */
tp_result tp_runtime_attach_storage(tp_freertos_runtime *rt, const tp_store_io *store, const tp_config *cfg);
tp_result tp_runtime_start(tp_freertos_runtime *rt, uint16_t stack_words, UBaseType_t priority);
/* Call from a UART task, not an ISR. Input is copied; queue-full returns TP_BUSY. */
tp_result tp_runtime_submit(tp_freertos_runtime *rt, unsigned port, const uint8_t *frame, size_t length);
#endif

