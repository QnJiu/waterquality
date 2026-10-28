#include "tp_pn532_scan.h"
#include <string.h>

tp_result tp_pn532_scan_init(tp_pn532_scan *s,const tp_pn532_io *io,void *ctx,tp_scan_report report,
                             uint32_t timeout,uint32_t interval,uint8_t retries) {
    if(!s || !io || !io->send || !io->receive || !report || !timeout || !interval ||
       timeout>0x7fffffffu || interval>0x7fffffffu || retries==0xff) return TP_INVALID;
    tp_pn532_io copy=*io; memset(s,0,sizeof(*s));
    tp_result r=tp_pn532_init(&s->device,&copy); if(r!=TP_OK) return r;
    s->report=report; s->report_ctx=ctx; s->timeout_ms=timeout; s->interval_ms=interval;
    s->retries=retries; s->state=TP_SCAN_START; return TP_OK;
}
static tp_result fault(tp_pn532_scan *s,tp_result r) { s->state=TP_SCAN_FAULT; return r; }
static tp_result begin(tp_pn532_scan *s,uint32_t now,uint8_t command,const uint8_t *data,size_t n,tp_scan_state state) {
    tp_result r=tp_pn532_start(&s->device,now,s->timeout_ms,command,data,n);
    if(r!=TP_OK) return fault(s,r);
    s->state=state; return TP_BUSY;
}
tp_result tp_pn532_scan_tick(tp_pn532_scan *s,uint32_t now) {
    static const uint8_t sam[]={1};
    static const uint8_t scan[]={1,0};
    if(!s || !s->report || s->state==TP_SCAN_FAULT) return TP_INVALID;
    if(s->state==TP_SCAN_START) return begin(s,now,0x14,sam,sizeof(sam),TP_SCAN_SAM);
    if(s->state==TP_SCAN_WAIT) {
        if((uint32_t)(now-s->last_scan_ms)<s->interval_ms) return TP_OK;
        return begin(s,now,0x4a,scan,sizeof(scan),TP_SCAN_POLL);
    }
    if(s->state!=TP_SCAN_SAM && s->state!=TP_SCAN_CONFIG && s->state!=TP_SCAN_POLL)
        return fault(s,TP_INVALID);
    tp_result r=tp_pn532_tick(&s->device,now);
    if(r==TP_BUSY) return r;
    if(r!=TP_OK) return fault(s,r);
    uint8_t data[TP_PN532_DATA_MAX]; size_t n=0;
    r=tp_pn532_take(&s->device,data,sizeof(data),&n); if(r!=TP_OK) return fault(s,r);
    if(s->state==TP_SCAN_SAM) {
        if(n) return fault(s,TP_INVALID);
        uint8_t config[]={5,0xff,1,s->retries};
        return begin(s,now,0x32,config,sizeof(config),TP_SCAN_CONFIG);
    }
    if(s->state==TP_SCAN_CONFIG) {
        if(n) return fault(s,TP_INVALID);
        return begin(s,now,0x4a,scan,sizeof(scan),TP_SCAN_POLL);
    }
    bool found=false; tp_pn532_type_a card;
    r=tp_pn532_type_a_parse(data,n,&found,&card); if(r!=TP_OK) return fault(s,r);
    s->last_scan_ms=now; s->state=TP_SCAN_WAIT;
    s->report(s->report_ctx,found ? &card : NULL); return TP_OK;
}
