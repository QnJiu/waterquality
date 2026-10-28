#include "tp_core.h"
#include <math.h>

static float limit(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

float tp_pid_update(tp_pid *p, float target, float measured, float dt)
{
    if (!p || !isfinite(target) || !isfinite(measured) || !isfinite(dt) || dt <= 0.0f ||
        !isfinite(p->kp) || !isfinite(p->ki) || !isfinite(p->kd) ||
        !isfinite(p->deadband_c) || !isfinite(p->integral_max) || !isfinite(p->output_max) ||
        p->deadband_c < 0 || p->integral_max < 0 || p->output_max < 0) return 0.0f;
    float error = target - measured;
    if (fabsf(error) <= p->deadband_c) error = 0;
    float derivative = p->initialized ? -(measured - p->previous_temp) / dt : 0;
    float candidate = limit(p->integral + error * dt, -p->integral_max, p->integral_max);
    float raw = p->kp * error + p->ki * candidate + p->kd * derivative;
    if (!isfinite(error) || !isfinite(derivative) || !isfinite(candidate) || !isfinite(raw))
        return NAN;
    /* Do not integrate further into saturation. */
    if ((raw >= 0 && raw <= p->output_max) || (raw > p->output_max && error < 0) ||
        (raw < 0 && error > 0)) p->integral = candidate;
    p->previous_temp = measured;
    p->initialized = true;
    float output = p->kp * error + p->ki * p->integral + p->kd * derivative;
    return isfinite(output) ? limit(output, 0, p->output_max) : NAN;
}

static tp_result optical_absorbance(const tp_calibration *c, float blank, float reacted,
                                    float turbidity, float *out) {
    if (!c || !out || !isfinite(blank) || !isfinite(reacted) || !isfinite(turbidity) ||
        !isfinite(c->turbidity_factor) || !isfinite(c->min_signal) || c->min_signal <= 0 ||
        blank < c->min_signal || reacted < c->min_signal) return TP_INVALID;
    /* Separate logarithms avoid overflowing an otherwise valid blank/reacted ratio. */
    float absorbance = log10f(blank) - log10f(reacted) - c->turbidity_factor * turbidity;
    if (!isfinite(absorbance)) return TP_INVALID;
    *out = absorbance; return TP_OK;
}

tp_result tp_concentration(const tp_calibration *c, float blank, float reacted,
                           float turbidity, float *out)
{
    float absorbance;
    if (!c || !out || !isfinite(c->slope) || !isfinite(c->intercept) ||
        !isfinite(c->max_concentration) || c->max_concentration <= 0 ||
        optical_absorbance(c, blank, reacted, turbidity, &absorbance) != TP_OK) return TP_INVALID;
    float value = c->slope * absorbance + c->intercept;
    if (!isfinite(value) || value < 0 || value > c->max_concentration) return TP_INVALID;
    *out = value; return TP_OK;
}

tp_result tp_calibrate_two_point(const tp_calibration *base,
    const tp_calibration_point *low, const tp_calibration_point *high,
    float min_delta, tp_calibration *out) {
    float a, b;
    if (!base || !low || !high || !out || !isfinite(base->max_concentration) ||
        base->max_concentration <= 0 || !isfinite(min_delta) || min_delta <= 0 ||
        !isfinite(low->concentration) || !isfinite(high->concentration) ||
        low->concentration < 0 || high->concentration <= low->concentration ||
        high->concentration > base->max_concentration ||
        optical_absorbance(base,low->blank,low->reacted,low->turbidity,&a)!=TP_OK ||
        optical_absorbance(base,high->blank,high->reacted,high->turbidity,&b)!=TP_OK ||
        !isfinite(b-a) || b-a < min_delta) return TP_INVALID;
    tp_calibration candidate=*base;
    candidate.slope=(high->concentration-low->concentration)/(b-a);
    candidate.intercept=low->concentration-candidate.slope*a;
    if (!isfinite(candidate.slope) || !isfinite(candidate.intercept)) return TP_INVALID;
    *out=candidate; return TP_OK;
}

tp_result tp_calibration_check(const tp_calibration *cal, const tp_calibration_point *ref,
    float tolerance, float *actual, bool *passed) {
    float value;
    if (!cal || !ref || !actual || !passed || !isfinite(tolerance) || tolerance < 0 ||
        !isfinite(ref->concentration) || ref->concentration < 0 ||
        ref->concentration > cal->max_concentration ||
        tp_concentration(cal,ref->blank,ref->reacted,ref->turbidity,&value)!=TP_OK) return TP_INVALID;
    *actual=value; *passed=fabsf(value-ref->concentration)<=tolerance; return TP_OK;
}
