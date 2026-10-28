#include "tp_door_service.h"
#include <string.h>

static void report(void *ctx,const tp_pn532_type_a *card) {
    tp_door_service *s=ctx;
    bool new_request=false; uint32_t id=0;
    s->callback_result=tp_door_observe(&s->door,s->now,card ? card->uid : NULL,
                                       card ? card->uid_len : 0,&new_request,&id);
    if(s->callback_result==TP_OK && new_request) {
        s->callback_result=s->request(s->request_ctx,id,card->uid,card->uid_len);
        if(s->callback_result==TP_BUSY) s->callback_result=TP_IO_ERROR;
    }
    if(s->callback_result!=TP_OK) (void)tp_door_reader_fault(&s->door);
}
tp_result tp_door_service_init(tp_door_service *s,const tp_pn532_io *reader,const tp_door_io *output,
                               void *ctx,tp_door_request request,uint32_t timeout,uint32_t interval,
                               uint8_t retries,uint32_t pulse,uint32_t freshness,uint32_t first) {
    if(!s || !reader || !output || !request) return TP_INVALID;
    memset(s,0,sizeof(*s));
    tp_result r=tp_door_init(&s->door,output,pulse,freshness,first);
    if(r!=TP_OK) return r;
    s->request=request; s->request_ctx=ctx;
    r=tp_pn532_scan_init(&s->scan,reader,s,report,timeout,interval,retries);
    if(r!=TP_OK) (void)tp_door_reader_fault(&s->door);
    return r;
}
tp_result tp_door_service_recover(tp_door_service *s) {
    if(!s || !s->request || s->door.state!=TP_DOOR_FAULT) return TP_INVALID;
    /* Initialize candidates without discarding the fault or recovery settings on failure. */
    tp_door door;
    tp_result r=tp_door_init(&door,&s->door.io,s->door.pulse_ms,
                             s->door.freshness_ms,s->door.next_id);
    if(r!=TP_OK) return r;
    tp_pn532_scan scan;
    r=tp_pn532_scan_init(&scan,&s->scan.device.io,s,report,s->scan.timeout_ms,
                         s->scan.interval_ms,s->scan.retries);
    if(r!=TP_OK) return r;
    s->door=door; s->scan=scan; s->callback_result=TP_OK;
    return TP_OK;
}
tp_result tp_door_service_tick(tp_door_service *s,uint32_t now) {
    if(!s || !s->request) return TP_INVALID;
    tp_result r=tp_door_tick(&s->door,now);
    if(r!=TP_OK) return r;
    s->now=now; s->callback_result=TP_OK;
    r=tp_pn532_scan_tick(&s->scan,now);
    if(r!=TP_OK && r!=TP_BUSY) { (void)tp_door_reader_fault(&s->door); return r; }
    return s->callback_result!=TP_OK ? s->callback_result : r;
}
tp_result tp_door_service_authorize(tp_door_service *s,uint32_t now,uint32_t id,bool approved) {
    if(!s || !s->request) return TP_INVALID;
    return tp_door_authorize(&s->door,now,id,approved);
}
