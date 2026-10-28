#include "tp_f407_board_config.h"
#include "tp_f407_recipe.h"
#include "tp_auto_calibration.h"
#include "tp_service.h"
#include <assert.h>
#include <math.h>

static tp_result valves(void *p,uint32_t x) { (void)p; (void)x; return TP_OK; }
static tp_result pump(void *p,int32_t x,uint32_t hz) { (void)p; (void)x; (void)hz; return TP_OK; }
static tp_result poll(void *p,bool *done) { (void)p; *done=true; return TP_OK; }
static tp_result temp(void *p,float *v) { (void)p; *v=20; return TP_OK; }
static tp_result heater(void *p,float v) { (void)p; (void)v; return TP_OK; }
static tp_result adc(void *p,uint8_t ch,float *v) { (void)p; (void)ch; *v=1; return TP_OK; }
static bool interlock(void *p) { (void)p; return true; }
static void safe(void *p) { (void)p; }
/* Exercise the exact teaching recipes through the service, using fake inputs. */
typedef struct { uint32_t source; unsigned pumps, safe_calls; bool pump_fails; } teaching_fake;
static tp_result teaching_valves(void *p,uint32_t mask) {
    teaching_fake *f=p;
    if (mask==1 || mask==16 || mask==32) f->source=mask;
    return TP_OK;
}
static tp_result teaching_pump(void *p,int32_t steps,uint32_t hz) {
    teaching_fake *f=p; assert(steps==100 && hz==100); f->pumps++;
    return f->pump_fails ? TP_IO_ERROR : TP_OK;
}
static tp_result teaching_temp(void *p,float *v) { (void)p; *v=30; return TP_OK; }
static tp_result teaching_adc(void *p,uint8_t ch,float *v) {
    teaching_fake *f=p;
    if (ch==0) *v=1.0f;
    else if (ch==1) *v=f->source==16 ? 1.0f : f->source==32 ? 0.1f : 0.5f;
    else if (ch==2) *v=0.1f;
    else return TP_INVALID;
    return TP_OK;
}
static void teaching_safe(void *p) { ((teaching_fake *)p)->safe_calls++; }
static tp_result teaching_save(void *p,const tp_config *cfg) { (void)p; (void)cfg; return TP_OK; }
static tp_result teaching_preflight(void *p,uint16_t *faults) { (void)p; *faults=0; return TP_OK; }
static void teaching_flow(const tp_f407_app_options *o) {
    teaching_fake f={0};
    tp_hal hal={&f,teaching_valves,teaching_pump,poll,teaching_temp,heater,teaching_adc,interlock,teaching_safe};
    tp_controller c; tp_init(&c,&hal,&o->defaults.calibration,NULL,o->defaults.max_temperature_c);
    tp_service s; assert(tp_service_init(&s,&c,o->recipe,o->recipe_len,0)==TP_OK);
    assert(tp_service_attach_config(&s,&o->defaults,&f,teaching_save)==TP_OK);
    assert(tp_service_attach_preflight(&s,NULL,teaching_preflight)==TP_OK);
    const tp_f407_auto_cal_options *cal=o->auto_calibration;
    const tp_f407_auto_standard_options *std=o->auto_standard;
    assert(tp_service_attach_auto_calibration(&s,cal->low_recipe,cal->low_recipe_len,
        cal->high_recipe,cal->high_recipe_len,cal->low_concentration,cal->high_concentration,
        cal->min_absorbance_delta)==TP_OK);
    assert(tp_service_attach_auto_standard(&s,std->recipe,std->recipe_len,
        std->concentration,std->tolerance)==TP_OK);
    uint32_t now=0; uint16_t command=1;
    assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    for (unsigned i=0;i<500 && c.state==TP_RUNNING;i++) {
        now+=50; tp_service_tick(&s,now,false,0);
        if (c.awaiting_advance) { command=5; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK); }
    }
    assert(c.state==TP_COMPLETE && c.result_valid && fabsf(c.result_mg_l-0.30103f)<0.0001f);
    assert(f.pumps==4 && f.safe_calls==1);
    command=6; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    for (unsigned i=0;i<1000 && (s.auto_cal.state==TP_AUTO_CAL_LOW || s.auto_cal.state==TP_AUTO_CAL_HIGH);i++) {
        now+=50; tp_service_tick(&s,now,false,0);
    }
    assert(s.auto_cal.state==TP_AUTO_CAL_COMPLETE && f.pumps==12);
    command=7; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    assert(fabsf(c.calibration.slope-5.0f)<0.0001f);
    command=9; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    for (unsigned i=0;i<500 && s.auto_standard.state==TP_STANDARD_RUNNING;i++) {
        now+=50; tp_service_tick(&s,now,false,0);
    }
    assert(s.auto_standard.state==TP_STANDARD_COMPLETE && s.auto_standard.passed);
    assert(fabsf(s.auto_standard.actual-5.0f)<0.0001f && f.pumps==16 && f.safe_calls==4);
}
/* Exercise the real teaching recipe when its pump cannot start. */
static void teaching_pump_failure(const tp_f407_app_options *o) {
    teaching_fake f={.pump_fails=true};
    tp_hal hal={&f,teaching_valves,teaching_pump,poll,teaching_temp,heater,teaching_adc,interlock,teaching_safe};
    tp_controller c; tp_init(&c,&hal,&o->defaults.calibration,NULL,o->defaults.max_temperature_c);
    tp_service s; assert(tp_service_init(&s,&c,o->recipe,o->recipe_len,0)==TP_OK);
    uint16_t command=1; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK);
    for (uint32_t now=50; now<=5000 && c.state==TP_RUNNING; now+=50) {
        tp_service_tick(&s,now,false,0);
        if (c.awaiting_advance) { command=5; assert(tp_service_write_holding(&s,0,1,&command)==TP_OK); }
    }
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_PUMP && !c.result_valid);
    assert(f.pumps==1 && f.safe_calls==1);
    tp_service_tick(&s,5100,false,0);
    assert(f.pumps==1 && f.safe_calls==1);
}
int main(void) {
    tp_f407_app_options o;
    /* Teaching options remain fail-closed, but their examples are inspectable. */
    assert(tp_f407_board_options(&o)==TP_INVALID);
    assert(o.auto_calibration && o.auto_standard && tp_config_validate(&o.defaults)==TP_OK);
    assert(o.channel_count==3 && o.adc_registers[0]==0 && o.adc_registers[1]==0 &&
           o.adc_registers[2]==0 && o.adc_registers[3]==0);
    for (size_t i=0;i<o.channel_count;i++) {
        assert(o.channels[i].mux==i);
        assert(fabsf(o.channels[i].volts_per_count * 8388608.0f - 2.048f)<0.00001f);
    }
    assert(!o.interlocks_ok(o.interlocks_ctx));
    tp_hal hal={NULL,valves,pump,poll,temp,heater,adc,interlock,safe};
    tp_controller c; tp_init(&c,&hal,&o.defaults.calibration,NULL,o.defaults.max_temperature_c);
    const tp_f407_auto_cal_options *cal=o.auto_calibration;
    const tp_f407_auto_standard_options *std=o.auto_standard;
    assert(tp_f407_recipe_validate(&c,o.recipe,o.recipe_len,o.valves,o.channel_count)==TP_OK);
    assert(tp_f407_recipe_validate(&c,cal->low_recipe,cal->low_recipe_len,o.valves,o.channel_count)==TP_OK);
    assert(tp_f407_recipe_validate(&c,cal->high_recipe,cal->high_recipe_len,o.valves,o.channel_count)==TP_OK);
    assert(tp_f407_recipe_validate(&c,std->recipe,std->recipe_len,o.valves,o.channel_count)==TP_OK);
    assert(tp_f407_recipe_outputs(o.recipe,o.recipe_len,true)==TP_OK);
    assert(tp_f407_recipe_outputs(cal->low_recipe,cal->low_recipe_len,false)==TP_OK);
    assert(tp_f407_recipe_outputs(cal->high_recipe,cal->high_recipe_len,false)==TP_OK);
    assert(tp_f407_recipe_outputs(std->recipe,std->recipe_len,false)==TP_OK);
    tp_auto_calibration a; tp_auto_standard b;
    assert(tp_auto_cal_init(&a,&c,cal->low_recipe,cal->low_recipe_len,cal->high_recipe,
        cal->high_recipe_len,cal->low_concentration,cal->high_concentration,
        cal->min_absorbance_delta,NULL,NULL)==TP_OK);
    assert(tp_auto_standard_init(&b,&c,std->recipe,std->recipe_len,std->concentration,
        std->tolerance,NULL,NULL)==TP_OK);
    assert(cal->low_recipe[0].arg.valves.mask != cal->high_recipe[0].arg.valves.mask);
    teaching_flow(&o);
    teaching_pump_failure(&o);
    return 0;
}
