#include "tp_core.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>

typedef struct { const float *samples; unsigned next, safe; } fake;
static tp_result valves(void *p,uint32_t mask) { (void)p; (void)mask; return TP_OK; }
static tp_result pump(void *p,int32_t steps,uint32_t hz) { (void)p; (void)steps; (void)hz; return TP_OK; }
static tp_result poll(void *p,bool *done) { (void)p; *done=true; return TP_OK; }
static tp_result temperature(void *p,float *v) { (void)p; *v=20; return TP_OK; }
static tp_result heat(void *p,float v) { (void)p; (void)v; return TP_OK; }
static tp_result adc(void *p,uint8_t ch,float *v) { fake *f=p; (void)ch; *v=f->samples[f->next++]; return TP_OK; }
static bool interlock(void *p) { (void)p; return true; }
static void safe(void *p) { ((fake *)p)->safe++; }

int main(void) {
    const float samples[]={1,1,100,1,1}; fake f={.samples=samples};
    tp_hal hal={&f,valves,pump,poll,temperature,heat,adc,interlock,safe};
    tp_controller c; tp_init(&c,&hal,NULL,NULL,100);
    tp_step recipe[]={ {.kind=TP_STEP_READ,.timeout_ms=1000,
        .arg.read={.slot=TP_READ_BLANK,.samples=5,.trim_extremes=true}}, {.kind=TP_STEP_END} };
    assert(tp_start(&c,recipe,2,0)==TP_OK);
    for (unsigned i=0;i<5;i++) tp_tick(&c,i*50);
    assert(c.index==1 && fabsf(c.readings[TP_READ_BLANK]-1)<0.0001f);
    tp_tick(&c,250); assert(c.state==TP_COMPLETE);
    f.next=0; recipe[0].arg.read.trim_extremes=false;
    assert(tp_start(&c,recipe,2,300)==TP_OK);
    for (unsigned i=0;i<5;i++) tp_tick(&c,300+i*50);
    assert(fabsf(c.readings[TP_READ_BLANK]-20.8f)<0.0001f);
    tp_tick(&c,550); assert(c.state==TP_COMPLETE);
    recipe[0].arg.read.trim_extremes=true; recipe[0].arg.read.samples=2;
    assert(tp_start(&c,recipe,2,600)==TP_INVALID);
    const float huge[]={FLT_MAX,FLT_MAX,FLT_MAX}; f.samples=huge; f.next=0;
    recipe[0].arg.read.samples=3;
    assert(tp_start(&c,recipe,2,600)==TP_OK);
    for (unsigned i=0;i<3;i++) tp_tick(&c,600+i*50);
    assert(c.index==1 && c.readings[TP_READ_BLANK]==FLT_MAX);
    tp_tick(&c,750); assert(c.state==TP_COMPLETE);
    const float bad[]={NAN}; f.samples=bad; f.next=0;
    assert(tp_start(&c,recipe,2,800)==TP_OK); tp_tick(&c,800);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_SENSOR);
    /* Replacing a reading after calculating must invalidate the old result. */
    const float values[]={10,1,0,2}; f.samples=values; f.next=0;
    tp_calibration cal={.slope=1,.intercept=0,.turbidity_factor=0,
                        .min_signal=0.01f,.max_concentration=10};
    tp_init(&c,&hal,&cal,NULL,100);
    tp_step result_recipe[]={
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_BLANK,0,1}},
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_REACTED,0,1}},
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_TURBIDITY,0,1}},
        {.kind=TP_STEP_CALCULATE},
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_REACTED,0,1}},
        {.kind=TP_STEP_END}
    };
    assert(tp_start(&c,result_recipe,6,0)==TP_OK);
    for(unsigned i=0;i<4;i++) tp_tick(&c,i*50);
    assert(c.result_valid && fabsf(c.result_mg_l-1.0f)<0.0001f);
    tp_tick(&c,200); assert(!c.result_valid && c.readings[TP_READ_REACTED]==2);
    tp_tick(&c,250); assert(c.state==TP_COMPLETE && !c.result_valid);
    /* A subsequent calculation makes the new result valid. */
    tp_step recalculated[7];
    for(unsigned i=0;i<5;i++) recalculated[i]=result_recipe[i];
    recalculated[5]=(tp_step){.kind=TP_STEP_CALCULATE};
    recalculated[6]=(tp_step){.kind=TP_STEP_END}; f.next=0;
    assert(tp_start(&c,recalculated,7,300)==TP_OK);
    for(unsigned i=0;i<7;i++) tp_tick(&c,300+i*50);
    assert(c.state==TP_COMPLETE && c.result_valid &&
           fabsf(c.result_mg_l-log10f(5.0f))<0.0001f);
    puts("sample filtering checks passed"); return 0;
}
