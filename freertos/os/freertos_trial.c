#include "tp_freertos_trial.h"
#include <string.h>
#if configTICK_RATE_HZ != 1000
#error "tp_freertos_trial requires a 1 kHz tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit ticks required");
tp_result tp_freertos_trial_step(tp_freertos_trial *t,uint32_t now) {
    if(!t || !t->healthy || !t->try_lock || !t->unlock || !t->timeout_ms || t->done) return TP_INVALID;
    if(!t->started) { t->started=true; t->started_ms=now; }
    if((uint32_t)(now-t->started_ms)>=t->timeout_ms) { t->done=true; return TP_TIMEOUT; }
    if(!t->try_lock(t->ctx)) { t->gate.timing=false; return TP_BUSY; }
    bool healthy=t->healthy(t->ctx);
    tp_result r=tp_trial_confirm_poll(&t->gate,now,healthy);
    t->unlock(t->ctx);
    if(r==TP_OK || r==TP_INVALID) t->done=true;
    return r;
}
static void trial_task(void *arg) {
    tp_freertos_trial *t=arg;
    for(;;) {
        tp_result r=tp_freertos_trial_step(t,(uint32_t)xTaskGetTickCount());
        if(t->done) { t->finished(t->ctx,r); vTaskDelete(NULL); return; }
        vTaskDelay(pdMS_TO_TICKS(t->period_ms));
    }
}
tp_result tp_freertos_trial_start(tp_freertos_trial *t,const tp_boot_io *boot,uint8_t bank,
    uint32_t stable,uint32_t max_gap,uint32_t period,uint32_t timeout,uint16_t stack,
    UBaseType_t priority,void *ctx,bool (*try_lock)(void *),void (*unlock)(void *),
    bool (*healthy)(void *),void (*finished)(void *,tp_result)) {
    if(!t || !try_lock || !unlock || !healthy || !finished || !period || period>max_gap ||
       timeout<=stable || timeout>=0x80000000u || !stack || priority>=configMAX_PRIORITIES ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    memset(t,0,sizeof(*t));
    tp_result r=tp_trial_confirm_init(&t->gate,boot,bank,stable,max_gap); if(r!=TP_OK) return r;
    t->ctx=ctx; t->try_lock=try_lock; t->unlock=unlock; t->healthy=healthy; t->finished=finished;
    t->period_ms=period; t->timeout_ms=timeout;
    if(xTaskCreate(trial_task,"tp-trial",stack,t,priority,&t->task)!=pdPASS) { memset(t,0,sizeof(*t)); return TP_IO_ERROR; }
    return TP_OK;
}
