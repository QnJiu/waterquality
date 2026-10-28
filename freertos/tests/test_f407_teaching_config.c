#include "tp_f407_board_config.h"
#include "tp_f407_recipe.h"
#include "tp_auto_calibration.h"
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
    tp_auto_calibration a; tp_auto_standard b;
    assert(tp_auto_cal_init(&a,&c,cal->low_recipe,cal->low_recipe_len,cal->high_recipe,
        cal->high_recipe_len,cal->low_concentration,cal->high_concentration,
        cal->min_absorbance_delta,NULL,NULL)==TP_OK);
    assert(tp_auto_standard_init(&b,&c,std->recipe,std->recipe_len,std->concentration,
        std->tolerance,NULL,NULL)==TP_OK);
    assert(cal->low_recipe[0].arg.valves.mask != cal->high_recipe[0].arg.valves.mask);
    return 0;
}
