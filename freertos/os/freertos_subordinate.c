#include "tp_freertos_subordinate.h"
#include "task.h"
#include <string.h>
#if configTICK_RATE_HZ != 1000
#error "tp_freertos_subordinate requires a 1 kHz FreeRTOS tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit FreeRTOS ticks required");

static void publish(tp_freertos_subordinate *s) {
    (void)xQueueOverwrite(s->snapshot,&s->link.poll.master);
}
tp_result tp_freertos_subordinate_init(tp_freertos_subordinate *s,void *ctx,tp_sub_send send,
    uint32_t baud,uint8_t bits,uint32_t period,uint32_t timeout,uint32_t freshness,uint32_t first) {
    if(!s || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    memset(s,0,sizeof(*s));
    tp_result r=tp_sub_link_master_init(&s->link,ctx,send,baud,bits,period,timeout,freshness,first);
    if(r!=TP_OK) return r;
    s->snapshot=xQueueCreate(1,sizeof(tp_sub_master));
    if(!s->snapshot) return TP_IO_ERROR;
    publish(s); return TP_OK;
}
tp_result tp_freertos_subordinate_byte(tp_freertos_subordinate *s,uint8_t byte,uint32_t us,uint32_t ms) {
    if(!s || !s->snapshot) return TP_INVALID;
    tp_result r=tp_sub_link_byte(&s->link,byte,us,ms); publish(s); return r;
}
tp_result tp_freertos_subordinate_error(tp_freertos_subordinate *s,uint32_t us,uint32_t ms) {
    if(!s || !s->snapshot) return TP_INVALID;
    tp_result r=tp_sub_link_error(&s->link,us,ms); publish(s); return r;
}
tp_result tp_freertos_subordinate_tick(tp_freertos_subordinate *s,uint32_t us,uint32_t ms) {
    if(!s || !s->snapshot) return TP_INVALID;
    tp_result r=tp_sub_link_tick(&s->link,us,ms); publish(s); return r;
}
bool tp_freertos_subordinate_healthy(void *ctx) {
    tp_freertos_subordinate *s=ctx; tp_sub_master m;
    return s && s->snapshot && xQueuePeek(s->snapshot,&m,0)==pdPASS &&
           tp_sub_master_healthy(&m,(uint32_t)xTaskGetTickCount());
}
uint16_t tp_freertos_subordinate_resource_faults(void *ctx) {
    tp_freertos_subordinate *s=ctx; tp_sub_master m;
    if(!s || !s->snapshot || xQueuePeek(s->snapshot,&m,0)!=pdPASS)
        return TP_PREFLIGHT_RESOURCE_UNKNOWN;
    return tp_sub_master_resource_faults(&m,(uint32_t)xTaskGetTickCount());
}
