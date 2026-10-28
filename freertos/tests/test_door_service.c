#include "tp_door_service.h"
#include "tp_door_allowlist.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t rx[TP_PN532_FRAME_MAX]; size_t rx_len;
    bool unlocked, reject, lock_fail; unsigned requests; uint32_t id;
} fake;
static tp_result send_frame(void *ctx,const uint8_t *data,size_t len) {
    (void)ctx; (void)data; (void)len; return TP_OK;
}
static tp_result receive_frame(void *ctx,uint8_t *data,size_t cap,size_t *len) {
    fake *f=ctx; if(!f->rx_len) return TP_BUSY;
    assert(f->rx_len<=cap); memcpy(data,f->rx,f->rx_len); *len=f->rx_len; f->rx_len=0; return TP_OK;
}
static tp_result output(void *ctx,bool unlocked) {
    fake *f=ctx; if(f->lock_fail) return TP_IO_ERROR; f->unlocked=unlocked; return TP_OK;
}
static tp_result request(void *ctx,uint32_t id,const uint8_t *uid,size_t len) {
    fake *f=ctx; assert(len==4 && uid[0]==0xab); f->requests++; f->id=id;
    return f->reject ? TP_BUSY : TP_OK;
}
static void ack(fake *f) { const uint8_t b[]={0,0,0xff,0,0xff,0}; memcpy(f->rx,b,sizeof(b)); f->rx_len=sizeof(b); }
static void response(fake *f,uint8_t command,const uint8_t *data,size_t n) {
    uint8_t *b=f->rx; b[0]=0; b[1]=0; b[2]=0xff; b[3]=(uint8_t)(n+2); b[4]=(uint8_t)(0-b[3]);
    b[5]=0xd5; b[6]=(uint8_t)(command+1); uint8_t sum=(uint8_t)(b[5]+b[6]);
    for(size_t i=0;i<n;i++) { b[7+i]=data[i]; sum=(uint8_t)(sum+data[i]); }
    b[7+n]=(uint8_t)(0-sum); b[8+n]=0; f->rx_len=n+9;
}
static void setup(tp_door_service *s,fake *f) {
    tp_pn532_io reader={f,send_frame,receive_frame}; tp_door_io lock={f,output};
    assert(tp_door_service_init(s,&reader,&lock,f,request,100,50,2,40,200,7)==TP_OK);
    assert(tp_door_service_tick(s,0)==TP_BUSY);
    ack(f); assert(tp_door_service_tick(s,1)==TP_BUSY);
    response(f,0x14,NULL,0); assert(tp_door_service_tick(s,2)==TP_BUSY);
    ack(f); assert(tp_door_service_tick(s,3)==TP_BUSY);
    response(f,0x32,NULL,0); assert(tp_door_service_tick(s,4)==TP_BUSY);
    ack(f); assert(tp_door_service_tick(s,5)==TP_BUSY);
}
static void card(fake *f) {
    const uint8_t data[]={1,1,4,0,8,4,0xab,0xcd,0xef,0x12}; response(f,0x4a,data,sizeof(data));
}
typedef struct { uint32_t id; bool approved; unsigned count; tp_result result; } decision;
static tp_result submit(void *ctx,uint32_t id,bool approved) {
    decision *d=ctx; d->id=id; d->approved=approved; d->count++; return d->result;
}
static void test_allowlist(void) {
    const tp_door_card cards[]={{{0xab,0xcd,0xef,0x12},4}};
    tp_door_allowlist policy; decision d={0}; fake f={0}; tp_door_service s;
    assert(tp_door_allowlist_init(&policy,cards,1,&d,submit)==TP_OK);
    setup(&s,&f); s.request=tp_door_allowlist_request; s.request_ctx=&policy;
    card(&f); assert(tp_door_service_tick(&s,6)==TP_OK);
    assert(d.count==1 && d.id==7 && d.approved && !f.unlocked);
    assert(tp_door_service_authorize(&s,7,d.id,d.approved)==TP_OK && f.unlocked);
    assert(tp_door_service_tick(&s,47)==TP_OK && !f.unlocked);
    /* A different card is denied, and cannot energize the lock. */
    bool created=false; uint32_t id=0; const uint8_t other[]={1,2,3,4};
    assert(tp_door_observe(&s.door,48,other,4,&created,&id)==TP_OK && created);
    assert(tp_door_allowlist_request(&policy,id,other,4)==TP_OK && !d.approved);
    assert(tp_door_service_authorize(&s,49,id,d.approved)==TP_OK && !f.unlocked);
    assert(tp_door_allowlist_init(&policy,NULL,0,&d,submit)==TP_OK);
    assert(tp_door_allowlist_request(&policy,9,cards[0].uid,4)==TP_OK && !d.approved);
    tp_door_card invalid={{0},5};
    assert(tp_door_allowlist_init(&policy,&invalid,1,&d,submit)==TP_INVALID);
    assert(tp_door_allowlist_request(&policy,9,cards[0].uid,3)==TP_INVALID);
    d.result=TP_BUSY;
    assert(tp_door_allowlist_request(&policy,9,cards[0].uid,4)==TP_BUSY);
}
int main(void) {
    fake f={0}; tp_door_service s; setup(&s,&f);
    card(&f); assert(tp_door_service_tick(&s,6)==TP_OK && f.requests==1 && f.id==7);
    assert(tp_door_service_authorize(&s,7,f.id,true)==TP_OK && f.unlocked);
    assert(tp_door_service_tick(&s,47)==TP_OK && !f.unlocked);
    assert(tp_door_service_tick(&s,56)==TP_BUSY);
    ack(&f); assert(tp_door_service_tick(&s,57)==TP_BUSY);
    card(&f); assert(tp_door_service_tick(&s,58)==TP_OK && f.requests==1);
    assert(tp_door_service_tick(&s,108)==TP_BUSY);
    assert(tp_door_service_tick(&s,208)==TP_TIMEOUT && s.door.state==TP_DOOR_FAULT && !f.unlocked);
    f.lock_fail=true;
    assert(tp_door_service_recover(&s)==TP_IO_ERROR && s.door.state==TP_DOOR_FAULT);
    f.lock_fail=false;
    assert(tp_door_service_recover(&s)==TP_OK && s.door.state==TP_DOOR_LOCKED);
    assert(tp_door_service_recover(&s)==TP_INVALID);
    assert(tp_door_service_authorize(&s,209,7,true)==TP_INVALID && !f.unlocked);
    assert(tp_door_service_tick(&s,210)==TP_BUSY);
    ack(&f); assert(tp_door_service_tick(&s,211)==TP_BUSY);
    response(&f,0x14,NULL,0); assert(tp_door_service_tick(&s,212)==TP_BUSY);
    ack(&f); assert(tp_door_service_tick(&s,213)==TP_BUSY);
    response(&f,0x32,NULL,0); assert(tp_door_service_tick(&s,214)==TP_BUSY);
    ack(&f); assert(tp_door_service_tick(&s,215)==TP_BUSY);
    card(&f); assert(tp_door_service_tick(&s,216)==TP_OK && f.id==8);
    assert(tp_door_service_authorize(&s,217,7,true)==TP_INVALID && !f.unlocked);
    assert(tp_door_service_authorize(&s,217,8,true)==TP_OK && f.unlocked);
    fake g={0}; tp_door_service t; setup(&t,&g); g.reject=true; card(&g);
    assert(tp_door_service_tick(&t,6)==TP_IO_ERROR && t.door.state==TP_DOOR_FAULT && !g.unlocked);
    assert(tp_door_service_authorize(&t,7,g.id,true)==TP_IO_ERROR);
    return 0;
}
