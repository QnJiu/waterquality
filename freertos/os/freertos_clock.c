#include "tp_freertos_clock.h"
#if configTICK_RATE_HZ != 1000
#error "tp_freertos_clock requires a 1 kHz FreeRTOS tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit FreeRTOS ticks required");
static void poll_clock(void *arg) {
    tp_freertos_clock *clock=arg;
    for (;;) {
        tp_clock_snapshot s={0};
        s.valid=tp_ds3231_read_utc(clock->rtc,&s.utc_seconds)==TP_OK;
        s.sampled_ms=(uint32_t)xTaskGetTickCount();
        (void)xQueueOverwrite(clock->snapshot,&s);
        vTaskDelay(pdMS_TO_TICKS(clock->poll_ms));
    }
}
tp_result tp_freertos_clock_start(tp_freertos_clock *clock,tp_ds3231 *rtc,
    uint32_t poll_ms,uint32_t max_age_ms,uint16_t stack_words,UBaseType_t priority) {
    if (!clock || !rtc || !poll_ms || poll_ms>0x7fffffffu ||
        max_age_ms<poll_ms || max_age_ms>0x7fffffffu || !stack_words ||
        xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    clock->rtc=rtc; clock->poll_ms=poll_ms; clock->max_age_ms=max_age_ms; clock->task=NULL;
    clock->snapshot=xQueueCreate(1,sizeof(tp_clock_snapshot));
    if (!clock->snapshot) return TP_IO_ERROR;
    if (xTaskCreate(poll_clock,"tp-clock",stack_words,clock,priority,&clock->task)!=pdPASS) {
        vQueueDelete(clock->snapshot); clock->snapshot=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}
bool tp_freertos_clock_now(tp_freertos_clock *clock,int64_t *utc) {
    tp_clock_snapshot s;
    if (!clock || !clock->snapshot || !utc || xQueuePeek(clock->snapshot,&s,0)!=pdPASS) return false;
    return tp_clock_snapshot_now(&s,(uint32_t)xTaskGetTickCount(),clock->max_age_ms,utc);
}
