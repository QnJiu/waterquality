#include "tp_service.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
typedef struct { unsigned valves, pumps, heat_calls, safe; bool interlock; float temperature, output; } fake;
static tp_result valves(void *ctx, uint32_t mask) { fake *f=ctx; assert(mask==3); f->valves++; return TP_OK; }
static tp_result pump(void *ctx, int32_t steps, uint32_t hz) { fake *f=ctx; assert(steps==100 && hz==500); f->pumps++; return TP_OK; }
static tp_result poll(void *ctx, bool *done) { (void)ctx; *done=true; return TP_OK; }
static tp_result temp(void *ctx, float *v) { *v=((fake *)ctx)->temperature; return TP_OK; }
static tp_result heat(void *ctx, float v) { fake *f=ctx; f->output=v; f->heat_calls++; return TP_OK; }
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
    uint16_t snapshot[3];
    assert(tp_service_read_holding(&s,128,3,snapshot)==TP_OK && snapshot[0]==0);
    uint16_t command=5; assert(tp_service_write_holding(&s,0,1,&command)==TP_INVALID);
    command=1; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    tp_service_tick(&s,0,false,0);
    assert(tp_service_read_holding(&s,128,3,snapshot)==TP_OK && snapshot[0]==1 && snapshot[1]==0x41a0 && snapshot[2]==0);
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
    assert(tp_service_read_holding(&s,128,3,snapshot)==TP_OK && snapshot[0]==0 && snapshot[1]==0 && snapshot[2]==0);
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
    /* A long non-heating step must not be integrated when heating begins. */
    fake g={.interlock=true,.temperature=20};
    tp_hal hal2={&g,valves,pump,poll,temp,heat,adc,interlock,safe};
    tp_pid pid2={.ki=0.01f,.output_max=1,.integral_max=100}; tp_controller c2;
    tp_init(&c2,&hal2,NULL,&pid2,100);
    const tp_step later[]={
        {.kind=TP_STEP_WAIT,.timeout_ms=30000,.arg.wait={30000}},
        {.kind=TP_STEP_HEAT,.timeout_ms=1000,.arg.heat={30,0.1f}},
        {.kind=TP_STEP_HEATER_OFF}, {.kind=TP_STEP_WAIT,.timeout_ms=30000,.arg.wait={30000}},
        {.kind=TP_STEP_HEAT,.timeout_ms=1000,.arg.heat={30,0.1f}}, {.kind=TP_STEP_END}};
    assert(tp_start(&c2,later,6,0)==TP_OK);
    tp_tick(&c2,0); tp_tick(&c2,30000); tp_tick(&c2,30001);
    assert(g.output>0.019f && g.output<0.021f);
    g.temperature=30; tp_tick(&c2,30002); tp_tick(&c2,30003);
    assert(!c2.heating && g.output==0);
    tp_tick(&c2,30004); tp_tick(&c2,60004);
    g.temperature=20; tp_tick(&c2,60005);
    assert(g.output>0.019f && g.output<0.021f);
    /* Finite but extreme PID settings must never reach the heater as NaN/Inf. */
    fake h={.interlock=true,.temperature=20};
    tp_hal hal3={&h,valves,pump,poll,temp,heat,adc,interlock,safe};
    tp_pid pid3={.kp=FLT_MAX,.output_max=1,.integral_max=1}; tp_controller c3;
    tp_init(&c3,&hal3,NULL,&pid3,100);
    const tp_step overflow[]={ {.kind=TP_STEP_HEAT,.timeout_ms=1000,.arg.heat={30,1}},
        {.kind=TP_STEP_END} };
    assert(tp_start(&c3,overflow,2,0)==TP_OK);
    tp_tick(&c3,0);
    assert(c3.state==TP_FAILED && c3.fault==TP_FAULT_IO && h.safe==1 && h.heat_calls==0);
    /* Invalid PID data must report a fault rather than silently output zero. */
    fake bad={.interlock=true,.temperature=20};
    tp_hal bad_hal={&bad,valves,pump,poll,temp,heat,adc,interlock,safe};
    tp_pid bad_pid={.kp=NAN,.output_max=1,.integral_max=1}; tp_controller bad_controller;
    tp_init(&bad_controller,&bad_hal,NULL,&bad_pid,100);
    assert(tp_start(&bad_controller,overflow,2,0)==TP_OK); tp_tick(&bad_controller,0);
    assert(bad_controller.state==TP_FAILED && bad_controller.fault==TP_FAULT_IO &&
           bad.safe==1 && bad.heat_calls==0);
    bad_pid.kp=-1; assert(isnan(tp_pid_update(&bad_pid,30,20,0.2f)));
    bad_pid.kp=1; bad_pid.output_max=2; assert(isnan(tp_pid_update(&bad_pid,30,20,0.2f)));
    assert(isnan(tp_pid_update(&pid,30,20,0)));
    /* Temperature remains a safety input during a non-heating wait. */
    fake j={.interlock=true,.temperature=20};
    tp_hal hal4={&j,valves,pump,poll,temp,heat,adc,interlock,safe};
    tp_controller c4; tp_init(&c4,&hal4,NULL,&pid,100);
    const tp_step wait_recipe[]={ {.kind=TP_STEP_WAIT,.timeout_ms=1000,.arg.wait={1000}},
        {.kind=TP_STEP_END} };
    assert(tp_start(&c4,wait_recipe,2,0)==TP_OK); tp_tick(&c4,0);
    j.temperature=100; tp_tick(&c4,200);
    assert(c4.state==TP_FAILED && c4.fault==TP_FAULT_OVERTEMP && j.safe==1 && !j.heat_calls);
    assert(tp_clear_fault(&c4)==TP_IO_ERROR && c4.state==TP_FAILED && c4.fault==TP_FAULT_OVERTEMP);
    j.temperature=20; assert(tp_clear_fault(&c4)==TP_OK);
    assert(tp_start(&c4,wait_recipe,2,300)==TP_OK); tp_tick(&c4,300);
    j.temperature=NAN; tp_tick(&c4,500);
    assert(c4.state==TP_FAILED && c4.fault==TP_FAULT_SENSOR && j.safe==3);
    assert(tp_clear_fault(&c4)==TP_IO_ERROR && c4.state==TP_FAILED);
    j.temperature=20; assert(tp_clear_fault(&c4)==TP_OK);
    /* A failed first temperature check must happen before starting a pump. */
    fake k={.interlock=true,.temperature=100};
    tp_hal hal5={&k,valves,pump,poll,temp,heat,adc,interlock,safe};
    tp_controller c5; tp_init(&c5,&hal5,NULL,&pid,100);
    const tp_step pump_recipe[]={ {.kind=TP_STEP_PUMP,.timeout_ms=1000,.arg.pump={100,500}},
        {.kind=TP_STEP_END} };
    assert(tp_start(&c5,pump_recipe,2,0)==TP_OK); tp_tick(&c5,0);
    assert(c5.state==TP_FAILED && c5.fault==TP_FAULT_OVERTEMP && k.pumps==0 && k.safe==1);
    assert(tp_clear_fault(&c5)==TP_IO_ERROR);
    k.temperature=20; assert(tp_clear_fault(&c5)==TP_OK); k.temperature=NAN;
    assert(tp_start(&c5,pump_recipe,2,200)==TP_OK); tp_tick(&c5,200);
    assert(c5.state==TP_FAILED && c5.fault==TP_FAULT_SENSOR && k.pumps==0 && k.safe==3);
    /* Reads between PID cycles must also update the published temperature. */
    fake m={.interlock=true,.temperature=20};
    tp_hal hal6={&m,valves,pump,poll,temp,heat,adc,interlock,safe};
    tp_controller c6; tp_init(&c6,&hal6,NULL,&pid,100);
    const tp_step heating[]={ {.kind=TP_STEP_HEAT,.timeout_ms=1000,.arg.heat={30,0.1f}},
        {.kind=TP_STEP_END} };
    tp_service s6; assert(tp_service_init(&s6,&c6,heating,2,0)==TP_OK);
    command=1; assert(tp_service_write_holding(&s6,0,1,&command)==TP_OK);
    tp_service_tick(&s6,0,false,0);
    m.temperature=29; tp_service_tick(&s6,1,false,0);
    assert(tp_service_read_holding(&s6,128,3,snapshot)==TP_OK &&
           snapshot[0]==1 && snapshot[1]==0x41e8 && snapshot[2]==0);
    m.temperature=NAN; tp_service_tick(&s6,2,false,0);
    assert(c6.state==TP_FAILED && c6.fault==TP_FAULT_SENSOR && !c6.temperature_valid);
    assert(tp_service_read_holding(&s6,128,3,snapshot)==TP_OK &&
           snapshot[0]==0 && snapshot[1]==0 && snapshot[2]==0);
    puts("controlled stepping checks passed"); return 0;
}
