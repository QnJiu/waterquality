#include "tp_subordinate_link.h"
#include <string.h>

static void frame(void *ctx,const uint8_t *data,size_t len) {
    tp_sub_link *l=ctx;
    if(l->role==TP_SUB_LINK_MASTER) {
        /* A queued frame met its deadline if its last byte arrived in time.
         * Freshness still uses the current time, so old data cannot revive. */
        uint32_t received=l->last_byte_timed ? l->last_byte_ms : l->now_ms;
        l->frame_result=tp_sub_poll_receive(&l->poll,received,data,len);
        if(l->frame_result==TP_OK && l->last_byte_timed)
            tp_sub_master_tick(&l->poll.master,l->now_ms);
        return;
    }
    /* Validate before reading sensors or sending a response. */
    tp_sub_status status={0}; uint8_t reply[TP_SUB_RESPONSE_SIZE];
    tp_result r=tp_sub_reply(data,len,&status,reply);
    if(r==TP_OK) r=l->snapshot(l->ctx,&status);
    if(r==TP_OK) r=tp_sub_reply(data,len,&status,reply);
    if(r==TP_OK) r=l->send(l->ctx,reply,sizeof(reply));
    l->frame_result=r;
}
static tp_result init(tp_sub_link *l,void *ctx,tp_sub_send send,uint32_t baud,uint8_t bits) {
    if(!l || !send) return TP_INVALID;
    memset(l,0,sizeof(*l)); l->ctx=ctx; l->send=send;
    return tp_rtu_receiver_init(&l->receiver,baud,bits,l,frame);
}
tp_result tp_sub_link_master_init(tp_sub_link *l,void *ctx,tp_sub_send send,uint32_t baud,uint8_t bits,
                                  uint32_t period,uint32_t timeout,uint32_t freshness,uint32_t first) {
    tp_result r=init(l,ctx,send,baud,bits); if(r!=TP_OK) return r;
    r=tp_sub_poll_init(&l->poll,ctx,send,period,timeout,freshness,first);
    if(r==TP_OK) { l->role=TP_SUB_LINK_MASTER; l->initialized=true; }
    return r;
}
tp_result tp_sub_link_slave_init(tp_sub_link *l,void *ctx,tp_sub_send send,tp_sub_snapshot snapshot,
                                 uint32_t baud,uint8_t bits) {
    if(!snapshot) return TP_INVALID;
    tp_result r=init(l,ctx,send,baud,bits); if(r!=TP_OK) return r;
    l->role=TP_SUB_LINK_SLAVE; l->snapshot=snapshot; l->initialized=true; return TP_OK;
}
tp_result tp_sub_link_byte(tp_sub_link *l,uint8_t byte,uint32_t us,uint32_t ms) {
    if(!l || !l->initialized) return TP_INVALID;
    l->now_ms=ms; l->frame_result=TP_OK; tp_rtu_receiver_byte(&l->receiver,byte,us);
    l->last_byte_timed=false;
    return l->frame_result;
}
tp_result tp_sub_link_byte_at(tp_sub_link *l,uint8_t byte,uint32_t us,uint32_t received_ms,uint32_t now_ms) {
    if(!l || !l->initialized || (uint32_t)(now_ms-received_ms)>0x7fffffffu) return TP_INVALID;
    l->now_ms=now_ms; l->frame_result=TP_OK; tp_rtu_receiver_byte(&l->receiver,byte,us);
    l->last_byte_ms=received_ms; l->last_byte_timed=true;
    return l->frame_result;
}
tp_result tp_sub_link_error(tp_sub_link *l,uint32_t us,uint32_t ms) {
    if(!l || !l->initialized) return TP_INVALID;
    l->now_ms=ms; l->frame_result=TP_OK; tp_rtu_receiver_error(&l->receiver,us);
    if(l->role==TP_SUB_LINK_MASTER) { l->poll.master.have_status=false; l->poll.master.pending=false; }
    return l->frame_result;
}
tp_result tp_sub_link_tick(tp_sub_link *l,uint32_t us,uint32_t ms) {
    if(!l || !l->initialized) return TP_INVALID;
    l->now_ms=ms; l->frame_result=TP_OK; tp_rtu_receiver_poll(&l->receiver,us);
    if(l->frame_result!=TP_OK) return l->frame_result;
    if(l->role==TP_SUB_LINK_MASTER) {
        /* Wait for the RTU gap before judging a buffered response. */
        if(l->receiver.receiving) return TP_OK;
        return tp_sub_poll_tick(&l->poll,ms);
    }
    return TP_OK;
}
bool tp_sub_link_healthy(const tp_sub_link *l,uint32_t ms) {
    return l && l->initialized && l->role==TP_SUB_LINK_MASTER && tp_sub_poll_healthy(&l->poll,ms);
}
uint16_t tp_sub_link_resource_faults(const tp_sub_link *l,uint32_t ms) {
    return tp_sub_poll_resource_faults(l && l->initialized && l->role==TP_SUB_LINK_MASTER ? &l->poll : NULL,ms);
}
