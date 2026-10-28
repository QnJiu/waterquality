#include "tp_freertos_health.h"
#include <string.h>
#if configTICK_RATE_HZ != 1000
#error "tp_freertos_health requires a 1 kHz FreeRTOS tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit FreeRTOS ticks required");
static void supervisor(void *arg) {
    tp_freertos_health *h=arg; TickType_t last=xTaskGetTickCount(); bool reported=false;
    taskENTER_CRITICAL(); (void)tp_health_arm(&h->health,(uint32_t)last); taskEXIT_CRITICAL();
    for(;;) {
        bool healthy; uint32_t failed;
        taskENTER_CRITICAL();
        healthy=tp_health_can_feed(&h->health,(uint32_t)xTaskGetTickCount());
        failed=h->health.failed_mask;
        if(healthy) h->feed(h->ctx);
        taskEXIT_CRITICAL();
        if(!healthy && !reported) { reported=true; if(h->fault) h->fault(h->ctx,failed); }
        vTaskDelayUntil(&last,pdMS_TO_TICKS(h->period_ms));
    }
}
tp_result tp_freertos_health_start(tp_freertos_health *h,const uint32_t *timeouts,uint8_t count,
    uint32_t period,uint16_t stack,UBaseType_t priority,void *ctx,
    void (*feed)(void *),void (*fault)(void *,uint32_t)) {
    if(!h || !timeouts || !period || !stack || !feed ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    tp_health checked;
    if(tp_health_init(&checked,timeouts,count)!=TP_OK) return TP_INVALID;
    for(uint8_t i=0;i<count;i++) if(period>=timeouts[i]) return TP_INVALID;
    memset(h,0,sizeof(*h)); h->health=checked; h->period_ms=period;
    h->ctx=ctx; h->feed=feed; h->fault=fault;
    if(xTaskCreate(supervisor,"tp-health",stack,h,priority,&h->task)!=pdPASS) return TP_IO_ERROR;
    return TP_OK;
}
tp_result tp_freertos_health_heartbeat(tp_freertos_health *h,uint8_t id) {
    if(!h || !h->task) return TP_INVALID;
    tp_result r;
    taskENTER_CRITICAL(); r=tp_health_heartbeat(&h->health,id,(uint32_t)xTaskGetTickCount()); taskEXIT_CRITICAL();
    return r;
}
