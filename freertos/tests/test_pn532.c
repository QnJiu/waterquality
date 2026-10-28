#include "tp_pn532.h"
#include <assert.h>
#include <string.h>

typedef struct { uint8_t tx[TP_PN532_FRAME_MAX],rx[TP_PN532_FRAME_MAX]; size_t tx_len,rx_len; tp_result send_result,receive_result; } fake;
static tp_result send(void *ctx,const uint8_t *frame,size_t n) {
    fake *f=ctx; memcpy(f->tx,frame,n); f->tx_len=n; return f->send_result;
}
static tp_result receive(void *ctx,uint8_t *frame,size_t cap,size_t *n) {
    fake *f=ctx; if(f->receive_result!=TP_OK) return f->receive_result;
    assert(f->rx_len<=cap); memcpy(frame,f->rx,f->rx_len); *n=f->rx_len; return TP_OK;
}
static void ack(fake *f) { const uint8_t a[]={0,0,0xff,0,0xff,0}; memcpy(f->rx,a,sizeof(a)); f->rx_len=sizeof(a); }
static void response(fake *f,uint8_t cmd,const uint8_t *data,size_t n) {
    f->rx[0]=0; f->rx[1]=0; f->rx[2]=0xff; f->rx[3]=(uint8_t)(n+2);
    f->rx[4]=(uint8_t)(0u-f->rx[3]); f->rx[5]=0xd5; f->rx[6]=(uint8_t)(cmd+1);
    uint8_t sum=(uint8_t)(0xd5u+cmd+1u);
    for(size_t i=0;i<n;i++) { f->rx[7+i]=data[i]; sum=(uint8_t)(sum+data[i]); }
    f->rx[7+n]=(uint8_t)(0u-sum); f->rx[8+n]=0; f->rx_len=n+9;
}
int main(void) {
    fake f={0}; tp_pn532_io io={&f,send,receive}; tp_pn532 p; size_t n=99;
    assert(tp_pn532_init(&p,&io)==TP_OK);
    uint8_t params[]={1,0};
    assert(tp_pn532_start(&p,0xfffffff0u,50,0x4a,params,sizeof(params))==TP_OK);
    const uint8_t expected[]={0,0,0xff,4,0xfc,0xd4,0x4a,1,0,0xe1,0};
    assert(f.tx_len==sizeof(expected) && !memcmp(f.tx,expected,sizeof(expected)));
    assert(tp_pn532_start(&p,0,50,2,NULL,0)==TP_BUSY);
    f.receive_result=TP_BUSY; assert(tp_pn532_tick(&p,0xfffffff1u)==TP_BUSY);
    f.receive_result=TP_OK; ack(&f); assert(tp_pn532_tick(&p,0xfffffff2u)==TP_BUSY);
    uint8_t target[]={1,1,0x04,0x00,0x08,4,0x11,0x22,0x33,0x44};
    response(&f,0x4a,target,sizeof(target)); assert(tp_pn532_tick(&p,0xfffffff3u)==TP_OK);
    uint8_t out[32]; assert(tp_pn532_take(&p,out,2,&n)==TP_INVALID && n==99);
    assert(tp_pn532_take(&p,out,sizeof(out),&n)==TP_OK && n==sizeof(target) && !memcmp(out,target,n));
    bool found=false; tp_pn532_type_a card={0};
    assert(tp_pn532_type_a_parse(out,n,&found,&card)==TP_OK && found && card.uid_len==4 && card.uid[3]==0x44);
    uint8_t none=0; assert(tp_pn532_type_a_parse(&none,1,&found,&card)==TP_OK && !found);
    target[5]=5; assert(tp_pn532_type_a_parse(target,sizeof(target),&found,&card)==TP_INVALID);
    assert(tp_pn532_start(&p,0xfffffff0u,20,2,NULL,0)==TP_OK);
    f.receive_result=TP_BUSY; assert(tp_pn532_tick(&p,4)==TP_TIMEOUT);
    assert(tp_pn532_start(&p,5,20,2,NULL,0)==TP_BUSY);
    assert(tp_pn532_init(&p,&io)==TP_OK);
    f.receive_result=TP_OK; assert(tp_pn532_start(&p,0,20,2,NULL,0)==TP_OK);
    ack(&f); f.rx[3]=1; assert(tp_pn532_tick(&p,1)==TP_INVALID);
    assert(tp_pn532_init(&p,&io)==TP_OK);
    assert(tp_pn532_start(&p,0,20,2,NULL,0)==TP_OK);
    ack(&f); assert(tp_pn532_tick(&p,1)==TP_BUSY);
    response(&f,2,target,sizeof(target)); f.rx[8]^=1;
    assert(tp_pn532_tick(&p,2)==TP_INVALID);
    assert(tp_pn532_init(&p,&io)==TP_OK);
    f.send_result=TP_IO_ERROR; assert(tp_pn532_start(&p,0,20,2,NULL,0)==TP_IO_ERROR);
    assert(p.state==TP_PN532_DESYNC);
    return 0;
}
