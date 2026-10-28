#ifndef TP_FREERTOS_EVENT_LOG_H
#define TP_FREERTOS_EVENT_LOG_H
#include "tp_event_log.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#define TP_EVENT_QUEUE_DEPTH 16u
typedef struct { uint32_t queued, written, dropped, io_errors; } tp_event_log_stats;
typedef struct {
    tp_event_log_io io; tp_event_log journal; QueueHandle_t queue; TaskHandle_t task;
    tp_event_log_stats stats;
} tp_freertos_event_log;
/* Create before scheduler. The single worker owns journal and performs all I/O. */
tp_result tp_freertos_event_log_start(tp_freertos_event_log *,const tp_event_log_io *,
                                      uint16_t stack_words,UBaseType_t priority);
/* Copy and return immediately. Full queue increments dropped count. Task-only. */
tp_result tp_freertos_event_log_submit(tp_freertos_event_log *,const tp_event *);
void tp_freertos_event_log_stats(tp_freertos_event_log *,tp_event_log_stats *);
#endif
