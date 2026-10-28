#ifndef TP_FREERTOS_TRIAL_H
#define TP_FREERTOS_TRIAL_H
#include "tp_trial_confirm.h"
#include "FreeRTOS.h"
#include "task.h"
/* All callbacks run in task context. try_lock and healthy must not block;
 * healthy must reflect faults latched between samples. The lock must also
 * protect all concurrent upgrade and image modification operations. finished
 * handles failure policy (e.g. a safe reset); the task does not reset itself.
 * The object, boot I/O and callback context must outlive the task. */
typedef struct {
    tp_trial_confirm gate;
    void *ctx;
    bool (*try_lock)(void *);
    void (*unlock)(void *);
    bool (*healthy)(void *);
    void (*finished)(void *,tp_result);
    uint32_t period_ms, timeout_ms, started_ms;
    bool started, done;
    TaskHandle_t task;
} tp_freertos_trial;
tp_result tp_freertos_trial_start(tp_freertos_trial *,const tp_boot_io *,uint8_t bank,
    uint32_t stable_ms,uint32_t max_gap_ms,uint32_t period_ms,uint32_t timeout_ms,
    uint16_t stack_words,UBaseType_t priority,void *ctx,
    bool (*try_lock)(void *),void (*unlock)(void *),bool (*healthy)(void *),
    void (*finished)(void *,tp_result));
/* One owner only; never call concurrently with the task. Exposed for tests. */
tp_result tp_freertos_trial_step(tp_freertos_trial *,uint32_t now_ms);
#endif
