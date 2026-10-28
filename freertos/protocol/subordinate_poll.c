#include "tp_subordinate_poll.h"
#include <string.h>

tp_result tp_sub_poll_init(tp_sub_poll *p,void *ctx,tp_sub_send send,uint32_t period,
                           uint32_t timeout,uint32_t freshness,uint32_t first) {
    if(!p || !send || !period || period>0x7fffffffu || period<timeout) return TP_INVALID;
    memset(p,0,sizeof(*p));
    tp_result r=tp_sub_master_init(&p->master,timeout,freshness,first);
    if(r!=TP_OK) return r;
    p->ctx=ctx; p->send=send; p->period_ms=period;
    return TP_OK;
}
tp_result tp_sub_poll_tick(tp_sub_poll *p,uint32_t now) {
    if(!p || !p->send || !p->period_ms) return TP_INVALID;
    tp_sub_master_tick(&p->master,now);
    if(p->master.pending || (p->attempted && (uint32_t)(now-p->last_attempt_ms)<p->period_ms))
        return TP_OK;
    uint8_t frame[TP_SUB_REQUEST_SIZE];
    tp_result r=tp_sub_master_request(&p->master,now,frame);
    if(r!=TP_OK) return r;
    p->attempted=true; p->last_attempt_ms=now;
    r=p->send(p->ctx,frame,sizeof(frame));
    if(r!=TP_OK) {
        p->master.pending=false; p->master.have_status=false;
        return r;
    }
    return TP_OK;
}
tp_result tp_sub_poll_receive(tp_sub_poll *p,uint32_t now,const uint8_t *frame,size_t len) {
    if(!p || !p->send) return TP_INVALID;
    return tp_sub_master_receive(&p->master,now,frame,len);
}
bool tp_sub_poll_healthy(const tp_sub_poll *p,uint32_t now) {
    return p && tp_sub_master_healthy(&p->master,now);
}
uint16_t tp_sub_poll_resource_faults(const tp_sub_poll *p,uint32_t now) {
    return tp_sub_master_resource_faults(p ? &p->master : NULL,now);
}
