#include "tp_subordinate_app.h"
#include <string.h>

static tp_result send(void *ctx,const uint8_t *data,size_t len) {
    tp_sub_app *a=ctx; return a->io.send(a->io.ctx,data,len);
}
static tp_result inputs(void *ctx,tp_sub_inputs *out) {
    tp_sub_app *a=ctx; tp_result r=a->io.read_inputs(a->io.ctx,out);
    if(r==TP_OK && a->door_fault_is_device_fault && a->door &&
       (a->door->door.state==TP_DOOR_FAULT || a->door_unavailable)) out->device_fault=true;
    return r;
}
tp_result tp_sub_app_init(tp_sub_app *a,const tp_sub_node_io *io,uint32_t baud,uint8_t bits,
                          tp_door_service *door,bool door_fault_is_device_fault) {
    if(!a || !io || !io->send || !io->read_inputs ||
       (door && (!door->request || !door->door.io.set_unlocked))) return TP_INVALID;
    tp_sub_node_io copy=*io; memset(a,0,sizeof(*a));
    a->io=copy; a->door=door; a->door_fault_is_device_fault=door_fault_is_device_fault;
    tp_sub_node_io adapted={a,send,inputs};
    return tp_sub_node_init(&a->node,&adapted,baud,bits);
}
tp_result tp_sub_app_tick(tp_sub_app *a,uint32_t us,uint32_t ms) {
    if(!a || !a->node.link.initialized) return TP_INVALID;
    a->door_result=a->door ? tp_door_service_tick(a->door,ms) : TP_OK;
    return tp_sub_node_tick(&a->node,us,ms);
}
bool tp_sub_app_sleep_budget(const tp_sub_app *a,uint32_t now,uint32_t maximum,
                             bool transport_idle,uint32_t *budget) {
    if(!a || !a->node.link.initialized || !budget || !transport_idle ||
       !maximum || maximum>0x7fffffffu || a->node.link.receiver.receiving) return false;
    uint32_t remaining=maximum;
    if(a->door) {
        const tp_door *d=&a->door->door; const tp_pn532_scan *s=&a->door->scan;
        if(a->door_unavailable || d->state!=TP_DOOR_LOCKED || d->auth_pending || s->state!=TP_SCAN_WAIT) return false;
        uint32_t elapsed=now-s->last_scan_ms;
        if(elapsed>=s->interval_ms) return false;
        uint32_t next=s->interval_ms-elapsed;
        if(next<remaining) remaining=next;
        if(d->observed) {
            elapsed=now-d->observed_at;
            if(elapsed>=d->freshness_ms) return false;
            next=d->freshness_ms-elapsed;
            if(next<remaining) remaining=next;
        }
    }
    *budget=remaining; return true;
}
