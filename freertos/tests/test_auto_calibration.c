#include "tp_auto_calibration.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

typedef struct { unsigned reads, safe, checks; bool interlock, fail_check, bad_sample; } fake;
static tp_result valves(void *p,uint32_t m) { (void)p; (void)m; return TP_OK; }
static tp_result pump(void *p,int32_t n,uint32_t hz) { (void)p;(void)n;(void)hz;return TP_OK; }
static tp_result poll(void *p,bool *done) { (void)p;*done=true;return TP_OK; }
static tp_result temp(void *p,float *v) { (void)p;*v=20;return TP_OK; }
static tp_result heater(void *p,float v) { (void)p;(void)v;return TP_OK; }
static tp_result adc(void *p,uint8_t channel,float *v) {
    fake *f=p; (void)channel; const float samples[]={1,1,0,1,0.1f,0};
    *v=f->bad_sample ? NAN : samples[f->reads++]; return TP_OK;
}
static bool interlock(void *p) { return ((fake *)p)->interlock; }
static void safe(void *p) { ((fake *)p)->safe++; }
static tp_result check(void *p) { fake *f=p; f->checks++; return f->fail_check ? TP_IO_ERROR : TP_OK; }
static const tp_step recipe[]={
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={.slot=TP_READ_BLANK,.samples=1}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={.slot=TP_READ_REACTED,.samples=1}},
    {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={.slot=TP_READ_TURBIDITY,.samples=1}},
    {.kind=TP_STEP_END}
};
static void init(tp_auto_calibration *a,tp_controller *c,fake *f) {
    assert(tp_auto_cal_init(a,c,recipe,4,recipe,4,0,5,0.01f,f,check)==TP_OK);
}
int main(void) {
    fake f={.interlock=true}; tp_hal hal={&f,valves,pump,poll,temp,heater,adc,interlock,safe};
    tp_calibration base={1,0,0,0.001f,10}, out={0}; tp_controller c; tp_auto_calibration a;
    tp_init(&c,&hal,&base,NULL,100);
    tp_step invalid_high[4]; for(unsigned i=0;i<4;i++) invalid_high[i]=recipe[i];
    invalid_high[1].timeout_ms=0;
    assert(tp_auto_cal_init(&a,&c,recipe,4,invalid_high,4,0,5,0.01f,NULL,NULL)==TP_INVALID);
    assert(f.reads==0 && f.safe==0 && c.state==TP_IDLE);
    init(&a,&c,&f);
    assert(tp_auto_cal_result(&a,&out)==TP_INVALID);
    assert(tp_auto_cal_start(&a,0)==TP_OK);
    for (unsigned t=0;t<8;t++) tp_auto_cal_tick(&a,t*50);
    assert(a.state==TP_AUTO_CAL_COMPLETE && f.checks==2 && f.safe==2);
    assert(tp_auto_cal_result(&a,&out)==TP_OK && fabsf(out.slope-5)<1e-5f);
    assert(c.calibration.slope==1); /* fitting never silently applies parameters */
    assert(tp_auto_cal_start(&a,500)==TP_INVALID);
    f.reads=0; f.checks=0; init(&a,&c,&f); assert(tp_auto_cal_start(&a,500)==TP_OK);
    f.fail_check=true;
    for(unsigned t=0;t<4;t++) tp_auto_cal_tick(&a,500+t*50);
    assert(a.state==TP_AUTO_CAL_FAILED && a.error==TP_IO_ERROR && f.checks==2 && c.state==TP_COMPLETE);
    f.fail_check=false; f.reads=0; init(&a,&c,&f); assert(tp_auto_cal_start(&a,1000)==TP_OK);
    tp_auto_cal_cancel(&a); assert(a.state==TP_AUTO_CAL_CANCELLED && c.state==TP_FAILED && c.fault==TP_FAULT_STOP);
    assert(tp_clear_fault(&c)==TP_OK);
    f.bad_sample=true; init(&a,&c,&f); assert(tp_auto_cal_start(&a,1200)==TP_OK);
    tp_auto_cal_tick(&a,1200); assert(a.state==TP_AUTO_CAL_FAILED && c.fault==TP_FAULT_SENSOR);
    tp_calibration before=out; assert(tp_auto_cal_result(&a,&out)==TP_INVALID && out.slope==before.slope);
    assert(tp_auto_cal_init(&a,&c,recipe,3,recipe,4,0,5,0.01f,NULL,NULL)==TP_INVALID);
    puts("automatic calibration checks passed"); return 0;
}
