#ifndef TP_FREERTOS_CLOCK_H
#define TP_FREERTOS_CLOCK_H
#include "tp_clock_cache.h"
#include "tp_ds3231.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
typedef struct {
    tp_ds3231 *rtc;
    QueueHandle_t snapshot, set_requests;
    uint8_t set_status; /* 1 idle, 2 pending, 3 verified, 4 failed */
    TaskHandle_t task;
    uint32_t poll_ms, max_age_ms;
} tp_freertos_clock;
/* Initialize and start before scheduler; RTC context must outlive the task.
 * RTC access is owned by this task; do not access it elsewhere after start. */
tp_result tp_freertos_clock_start(tp_freertos_clock *clock, tp_ds3231 *rtc,
    uint32_t poll_ms, uint32_t max_age_ms, uint16_t stack_words, UBaseType_t priority);
/* Nonblocking request; seconds are Unix UTC, 2000-01-01 through 2099-12-31.
 * The clock task writes and verifies the device. Only one request may be pending. */
tp_result tp_freertos_clock_set(tp_freertos_clock *clock, uint32_t seconds);
uint8_t tp_freertos_clock_set_status(tp_freertos_clock *clock);
/* Nonblocking and task-safe, suitable for a runtime rtc_utc callback wrapper. */
bool tp_freertos_clock_now(tp_freertos_clock *clock, int64_t *utc_seconds);
#endif
