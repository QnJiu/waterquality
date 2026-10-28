#include "tp_auto_calibration.h"
#include <math.h>
#include <string.h>

static bool recipe_has_readings(const tp_step *r, size_t n) {
    unsigned mask=0;
    if (!r || !n || r[n-1].kind!=TP_STEP_END) return false;
    for (size_t i=0; i<n; i++) {
        if (r[i].kind==TP_STEP_CALCULATE || (i+1<n && r[i].kind==TP_STEP_END)) return false;
        if (r[i].kind==TP_STEP_READ) {
            if ((unsigned)r[i].arg.read.slot>=TP_READ_COUNT) return false;
            mask |= 1u << r[i].arg.read.slot;
        }
    }
    return mask == (1u << TP_READ_COUNT)-1u;
}
tp_result tp_auto_cal_init(tp_auto_calibration *a, tp_controller *c,
    const tp_step *low, size_t low_len, const tp_step *high, size_t high_len,
    float low_concentration, float high_concentration, float min_delta,
    void *check_ctx, tp_result (*check)(void *)) {
    if (!a || !c || tp_validate_recipe(c,low,low_len)!=TP_OK ||
        tp_validate_recipe(c,high,high_len)!=TP_OK || !recipe_has_readings(low,low_len) || !recipe_has_readings(high,high_len) ||
        !isfinite(low_concentration) || !isfinite(high_concentration) || low_concentration<0 ||
        high_concentration<=low_concentration || !isfinite(min_delta) || min_delta<=0 ||
        !isfinite(c->calibration.max_concentration) || high_concentration>c->calibration.max_concentration ||
        !isfinite(c->calibration.turbidity_factor) || !isfinite(c->calibration.min_signal) ||
        c->calibration.min_signal<=0)
        return TP_INVALID;
    memset(a,0,sizeof(*a)); a->controller=c; a->recipes[0]=low; a->recipes[1]=high;
    a->lengths[0]=low_len; a->lengths[1]=high_len; a->base=c->calibration;
    a->points[0].concentration=low_concentration; a->points[1].concentration=high_concentration;
    a->min_delta=min_delta; a->check=check; a->check_ctx=check_ctx; return TP_OK;
}
static tp_result start_stage(tp_auto_calibration *a, unsigned stage, uint32_t now) {
    if (a->check) { tp_result r=a->check(a->check_ctx); if (r!=TP_OK) return r; }
    return tp_start(a->controller,a->recipes[stage],a->lengths[stage],now);
}
tp_result tp_auto_cal_start(tp_auto_calibration *a, uint32_t now) {
    if (!a || !a->controller || a->state!=TP_AUTO_CAL_IDLE) return TP_INVALID;
    tp_result r=start_stage(a,0,now);
    if (r==TP_OK) a->state=TP_AUTO_CAL_LOW;
    return r;
}
void tp_auto_cal_tick(tp_auto_calibration *a, uint32_t now) {
    if (!a || (a->state!=TP_AUTO_CAL_LOW && a->state!=TP_AUTO_CAL_HIGH)) return;
    tp_controller *c=a->controller; tp_tick(c,now);
    if (c->state==TP_FAILED) { a->error=TP_IO_ERROR; a->state=TP_AUTO_CAL_FAILED; return; }
    if (c->state!=TP_COMPLETE) {
        if (c->state!=TP_RUNNING) { a->error=TP_INVALID; a->state=TP_AUTO_CAL_FAILED; }
        return;
    }
    unsigned stage=a->state==TP_AUTO_CAL_LOW ? 0u : 1u;
    if (c->valid_readings != (1u << TP_READ_COUNT)-1u) {
        a->error=TP_INVALID; a->state=TP_AUTO_CAL_FAILED; return;
    }
    a->points[stage].blank=c->readings[TP_READ_BLANK];
    a->points[stage].reacted=c->readings[TP_READ_REACTED];
    a->points[stage].turbidity=c->readings[TP_READ_TURBIDITY];
    if (!stage) {
        a->error=start_stage(a,1,now);
        a->state=a->error==TP_OK ? TP_AUTO_CAL_HIGH : TP_AUTO_CAL_FAILED;
    } else {
        a->error=tp_calibrate_two_point(&a->base,&a->points[0],&a->points[1],a->min_delta,&a->fitted);
        a->state=a->error==TP_OK ? TP_AUTO_CAL_COMPLETE : TP_AUTO_CAL_FAILED;
    }
}
void tp_auto_cal_cancel(tp_auto_calibration *a) {
    if (!a || (a->state!=TP_AUTO_CAL_LOW && a->state!=TP_AUTO_CAL_HIGH)) return;
    if (a->controller->state==TP_RUNNING) tp_stop(a->controller);
    a->state=TP_AUTO_CAL_CANCELLED;
}
tp_result tp_auto_cal_result(const tp_auto_calibration *a, tp_calibration *out) {
    if (!a || !out || a->state!=TP_AUTO_CAL_COMPLETE) return TP_INVALID;
    *out=a->fitted; return TP_OK;
}
