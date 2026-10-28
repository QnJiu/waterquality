#include "tp_pn532.h"
#include <string.h>

tp_result tp_pn532_init(tp_pn532 *p,const tp_pn532_io *io) {
    if(!p || !io || !io->send || !io->receive) return TP_INVALID;
    tp_pn532_io copy=*io; memset(p,0,sizeof(*p)); p->io=copy; return TP_OK;
}
tp_result tp_pn532_start(tp_pn532 *p,uint32_t now,uint32_t timeout,uint8_t command,
                         const uint8_t *parameters,size_t len) {
    if(!p || !p->io.send || !p->io.receive || !timeout || timeout>0x7fffffffu ||
       len>TP_PN532_DATA_MAX || (len && !parameters)) return TP_INVALID;
    if(p->state!=TP_PN532_IDLE) return TP_BUSY;
    uint8_t frame[TP_PN532_FRAME_MAX]; size_t n=len+2;
    frame[0]=0; frame[1]=0; frame[2]=0xff; frame[3]=(uint8_t)n;
    frame[4]=(uint8_t)(0u-frame[3]); frame[5]=0xd4; frame[6]=command;
    uint8_t sum=(uint8_t)(0xd4u+command);
    for(size_t i=0;i<len;i++) { frame[7+i]=parameters[i]; sum=(uint8_t)(sum+parameters[i]); }
    frame[7+len]=(uint8_t)(0u-sum); frame[8+len]=0;
    p->state=TP_PN532_DESYNC;
    tp_result r=p->io.send(p->io.ctx,frame,len+9);
    if(r!=TP_OK) return r;
    p->started_ms=now; p->timeout_ms=timeout; p->command=command;
    p->data_len=0; p->state=TP_PN532_ACK; return TP_OK;
}
static tp_result fail(tp_pn532 *p,tp_result r) { p->state=TP_PN532_DESYNC; return r; }
tp_result tp_pn532_tick(tp_pn532 *p,uint32_t now) {
    static const uint8_t ack[]={0,0,0xff,0,0xff,0};
    if(!p || !p->io.receive) return TP_INVALID;
    if(p->state==TP_PN532_DONE) return TP_OK;
    if(p->state!=TP_PN532_ACK && p->state!=TP_PN532_RESPONSE) return TP_INVALID;
    if((uint32_t)(now-p->started_ms)>=p->timeout_ms) return fail(p,TP_TIMEOUT);
    uint8_t frame[TP_PN532_FRAME_MAX]; size_t n=0;
    tp_result r=p->io.receive(p->io.ctx,frame,sizeof(frame),&n);
    if(r==TP_BUSY) return TP_BUSY;
    if(r!=TP_OK) return fail(p,r);
    if(n>sizeof(frame)) return fail(p,TP_INVALID);
    if(p->state==TP_PN532_ACK) {
        if(n!=sizeof(ack) || memcmp(frame,ack,sizeof(ack))) return fail(p,TP_INVALID);
        p->state=TP_PN532_RESPONSE; return TP_BUSY;
    }
    if(n<9 || frame[0] || frame[1] || frame[2]!=0xff || frame[3]<2 ||
       (uint8_t)(frame[3]+frame[4]) || n!=(size_t)frame[3]+7 || frame[n-1] ||
       frame[5]!=0xd5 || frame[6]!=(uint8_t)(p->command+1u)) return fail(p,TP_INVALID);
    uint8_t sum=0;
    for(size_t i=5;i<n-1;i++) sum=(uint8_t)(sum+frame[i]);
    if(sum) return fail(p,TP_INVALID);
    p->data_len=frame[3]-2u; memcpy(p->data,frame+7,p->data_len);
    p->state=TP_PN532_DONE; return TP_OK;
}
tp_result tp_pn532_take(tp_pn532 *p,uint8_t *data,size_t capacity,size_t *len) {
    if(!p || !len || p->state!=TP_PN532_DONE) return TP_INVALID;
    if(capacity<p->data_len || (p->data_len && !data)) return TP_INVALID;
    if(p->data_len) memcpy(data,p->data,p->data_len);
    *len=p->data_len; p->state=TP_PN532_IDLE; return TP_OK;
}
tp_result tp_pn532_type_a_parse(const uint8_t *data,size_t n,bool *found,tp_pn532_type_a *out) {
    if(!data || !found || !out || !n) return TP_INVALID;
    if(data[0]==0) { if(n!=1) return TP_INVALID; *found=false; return TP_OK; }
    if(data[0]!=1 || n<6 || data[1]!=1 ||
       (data[5]!=4 && data[5]!=7 && data[5]!=10) || n<(size_t)6+data[5]) return TP_INVALID;
    /* Extra bytes contain optional ATS and are not interpreted. */
    tp_pn532_type_a result={0}; result.target=data[1]; result.sens_res[0]=data[2];
    result.sens_res[1]=data[3]; result.sel_res=data[4]; result.uid_len=data[5];
    memcpy(result.uid,data+6,result.uid_len); *out=result; *found=true; return TP_OK;
}
