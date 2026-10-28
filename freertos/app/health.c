#include "tp_health.h"
#include <string.h>

tp_result tp_health_init(tp_health *h,const uint32_t *timeouts,uint8_t count) {
    if(!h || !timeouts || !count || count>TP_HEALTH_MAX_TASKS) return TP_INVALID;
    for(uint8_t i=0;i<count;i++)
        if(!timeouts[i] || timeouts[i]>=0x80000000u) return TP_INVALID;
    memset(h,0,sizeof(*h)); h->count=count;
    for(uint8_t i=0;i<count;i++) h->deadline_ms[i]=timeouts[i];
    return TP_OK;
}
tp_result tp_health_arm(tp_health *h,uint32_t now) {
    if(!h || !h->count || h->count>TP_HEALTH_MAX_TASKS || h->armed) return TP_INVALID;
    for(uint8_t i=0;i<h->count;i++) h->last_ms[i]=now;
    h->failed_mask=0; h->armed=true; return TP_OK;
}
bool tp_health_can_feed(tp_health *h,uint32_t now) {
    if(!h || !h->armed || !h->count || h->count>TP_HEALTH_MAX_TASKS) return false;
    for(uint8_t i=0;i<h->count;i++)
        if((uint32_t)(now-h->last_ms[i])>=h->deadline_ms[i]) h->failed_mask|=1u<<i;
    return h->failed_mask==0;
}
tp_result tp_health_heartbeat(tp_health *h,uint8_t task,uint32_t now) {
    if(!h || !h->armed || task>=h->count || h->count>TP_HEALTH_MAX_TASKS) return TP_INVALID;
    if(!tp_health_can_feed(h,now)) return TP_TIMEOUT;
    h->last_ms[task]=now; h->seen_mask|=1u<<task; return TP_OK;
}

bool tp_health_all_started(tp_health *h,uint32_t now) {
    if(!tp_health_can_feed(h,now)) return false;
    uint32_t expected=(1u<<h->count)-1u;
    return (h->seen_mask & expected)==expected;
}
