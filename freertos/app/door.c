#include "tp_door.h"
#include <string.h>

static tp_result fault(tp_door *d) {
    d->state=TP_DOOR_FAULT; d->auth_pending=false; d->observed=false;
    (void)d->io.set_unlocked(d->io.ctx,false); return TP_IO_ERROR;
}
static tp_result lock(tp_door *d) {
    if(d->state!=TP_DOOR_UNLOCKED) return TP_OK;
    if(d->io.set_unlocked(d->io.ctx,false)!=TP_OK) return fault(d);
    d->state=TP_DOOR_LOCKED; return TP_OK;
}
tp_result tp_door_init(tp_door *d,const tp_door_io *io,uint32_t pulse,uint32_t fresh,uint32_t first) {
    if(!d || !io || !io->set_unlocked || !pulse || !fresh ||
       pulse>0x7fffffffu || fresh>0x7fffffffu) return TP_INVALID;
    tp_door_io copy=*io; memset(d,0,sizeof(*d)); d->io=copy; d->state=TP_DOOR_FAULT;
    if(d->io.set_unlocked(d->io.ctx,false)!=TP_OK) return TP_IO_ERROR;
    d->pulse_ms=pulse; d->freshness_ms=fresh; d->next_id=first; d->state=TP_DOOR_LOCKED;
    return TP_OK;
}
tp_result tp_door_tick(tp_door *d,uint32_t now) {
    if(!d || !d->io.set_unlocked) return TP_INVALID;
    if(d->state==TP_DOOR_FAULT) return TP_IO_ERROR;
    if(d->observed && (uint32_t)(now-d->observed_at)>=d->freshness_ms) {
        d->observed=false; d->present=false; d->auth_pending=false;
        return lock(d);
    }
    if(d->state==TP_DOOR_UNLOCKED && (uint32_t)(now-d->unlocked_at)>=d->pulse_ms) return lock(d);
    return TP_OK;
}
tp_result tp_door_observe(tp_door *d,uint32_t now,const uint8_t *id,size_t len,bool *new_request,uint32_t *request) {
    if(!d || !new_request || !request || len>sizeof(d->card_id) || (len && !id) || (!len && id)) return TP_INVALID;
    tp_result r=tp_door_tick(d,now); if(r!=TP_OK) return r;
    *new_request=false;
    bool present=len!=0;
    if(present && (!d->present || d->card_id_len!=len || memcmp(d->card_id,id,len))) {
        d->active_id=d->next_id++; d->auth_pending=true; *request=d->active_id; *new_request=true;
    } else if(!present) d->auth_pending=false;
    if(present) memcpy(d->card_id,id,len);
    d->card_id_len=(uint8_t)len;
    d->present=present; d->observed=true; d->observed_at=now; return TP_OK;
}
tp_result tp_door_authorize(tp_door *d,uint32_t now,uint32_t request,bool approved) {
    if(!d) return TP_INVALID;
    tp_result r=tp_door_tick(d,now); if(r!=TP_OK) return r;
    if(!d->observed || !d->present || !d->auth_pending || request!=d->active_id) return TP_INVALID;
    d->auth_pending=false;
    if(!approved || d->state==TP_DOOR_UNLOCKED) return TP_OK;
    if(d->io.set_unlocked(d->io.ctx,true)!=TP_OK) return fault(d);
    d->unlocked_at=now; d->state=TP_DOOR_UNLOCKED; return TP_OK;
}
tp_result tp_door_reader_fault(tp_door *d) {
    if(!d || !d->io.set_unlocked) return TP_INVALID;
    return fault(d);
}
