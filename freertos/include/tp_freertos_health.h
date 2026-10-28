#ifndef TP_FREERTOS_HEALTH_H
#define TP_FREERTOS_HEALTH_H
#include "tp_health.h"
#include "FreeRTOS.h"
#include "task.h"
typedef struct {
    tp_health health;
    TaskHandle_t task;
    void *ctx;
    void (*feed)(void *);
    void (*fault)(void *,uint32_t);
    uint32_t period_ms;
} tp_freertos_health;
/* Pre-scheduler only. Period must be shorter than every deadline. Callbacks
 * must not block; feed executes in a critical section and must be very short.
 * Give the supervisor higher priority than monitored tasks so it arms first.
 * fault is optional and called once; feed is never called after
 * a timeout. The object and callback context must remain valid until reset. */
tp_result tp_freertos_health_start(tp_freertos_health *,const uint32_t *,uint8_t,
    uint32_t period_ms,uint16_t stack_words,UBaseType_t priority,void *ctx,
    void (*feed)(void *),void (*fault)(void *,uint32_t));
/* Task context only. Call after the task has made meaningful progress. */
tp_result tp_freertos_health_heartbeat(tp_freertos_health *,uint8_t task_id);
#endif
