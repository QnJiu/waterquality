#include "tp_subordinate_poll.h"
#include "tp_service.h"
#include <assert.h>
#include <string.h>

typedef struct {
    tp_sub_poll poll;
    uint32_t now;
    uint8_t request[TP_SUB_REQUEST_SIZE];
    unsigned sent, safe;
    bool send_fail, pump_running;
} fixture;
static tp_result send_frame(void *ctx,const uint8_t *frame,size_t len) {
    fixture *f=ctx; assert(len==sizeof(f->request));
    f->sent++; memcpy(f->request,frame,len);
    return f->send_fail ? TP_IO_ERROR : TP_OK;
}
static void reply(fixture *f,uint8_t faults) {
    tp_sub_status status={42,faults}; uint8_t frame[TP_SUB_RESPONSE_SIZE];
    assert(tp_sub_reply(f->request,sizeof(f->request),&status,frame)==TP_OK);
    assert(tp_sub_poll_receive(&f->poll,f->now,frame,sizeof(frame))==TP_OK);
}
static bool interlock(void *ctx) { fixture *f=ctx; return tp_sub_poll_healthy(&f->poll,f->now); }
static uint16_t resources(void *ctx) { fixture *f=ctx; return tp_sub_poll_resource_faults(&f->poll,f->now); }
static tp_result temp(void *ctx,float *value) { (void)ctx; *value=20; return TP_OK; }
static tp_result check(void *ctx,uint16_t *faults) {
    tp_preflight_io io={ctx,interlock,temp,resources,80}; return tp_preflight_check(&io,faults);
}
static tp_result valves(void *ctx,uint32_t mask) { (void)ctx; (void)mask; return TP_OK; }
static tp_result pump(void *ctx,int32_t steps,uint32_t hz) {
    fixture *f=ctx; assert(steps==10 && hz==100); f->pump_running=true; return TP_OK;
}
static tp_result pump_poll(void *ctx,bool *done) { (void)ctx; *done=false; return TP_OK; }
static tp_result heater(void *ctx,float value) { (void)ctx; (void)value; return TP_OK; }
static tp_result adc(void *ctx,uint8_t channel,float *value) { (void)ctx; (void)channel; *value=1; return TP_OK; }
static void safe(void *ctx) { fixture *f=ctx; f->safe++; f->pump_running=false; }

int main(void) {
    fixture f={0};
    assert(tp_sub_poll_init(&f.poll,&f,send_frame,19,20,100,1)==TP_INVALID);
    assert(tp_sub_poll_init(&f.poll,&f,send_frame,50,20,100,1)==TP_OK);
    tp_hal hal={&f,valves,pump,pump_poll,temp,heater,adc,interlock,safe};
    tp_controller controller; tp_init(&controller,&hal,NULL,NULL,80);
    const tp_step recipe[]={ {.kind=TP_STEP_PUMP,.timeout_ms=1000,.arg.pump={10,100}}, {.kind=TP_STEP_END} };
    tp_service service; uint16_t start=1;
    assert(tp_service_init(&service,&controller,recipe,2,0)==TP_OK);
    assert(tp_service_attach_preflight(&service,&f,check)==TP_OK);
    assert(tp_service_write_holding(&service,0,1,&start)==TP_IO_ERROR);
    assert(service.preflight_faults & TP_PREFLIGHT_RESOURCE_UNKNOWN);
    assert(tp_sub_poll_tick(&f.poll,0)==TP_OK && f.sent==1);
    f.now=1; reply(&f,0);
    assert(tp_sub_poll_healthy(&f.poll,f.now));
    assert(tp_service_write_holding(&service,0,1,&start)==TP_OK);
    tp_service_tick(&service,f.now,false,0); assert(f.pump_running);
    f.now=50; assert(tp_sub_poll_tick(&f.poll,f.now)==TP_OK && f.sent==2);
    f.now=70; assert(tp_sub_poll_tick(&f.poll,f.now)==TP_OK && f.sent==2);
    f.now=100; assert(tp_sub_poll_tick(&f.poll,f.now)==TP_OK && f.sent==3);
    f.now=101; tp_service_tick(&service,f.now,false,0);
    assert(controller.state==TP_FAILED && controller.fault==TP_FAULT_INTERLOCK && !f.pump_running);
    assert(resources(&f)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    f.now=102; reply(&f,TP_SUB_LEAK_FAULT);
    assert(resources(&f)==TP_PREFLIGHT_LEAK && !interlock(&f));
    uint16_t faults=0; assert(check(&f,&faults)==TP_IO_ERROR && (faults & TP_PREFLIGHT_LEAK));
    f.now=150; assert(tp_sub_poll_tick(&f.poll,f.now)==TP_OK); f.now=151; reply(&f,0);
    assert(interlock(&f));
    f.now=200; f.send_fail=true;
    assert(tp_sub_poll_tick(&f.poll,f.now)==TP_IO_ERROR && !interlock(&f));
    assert(resources(&f)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    assert(tp_sub_poll_tick(&f.poll,201)==TP_OK && f.sent==5);

    /* A fresh leak report while pumping must trip the running interlock. */
    memset(&f,0,sizeof(f));
    assert(tp_sub_poll_init(&f.poll,&f,send_frame,50,20,100,10)==TP_OK);
    tp_init(&controller,&hal,NULL,NULL,80);
    assert(tp_service_init(&service,&controller,recipe,2,0)==TP_OK);
    assert(tp_service_attach_preflight(&service,&f,check)==TP_OK);
    assert(tp_sub_poll_tick(&f.poll,0)==TP_OK); f.now=1; reply(&f,0);
    assert(tp_service_write_holding(&service,0,1,&start)==TP_OK);
    tp_service_tick(&service,1,false,0); assert(f.pump_running);
    f.now=50; assert(tp_sub_poll_tick(&f.poll,f.now)==TP_OK);
    f.now=51; reply(&f,TP_SUB_LEAK_FAULT);
    tp_service_tick(&service,f.now,false,0);
    assert(controller.state==TP_FAILED && controller.fault==TP_FAULT_INTERLOCK && !f.pump_running);

    memset(&f,0,sizeof(f));
    assert(tp_sub_poll_init(&f.poll,&f,send_frame,50,20,100,4)==TP_OK);
    f.now=0xfffffff0u; assert(tp_sub_poll_tick(&f.poll,f.now)==TP_OK);
    f.now=0xfffffff1u; reply(&f,0);
    f.now=34; assert(tp_sub_poll_tick(&f.poll,f.now)==TP_OK && f.sent==2);
    assert(tp_sub_poll_healthy(&f.poll,34));
    assert(!tp_sub_poll_healthy(&f.poll,85));
    return 0;
}
