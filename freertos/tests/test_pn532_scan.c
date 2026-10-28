#include "tp_pn532_scan.h"
#include <assert.h>
#include <string.h>
typedef struct {
    uint8_t tx[TP_PN532_FRAME_MAX],rx[TP_PN532_FRAME_MAX];
    size_t tx_len,rx_len; unsigned reports; bool found; uint8_t uid;
} fake;
static tp_result send(void *p,const uint8_t *b,size_t n) {
    fake *f=p; memcpy(f->tx,b,n); f->tx_len=n; return TP_OK;
}
static tp_result receive(void *p,uint8_t *b,size_t cap,size_t *n) {
    fake *f=p; if(!f->rx_len) return TP_BUSY; assert(f->rx_len<=cap);
    memcpy(b,f->rx,f->rx_len); *n=f->rx_len; f->rx_len=0; return TP_OK;
}
static void report(void *p,const tp_pn532_type_a *card) {
    fake *f=p; f->reports++; f->found=card!=NULL; f->uid=card ? card->uid[0] : 0;
}
static void ack(fake *f) { const uint8_t b[]={0,0,0xff,0,0xff,0}; memcpy(f->rx,b,sizeof(b)); f->rx_len=sizeof(b); }
static void response(fake *f,uint8_t command,const uint8_t *data,size_t n) {
    uint8_t *b=f->rx; b[0]=0; b[1]=0; b[2]=0xff; b[3]=(uint8_t)(n+2); b[4]=(uint8_t)(0-b[3]);
    b[5]=0xd5; b[6]=(uint8_t)(command+1); uint8_t sum=(uint8_t)(b[5]+b[6]);
    for(size_t i=0;i<n;i++) { b[7+i]=data[i]; sum=(uint8_t)(sum+data[i]); }
    b[7+n]=(uint8_t)(0-sum); b[8+n]=0; f->rx_len=n+9;
}
int main(void) {
    fake f={0}; tp_pn532_io io={&f,send,receive}; tp_pn532_scan s;
    assert(tp_pn532_scan_init(&s,&io,&f,report,100,50,0xff)==TP_INVALID);
    assert(tp_pn532_scan_init(&s,&io,&f,report,100,50,2)==TP_OK);
    assert(tp_pn532_scan_tick(&s,0)==TP_BUSY && f.tx[6]==0x14 && f.tx[7]==1);
    ack(&f); assert(tp_pn532_scan_tick(&s,1)==TP_BUSY);
    response(&f,0x14,NULL,0); assert(tp_pn532_scan_tick(&s,2)==TP_BUSY);
    assert(f.tx[6]==0x32 && f.tx[7]==5 && f.tx[8]==0xff && f.tx[9]==1 && f.tx[10]==2);
    ack(&f); assert(tp_pn532_scan_tick(&s,3)==TP_BUSY);
    response(&f,0x32,NULL,0); assert(tp_pn532_scan_tick(&s,4)==TP_BUSY);
    assert(f.tx[6]==0x4a && f.tx[7]==1 && f.tx[8]==0);
    ack(&f); assert(tp_pn532_scan_tick(&s,5)==TP_BUSY);
    const uint8_t card[]={1,1,4,0,8,4,0xab,0xcd,0xef,0x12};
    response(&f,0x4a,card,sizeof(card)); assert(tp_pn532_scan_tick(&s,6)==TP_OK);
    assert(f.reports==1 && f.found && f.uid==0xab);
    assert(tp_pn532_scan_tick(&s,55)==TP_OK && s.state==TP_SCAN_WAIT);
    assert(tp_pn532_scan_tick(&s,56)==TP_BUSY && s.state==TP_SCAN_POLL);
    ack(&f); assert(tp_pn532_scan_tick(&s,57)==TP_BUSY);
    const uint8_t none[]={0}; response(&f,0x4a,none,sizeof(none));
    assert(tp_pn532_scan_tick(&s,58)==TP_OK && f.reports==2 && !f.found);
    assert(tp_pn532_scan_tick(&s,108)==TP_BUSY);
    assert(tp_pn532_scan_tick(&s,208)==TP_TIMEOUT && s.state==TP_SCAN_FAULT);
    assert(tp_pn532_scan_tick(&s,209)==TP_INVALID && f.reports==2);
    return 0;
}
