#include "tp_service.h"
#include <assert.h>
#include <stdio.h>
typedef struct { unsigned valves, pumps, heat_calls, safe; bool interlock; float temperature; } fake;
static tp_result valves(void *ctx, uint32_t mask) { fake *f=ctx; assert(mask==3); f->valves++; return TP_OK; }
static tp_result pump(void *ctx, int32_t steps, uint32_t hz) { fake *f=ctx; assert(steps==100 && hz==500); f->pumps++; return TP_OK; }
static tp_result poll(void *ctx, bool *done) { (void)ctx; *done=true; return TP_OK; }
static tp_result temp(void *ctx, float *v) { *v=((fake *)ctx)->temperature; return TP_OK; }
static tp_result heat(void *ctx, float v) { fake *f=ctx; (void)v; f->heat_calls++; return TP_OK; }
static tp_result adc(void *ctx, uint8_t ch, float *v) { (void)ctx; (void)ch; *v=1; return TP_OK; }
static bool interlock(void *ctx) { return ((fake *)ctx)->interlock; }
static void safe(void *ctx) { ((fake *)ctx)->safe++; }
int main(void) {
    fake f={.interlock=true,.temperature=20};
    tp_hal hal={&f,valves,pump,poll,temp,heat,adc,interlock,safe};
    tp_pid pid={.kp=0.1f,.output_max=1,.integral_max=1}; tp_controller c;
    tp_init(&c,&hal,NULL,&pid,100);
    const tp_step recipe[]={ {.kind=TP_STEP_HEAT,.timeout_ms=1000,.arg.heat={20,1}},
        {.kind=TP_STEP_VALVES,.arg.valves={3}},
        {.kind=TP_STEP_PUMP,.timeout_ms=1000,.arg.pump={100,500}},
        {.kind=TP_STEP_HEATER_OFF}, {.kind=TP_STEP_END} };
    tp_service s; assert(tp_service_init(&s,&c,recipe,5,0)==TP_OK);
    uint16_t command=5; assert(tp_service_write_holding(&s,0,1,&command)==TP_INVALID);
    command=1; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_tick(&s,0,false,0);
    uint16_t waiting; assert(tp_service_read_holding(&s,18,1,&waiting)==TP_OK && waiting==1);
    assert(c.index==1 && f.valves==0 && f.pumps==0 && c.heating);
    tp_service_tick(&s,200,false,0); assert(f.heat_calls>=2 && f.valves==0);
    command=5; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    assert(tp_service_write_holding(&s,0,1,&command)==TP_INVALID);
    tp_service_tick(&s,201,false,0); assert(f.valves==1 && f.pumps==0 && c.awaiting_advance);
    assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_tick(&s,202,false,0); assert(f.pumps==1 && c.awaiting_advance);
    assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_tick(&s,203,false,0); assert(!c.heating && c.awaiting_advance);
    assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_tick(&s,204,false,0); assert(c.state==TP_COMPLETE && f.safe==1);
    command=1; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_tick(&s,205,false,0); assert(c.awaiting_advance);
    f.interlock=false; command=5; assert(tp_service_write_holding(&s,0,1,&command)==TP_IO_ERROR);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_INTERLOCK && f.safe==2);
    f.interlock=true; command=3; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    command=1; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_tick(&s,206,false,0); assert(c.awaiting_advance);
    s.now_ms=30206; command=5;
    assert(tp_service_write_holding(&s,0,1,&command)==TP_TIMEOUT);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_TIMEOUT);
    command=3; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    command=1; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_tick(&s,30207,false,0); assert(c.awaiting_advance);
    unsigned safes=f.safe; command=2;
    assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_STOP && f.safe==safes+1);
    command=5; assert(tp_service_write_holding(&s,0,1,&command)==TP_INVALID);
    puts("controlled stepping checks passed"); return 0;
}
