#ifndef TP_FREERTOS_CLOCK_H
#define TP_FREERTOS_CLOCK_H
#include "tp_clock_cache.h"
#include "tp_ds3231.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
typedef struct {
    tp_ds3231 *rtc;
    QueueHandle_t snapshot;
    TaskHandle_t task;
    uint32_t poll_ms, max_age_ms;
} tp_freertos_clock;
/* Initialize and start before scheduler; RTC context must outlive the task.
 * All RTC writes must be serialized externally with the polling task. */
tp_result tp_freertos_clock_start(tp_freertos_clock *clock, tp_ds3231 *rtc,
    uint32_t poll_ms, uint32_t max_age_ms, uint16_t stack_words, UBaseType_t priority);
/* Nonblocking and task-safe, suitable for a runtime rtc_utc callback wrapper. */
bool tp_freertos_clock_now(tp_freertos_clock *clock, int64_t *utc_seconds);
#endif
