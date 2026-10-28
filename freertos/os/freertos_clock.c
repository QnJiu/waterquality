#include "tp_freertos_clock.h"
#if configTICK_RATE_HZ != 1000
#error "tp_freertos_clock requires a 1 kHz FreeRTOS tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit FreeRTOS ticks required");
static void poll_once(tp_freertos_clock *clock,bool setting,uint32_t requested) {
    tp_result set_result=setting ? tp_ds3231_set_utc(clock->rtc,requested) : TP_OK;
    tp_clock_snapshot s={0};
    s.valid=tp_ds3231_read_utc(clock->rtc,&s.utc_seconds)==TP_OK;
    s.sampled_ms=(uint32_t)xTaskGetTickCount();
    bool ok=!setting || (set_result==TP_OK && s.valid && s.utc_seconds >= requested && s.utc_seconds <= (int64_t)requested+1);
    if (!ok) s.valid=false;
    (void)xQueueOverwrite(clock->snapshot,&s);
    if (setting) {
        taskENTER_CRITICAL(); clock->set_status=ok ? 3 : 4; taskEXIT_CRITICAL();
    }
}
static void poll_clock(void *arg) {
    tp_freertos_clock *clock=arg;
    uint32_t requested=0;
    bool setting=false;
    for (;;) {
        poll_once(clock,setting,requested);
        setting=xQueueReceive(clock->set_requests,&requested,pdMS_TO_TICKS(clock->poll_ms))==pdPASS;
    }
}
tp_result tp_freertos_clock_start(tp_freertos_clock *clock,tp_ds3231 *rtc,
    uint32_t poll_ms,uint32_t max_age_ms,uint16_t stack_words,UBaseType_t priority) {
    if (!clock || !rtc || !poll_ms || poll_ms>0x7fffffffu ||
        max_age_ms<poll_ms || max_age_ms>0x7fffffffu || !stack_words || priority>=configMAX_PRIORITIES ||
        xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    clock->rtc=rtc; clock->poll_ms=poll_ms; clock->max_age_ms=max_age_ms; clock->task=NULL;
    clock->set_requests=NULL; clock->set_status=1;
    clock->snapshot=xQueueCreate(1,sizeof(tp_clock_snapshot));
    if (!clock->snapshot) return TP_IO_ERROR;
    clock->set_requests=xQueueCreate(1,sizeof(uint32_t));
    if (!clock->set_requests) { vQueueDelete(clock->snapshot); clock->snapshot=NULL; return TP_IO_ERROR; }
    if (xTaskCreate(poll_clock,"tp-clock",stack_words,clock,priority,&clock->task)!=pdPASS) {
        vQueueDelete(clock->set_requests); clock->set_requests=NULL;
        vQueueDelete(clock->snapshot); clock->snapshot=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}
tp_result tp_freertos_clock_set(tp_freertos_clock *clock,uint32_t seconds) {
    if (!clock || !clock->set_requests || seconds<946684800u || seconds>=4102444800u) return TP_INVALID;
    taskENTER_CRITICAL();
    if (clock->set_status==2) { taskEXIT_CRITICAL(); return TP_BUSY; }
    clock->set_status=2;
    if (xQueueSend(clock->set_requests,&seconds,0)!=pdPASS) {
        clock->set_status=4; taskEXIT_CRITICAL(); return TP_IO_ERROR;
    }
    taskEXIT_CRITICAL(); return TP_OK;
}
uint8_t tp_freertos_clock_set_status(tp_freertos_clock *clock) {
    if (!clock || !clock->set_requests) return 0;
    taskENTER_CRITICAL(); uint8_t status=clock->set_status; taskEXIT_CRITICAL(); return status;
}
bool tp_freertos_clock_now(tp_freertos_clock *clock,int64_t *utc) {
    tp_clock_snapshot s;
    if (!clock || !clock->snapshot || !utc) return false;
    uint8_t status=tp_freertos_clock_set_status(clock);
    /* A failed write can leave a partially updated RTC: require verified re-setting. */
    if (status==2 || status==4 || xQueuePeek(clock->snapshot,&s,0)!=pdPASS) return false;
    return tp_clock_snapshot_now(&s,(uint32_t)xTaskGetTickCount(),clock->max_age_ms,utc);
}
