#include "tp_f407_recipe.h"
#include <assert.h>
#include <limits.h>
static tp_result valves(void *p,uint32_t x) { (void)p; (void)x; return TP_OK; }
static tp_result pump(void *p,int32_t x,uint32_t y) { (void)p; (void)x; (void)y; return TP_OK; }
static tp_result poll(void *p,bool *done) { (void)p; *done=true; return TP_OK; }
static tp_result temp(void *p,float *v) { (void)p; *v=20; return TP_OK; }
static tp_result heater(void *p,float v) { (void)p; (void)v; return TP_OK; }
static tp_result adc(void *p,uint8_t ch,float *v) { (void)p; (void)ch; *v=1; return TP_OK; }
static bool interlock(void *p) { (void)p; return true; }
static void safe(void *p) { (void)p; }
int main(void) {
    tp_hal hal={NULL,valves,pump,poll,temp,heater,adc,interlock,safe};
    tp_controller c; tp_init(&c,&hal,NULL,NULL,80);
    tp_valve_policy policy={0}; policy.forbidden[0]=2; policy.forbidden[1]=1;
    tp_step r[]={
        {.kind=TP_STEP_VALVES,.arg.valves={1}},
        {.kind=TP_STEP_PUMP,.timeout_ms=1000,.arg.pump={10,100}},
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_BLANK,0,1}},
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_REACTED,1,1}},
        {.kind=TP_STEP_READ,.timeout_ms=1000,.arg.read={TP_READ_TURBIDITY,2,1}},
        {.kind=TP_STEP_CALCULATE}, {.kind=TP_STEP_END}
    };
    size_t n=sizeof(r)/sizeof(r[0]);
    assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_OK);
    assert(tp_f407_recipe_outputs(r,n,true)==TP_OK);
    assert(tp_f407_recipe_outputs(r,n,false)==TP_OK);
    r[0].arg.valves.mask=3; assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_INVALID);
    r[0].arg.valves.mask=1u<<19; assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_INVALID);
    r[0].arg.valves.mask=1;
    r[1].arg.pump.frequency_hz=29; assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_INVALID);
    r[1].arg.pump.frequency_hz=1001; assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_INVALID);
    r[1].arg.pump.frequency_hz=100; r[1].arg.pump.steps=INT32_MIN;
    assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_INVALID);
    r[1].arg.pump.steps=-10; assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_OK);
    r[4].arg.read.channel=3; assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_INVALID);
    r[4].arg.read.channel=2; r[4].arg.read.slot=TP_READ_BLANK;
    assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_INVALID);
    r[5].kind=TP_STEP_WAIT; assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_OK);
    assert(tp_f407_recipe_outputs(r,n,true)==TP_INVALID);
    assert(tp_f407_recipe_outputs(r,n,false)==TP_INVALID); /* missing reacted */
    r[4].arg.read.slot=TP_READ_TURBIDITY;
    assert(tp_f407_recipe_outputs(r,n,false)==TP_OK);
    assert(tp_f407_recipe_outputs(r,n,true)==TP_INVALID);
    tp_step stale[]={r[2],r[3],r[4],{.kind=TP_STEP_CALCULATE},r[3],{.kind=TP_STEP_END}};
    assert(tp_f407_recipe_outputs(stale,6,true)==TP_INVALID);
    assert(tp_f407_recipe_validate(&c,r,n,&policy,0)==TP_INVALID);
    policy.forbidden[1]=0; assert(tp_f407_recipe_validate(&c,r,n,&policy,3)==TP_INVALID);
    return 0;
}
