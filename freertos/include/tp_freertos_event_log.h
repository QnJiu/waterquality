#ifndef TP_FREERTOS_EVENT_LOG_H
#define TP_FREERTOS_EVENT_LOG_H
#include "tp_event_log.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#define TP_EVENT_QUEUE_DEPTH 16u
typedef struct { uint32_t queued, written, dropped, io_errors; } tp_event_log_stats;
typedef struct { bool query, latest; uint32_t sequence; tp_event event; } tp_event_log_job;
typedef struct { tp_result result; tp_event event; } tp_event_log_reply;
typedef struct {
    tp_event_log_io io; tp_event_log journal; QueueHandle_t queue, replies; TaskHandle_t task;
    tp_event_log_stats stats; bool query_pending;
} tp_freertos_event_log;
/* Create before scheduler. The single worker owns journal and performs all I/O. */
tp_result tp_freertos_event_log_start(tp_freertos_event_log *,const tp_event_log_io *,
                                      uint16_t stack_words,UBaseType_t priority);
/* Copies and returns immediately. Full queue increments dropped count. Task-only. */
tp_result tp_freertos_event_log_submit(tp_freertos_event_log *,const tp_event *);
/* Query/poll must have one task owner; only one request may be outstanding.
 * TP_INVALID in a reply denotes an empty log or absent/overwritten sequence. */
tp_result tp_freertos_event_log_query(tp_freertos_event_log *,bool latest,uint32_t sequence);
bool tp_freertos_event_log_poll(tp_freertos_event_log *,tp_result *,tp_event *);
void tp_freertos_event_log_stats(tp_freertos_event_log *,tp_event_log_stats *);
#endif
