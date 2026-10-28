#include "tp_adc_reader.h"
#include "tp_service.h"
#include "tp_modbus.h"
#include "tp_event_transition.h"
#include <assert.h>
#include <math.h>
#include <string.h>

typedef struct {
    tp_ads1220 adc;
    tp_adc_reader reader;
    uint8_t regs[4];
    uint32_t now;
    bool ready, fail_write;
    unsigned safe_calls;
} fixture;
static tp_result transfer(void *p,const uint8_t *tx,uint8_t *rx,size_t n) {
    fixture *f=p; memset(rx,0,n);
    if (n==5 && tx[0]==0x43) {
        if (f->fail_write) return TP_IO_ERROR;
        memcpy(f->regs,tx+1,4);
    } else if (n==5 && tx[0]==0x23) memcpy(rx+1,f->regs,4);
    else if (n==2 && tx[0]==0x40) f->regs[0]=tx[1];
    else if (n==4 && tx[0]==0x10) { rx[3]=42; }
    else if (n!=1 || (tx[0]!=0x02 && tx[0]!=0x08)) return TP_IO_ERROR;
    return TP_OK;
}
static tp_result ready(void *p,bool *r) { *r=((fixture *)p)->ready; return TP_OK; }
static tp_result valves(void *p,uint32_t mask) { (void)p; (void)mask; return TP_OK; }
static tp_result pump(void *p,int32_t steps,uint32_t hz) { (void)p;(void)steps;(void)hz;return TP_OK; }
static tp_result poll(void *p,bool *done) { (void)p; *done=true; return TP_OK; }
static tp_result temp(void *p,float *v) { (void)p; *v=20; return TP_OK; }
static tp_result heat(void *p,float v) { (void)p;(void)v;return TP_OK; }
static tp_result read_adc(void *p,uint8_t ch,float *v) {
    fixture *f=p; return tp_adc_reader_read(&f->reader,ch,f->now,v);
}
static bool interlock(void *p) { (void)p; return true; }
static void safe(void *p) {
    fixture *f=p; f->safe_calls++; (void)tp_adc_reader_cancel(&f->reader);
}
static tp_result preflight(void *p,uint16_t *faults) {
    *faults=tp_adc_reader_ready(&((fixture *)p)->reader) ? 0 : TP_PREFLIGHT_ADC;
    return *faults ? TP_IO_ERROR : TP_OK;
}
static tp_result recover(void *p) {
    fixture *f=p;
    if (f->reader.active || f->adc.pending) return TP_BUSY;
    return tp_ads1220_recover(&f->adc);
}
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffff; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0)); } return c;
}
static uint8_t command(tp_modbus *m,uint16_t cmd) {
    uint8_t q[8]={1,6,0,0,(uint8_t)(cmd>>8),(uint8_t)cmd},reply[16]; size_t n=0;
    uint16_t c=crc(q,6); q[6]=(uint8_t)c; q[7]=(uint8_t)(c>>8);
    assert(tp_modbus_frame(m,q,sizeof(q),reply,sizeof(reply),&n)==TP_OK);
    assert(n>=5 && crc(reply,n-2)==(uint16_t)(reply[n-2]|((uint16_t)reply[n-1]<<8)));
    return reply[1];
}
int main(void) {
    fixture f={0}; tp_ads1220_io io={&f,transfer,ready};
    const uint8_t regs[4]={1,0,0,0}; const tp_adc_channel channel={8,0.001f};
    assert(tp_ads1220_init(&f.adc,&io,regs,50)==TP_OK);
    assert(tp_adc_reader_init(&f.reader,&f.adc,&channel,1)==TP_OK);
    tp_hal hal={&f,valves,pump,poll,temp,heat,read_adc,interlock,safe};
    tp_controller c; tp_init(&c,&hal,NULL,NULL,100);
    const tp_step recipe[]={ {.kind=TP_STEP_READ,.timeout_ms=1000,
        .arg.read={.slot=TP_READ_BLANK,.channel=0,.samples=1}}, {.kind=TP_STEP_END} };
    tp_service s; assert(tp_service_init(&s,&c,recipe,2,0)==TP_OK);
    assert(tp_service_attach_preflight(&s,&f,preflight)==TP_OK);
    assert(tp_service_attach_adc_recovery(&s,&f,recover)==TP_OK);
    tp_modbus m={&s,1,tp_service_read_holding,tp_service_write_holding};
    assert(command(&m,1)==6); tp_service_tick(&s,0,false,0);
    assert(f.adc.pending && command(&m,8)==(6|0x80));
    assert(command(&m,10)==(6|0x80) && c.state==TP_RUNNING);
    f.now=50; tp_service_tick(&s,50,false,0);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_SENSOR && !f.adc.configured);
    tp_operation_snapshot before=tp_operation_capture(&s); tp_event event;
    assert(command(&m,10)==(6|0x80) && s.preflight_faults==TP_PREFLIGHT_ADC);
    assert(tp_operation_preflight_event(before,&s,&event) && event.type==TP_EVENT_PREFLIGHT && event.detail==TP_PREFLIGHT_ADC);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_SENSOR);
    assert(command(&m,3)==(6|0x80) && c.state==TP_FAILED && c.fault==TP_FAULT_SENSOR);
    assert(command(&m,1)==(6|0x80) && s.preflight_faults==TP_PREFLIGHT_ADC);
    f.fail_write=true;
    assert(command(&m,8)==(6|0x80) && !tp_adc_reader_ready(&f.reader));
    f.fail_write=false;
    assert(command(&m,8)==6 && tp_adc_reader_ready(&f.reader));
    before=tp_operation_capture(&s);
    assert(command(&m,10)==6 && s.preflight_faults==0 && c.state==TP_FAILED);
    assert(tp_operation_preflight_event(before,&s,&event) && event.type==TP_EVENT_PREFLIGHT && event.detail==0);
    before=tp_operation_capture(&s);
    assert(command(&m,10)==6 && !tp_operation_preflight_event(before,&s,&event));
    assert(command(&m,3)==6 && c.state==TP_IDLE);
    f.ready=true; f.now=60; tp_service_tick(&s,60,false,0);
    assert(command(&m,1)==6); tp_service_tick(&s,60,false,0);
    f.now=61; tp_service_tick(&s,61,false,0);
    assert(c.awaiting_advance && fabsf(c.readings[TP_READ_BLANK]-0.042f)<0.00001f);
    assert(command(&m,5)==6); tp_service_tick(&s,62,false,0);
    assert(c.state==TP_COMPLETE && f.safe_calls>=4);
    return 0;
}
