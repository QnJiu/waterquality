#include "tp_modbus.h"
#include "tp_service.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
typedef struct { unsigned valve_calls,pump_calls,safe_calls; bool interlock, adc_bad; } fake;
static tp_result valve(void *p,uint32_t mask) { fake *f=p; assert(mask==1); f->valve_calls++; return TP_OK; }
static tp_result pump(void *p,int32_t steps,uint32_t hz) { fake *f=p; assert(steps==10 && hz==100); f->pump_calls++; return TP_OK; }
static tp_result poll(void *p,bool *done) { (void)p; *done=true; return TP_OK; }
static tp_result temp(void *p,float *c) { (void)p; *c=20; return TP_OK; }
static tp_result heater(void *p,float f) { (void)p; (void)f; return TP_OK; }
static tp_result adc(void *p,uint8_t ch,float *v) { const float values[]={1.0f,0.5f,0.1f}; if (ch>=3) return TP_INVALID; *v=((fake *)p)->adc_bad ? NAN : values[ch]; return TP_OK; }
static bool interlock(void *p) { return ((fake *)p)->interlock; }
static void safe(void *p) { ((fake *)p)->safe_calls++; }
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffff; while(n--) { c^=*p++; for(unsigned i=0;i<8;i++) c=(uint16_t)((c>>1)^((c&1)?0xa001:0)); } return c;
}
static void exchange(tp_modbus *m,uint8_t fn,uint16_t address,uint16_t value,uint8_t *out) {
    uint8_t q[8]={1,fn,(uint8_t)(address>>8),(uint8_t)address,(uint8_t)(value>>8),(uint8_t)value};
    uint16_t c=crc(q,6); q[6]=(uint8_t)c; q[7]=(uint8_t)(c>>8); size_t n=0;
    assert(tp_modbus_frame(m,q,sizeof(q),out,32,&n)==TP_OK && n>=5 && out[1]==fn);
    assert(crc(out,n-2)==(uint16_t)(out[n-2]|((uint16_t)out[n-1]<<8)));
}
static float response_float(const uint8_t *p) {
    uint32_t bits=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    float value; memcpy(&value,&bits,sizeof(value)); return value;
}
/* Exercise the real frames, controller sampling and published concentration together. */
static void measurement_snapshot(void) {
    fake f={.interlock=true}; tp_hal hal={&f,valve,pump,poll,temp,heater,adc,interlock,safe};
    tp_calibration cal={1,0,0,0.01f,10}; tp_controller c; tp_init(&c,&hal,&cal,NULL,100);
    const tp_step recipe[]={
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_BLANK,0,1}},
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_REACTED,1,1}},
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_TURBIDITY,2,1}},
        {.kind=TP_STEP_CALCULATE}, {.kind=TP_STEP_END}};
    tp_service s; assert(tp_service_init(&s,&c,recipe,5,0)==TP_OK);
    tp_modbus m={&s,1,tp_service_read_holding,tp_service_write_holding}; uint8_t response[32];
    exchange(&m,3,132,7,response); assert(response[4]==0);
    exchange(&m,6,0,1,response);
    for (unsigned i=0;i<3;i++) {
        if (i) exchange(&m,6,0,5,response);
        tp_service_tick(&s,i*100,false,0);
        exchange(&m,3,132,7,response);
        assert(response[4]==((1u<<(i+1))-1u));
        assert(response_float(&response[5+4*i])==(i==0 ? 1.0f : i==1 ? 0.5f : 0.1f));
    }
    exchange(&m,6,0,5,response); tp_service_tick(&s,300,false,0);
    exchange(&m,6,0,5,response); tp_service_tick(&s,400,false,0);
    exchange(&m,3,10,7,response);
    assert(response[4]==TP_COMPLETE && response[16]==1 && fabsf(response_float(&response[11])-0.30103f)<0.0001f);
    exchange(&m,6,0,1,response);
    exchange(&m,3,132,7,response); assert(response[4]==0);
    tp_service_tick(&s,500,false,0);
    assert(c.valid_readings==1); /* The first new sample completed. */
    exchange(&m,6,0,2,response);
    assert(c.valid_readings==0);
    exchange(&m,3,132,7,response); assert(response[4]==0);
    /* After a successful measurement and stop, a bad ADC sample must trip
     * the controller and cannot publish either the old result or readings. */
    exchange(&m,6,0,3,response);
    f.adc_bad=true; unsigned safe_before=f.safe_calls;
    exchange(&m,6,0,1,response); tp_service_tick(&s,600,false,0);
    exchange(&m,3,10,7,response);
    assert(response[4]==TP_FAILED && response[6]==TP_FAULT_SENSOR && response[16]==0);
    assert(f.safe_calls==safe_before+1);
    exchange(&m,3,132,7,response); assert(response[4]==0);
    /* A later interlock fault also invalidates already collected samples. */
    exchange(&m,6,0,3,response);
    f.adc_bad=false; exchange(&m,6,0,1,response); tp_service_tick(&s,700,false,0);
    assert(c.valid_readings==1);
    f.interlock=false; tp_service_tick(&s,701,false,0);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_INTERLOCK && c.valid_readings==0);
    exchange(&m,3,132,7,response); assert(response[4]==0);
}

int main(void) {
    fake f={.interlock=true}; tp_hal hal={&f,valve,pump,poll,temp,heater,adc,interlock,safe};
    tp_controller c; tp_init(&c,&hal,NULL,NULL,100);
    const tp_step recipe[]={ {.kind=TP_STEP_VALVES,.arg.valves={1}},
        {.kind=TP_STEP_PUMP,.timeout_ms=1000,.arg.pump={10,100}}, {.kind=TP_STEP_END} };
    tp_service s; assert(tp_service_init(&s,&c,recipe,3,0)==TP_OK);
    tp_modbus m={&s,1,tp_service_read_holding,tp_service_write_holding}; uint8_t response[32];
    exchange(&m,6,0,1,response); tp_service_tick(&s,0,false,0);
    assert(f.valve_calls==1 && c.awaiting_advance);
    exchange(&m,6,0,5,response); tp_service_tick(&s,1,false,0);
    assert(f.pump_calls==1 && c.awaiting_advance);
    exchange(&m,6,0,5,response); tp_service_tick(&s,2,false,0);
    exchange(&m,3,10,2,response);
    assert(response[3]==0 && response[4]==TP_COMPLETE && response[5]==0 && response[6]==TP_FAULT_NONE);
    assert(f.safe_calls==1);
    exchange(&m,6,0,1,response); tp_service_tick(&s,3,false,0);
    f.interlock=false; tp_service_tick(&s,4,false,0);
    exchange(&m,3,10,2,response);
    assert(response[4]==TP_FAILED && response[6]==TP_FAULT_INTERLOCK && f.safe_calls==2);
    measurement_snapshot();
    puts("protocol-to-controller checks passed");
    return 0;
}
