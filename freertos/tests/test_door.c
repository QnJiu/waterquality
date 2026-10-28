#include "tp_door.h"
#include <assert.h>
typedef struct { bool unlocked,fail_open,fail_close; unsigned opens,closes; } fake;
static tp_result output(void *p,bool open) {
    fake *f=p;
    if(open) { f->opens++; if(f->fail_open) return TP_IO_ERROR; }
    else { f->closes++; if(f->fail_close) return TP_IO_ERROR; }
    f->unlocked=open; return TP_OK;
}
int main(void) {
    fake f={0}; tp_door_io io={&f,output}; tp_door d; bool request; uint32_t id=999,old;
    const uint8_t a[]={1,2,3,4}, b[]={4,3,2,1};
    assert(tp_door_init(&d,&io,50,100,0xffffffffu)==TP_OK && f.closes==1);
    assert(tp_door_observe(&d,0xfffffff0u,a,sizeof(a),&request,&id)==TP_OK && request && id==0xffffffffu);
    old=id; assert(tp_door_observe(&d,0xfffffff1u,b,sizeof(b),&request,&id)==TP_OK && request && id==0);
    assert(tp_door_authorize(&d,0xfffffff2u,old,true)==TP_INVALID && !f.unlocked);
    assert(tp_door_authorize(&d,0xfffffff2u,id,true)==TP_OK && f.unlocked);
    assert(tp_door_authorize(&d,0xfffffff3u,id,true)==TP_INVALID);
    assert(tp_door_observe(&d,0xfffffff4u,b,sizeof(b),&request,&id)==TP_OK && !request);
    assert(tp_door_tick(&d,35)==TP_OK && f.unlocked && f.opens==1);
    assert(tp_door_tick(&d,36)==TP_OK && !f.unlocked);
    assert(tp_door_observe(&d,36,b,sizeof(b),&request,&id)==TP_OK && !request);
    assert(tp_door_observe(&d,37,NULL,0,&request,&id)==TP_OK && !request);
    assert(tp_door_observe(&d,38,a,sizeof(a),&request,&id)==TP_OK && request);
    assert(tp_door_authorize(&d,39,id,false)==TP_OK && !f.unlocked);
    assert(tp_door_authorize(&d,40,id,true)==TP_INVALID);
    assert(tp_door_observe(&d,41,NULL,0,&request,&id)==TP_OK);
    assert(tp_door_observe(&d,42,a,sizeof(a),&request,&id)==TP_OK && request);
    assert(tp_door_authorize(&d,142,id,true)==TP_INVALID && !f.unlocked);
    assert(tp_door_observe(&d,143,a,sizeof(a),&request,&id)==TP_OK && request);
    assert(tp_door_authorize(&d,144,id,true)==TP_OK && f.unlocked);
    assert(tp_door_reader_fault(&d)==TP_IO_ERROR && d.state==TP_DOOR_FAULT && !f.unlocked);
    assert(tp_door_observe(&d,145,a,sizeof(a),&request,&id)==TP_IO_ERROR);
    assert(tp_door_init(&d,&io,50,100,20)==TP_OK);
    assert(tp_door_observe(&d,200,a,sizeof(a),&request,&id)==TP_OK && request);
    f.fail_open=true; assert(tp_door_authorize(&d,201,id,true)==TP_IO_ERROR && d.state==TP_DOOR_FAULT && !f.unlocked);
    f.fail_open=false; assert(tp_door_init(&d,&io,50,100,30)==TP_OK);
    assert(tp_door_observe(&d,300,a,sizeof(a),&request,&id)==TP_OK);
    assert(tp_door_authorize(&d,301,id,true)==TP_OK); f.fail_close=true;
    assert(tp_door_tick(&d,351)==TP_IO_ERROR && d.state==TP_DOOR_FAULT);
    assert(f.unlocked); /* An unsuccessful output command cannot prove a physical lock. */
    return 0;
}
