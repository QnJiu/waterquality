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
    /* Do not integrate further into saturation. */
    if ((raw >= 0 && raw <= p->output_max) || (raw > p->output_max && error < 0) ||
        (raw < 0 && error > 0)) p->integral = candidate;
    p->previous_temp = measured;
    p->initialized = true;
    return limit(p->kp * error + p->ki * p->integral + p->kd * derivative, 0, p->output_max);
}

tp_result tp_concentration(const tp_calibration *c, float blank, float reacted,
                           float turbidity, float *out)
{
    if (!c || !out || !isfinite(blank) || !isfinite(reacted) || !isfinite(turbidity) ||
        !isfinite(c->slope) || !isfinite(c->intercept) || !isfinite(c->turbidity_factor) ||
        !isfinite(c->min_signal) || !isfinite(c->max_concentration) || c->min_signal <= 0 ||
        c->max_concentration <= 0 || blank < c->min_signal || reacted < c->min_signal)
        return TP_INVALID;
    /* Calibration model is configurable; optical and chemical validation is required. */
    float absorbance = log10f(blank / reacted) - c->turbidity_factor * turbidity;
    float value = c->slope * absorbance + c->intercept;
    if (!isfinite(value) || value < 0 || value > c->max_concentration) return TP_INVALID;
    *out = value;
    return TP_OK;
}
