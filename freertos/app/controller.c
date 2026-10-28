#include "tp_core.h"
#include <math.h>
#include <string.h>

#define TP_PID_PERIOD_MS 200u
#define TP_SAMPLE_PERIOD_MS 50u
#define TP_MAX_DURATION 0x7fffffffu
#define TP_ADVANCE_TIMEOUT_MS 30000u
static bool expired(uint32_t now, uint32_t start, uint32_t duration) { return (uint32_t)(now - start) >= duration; }
static void fail(tp_controller *c, tp_fault f) {
    c->heating = false; c->fault = f; c->state = TP_FAILED; c->hal.safe_outputs(c->hal.ctx);
}
static void next(tp_controller *c, uint32_t now) {
    c->index++; c->entered = false;
    if (c->single_step) { c->awaiting_advance = true; c->advance_wait_ms = now; }
}
void tp_init(tp_controller *c, const tp_hal *hal, const tp_calibration *cal,
             const tp_pid *pid, float max_temperature_c) {
    memset(c, 0, sizeof(*c));
    if (hal) c->hal = *hal;
    if (cal) c->calibration = *cal;
    if (pid) c->pid = *pid;
    c->max_temperature_c = max_temperature_c;
    c->state = TP_IDLE;
}
static bool valid_step(const tp_step *s) {
    if ((unsigned)s->kind > TP_STEP_END || s->timeout_ms > TP_MAX_DURATION) return false;
    switch (s->kind) {
    case TP_STEP_PUMP: return s->timeout_ms && s->arg.pump.steps && s->arg.pump.frequency_hz;
    case TP_STEP_HEAT: return s->timeout_ms && isfinite(s->arg.heat.target_c) &&
        isfinite(s->arg.heat.tolerance_c) && s->arg.heat.tolerance_c > 0;
    case TP_STEP_HOLD: case TP_STEP_WAIT: return s->arg.wait.duration_ms <= TP_MAX_DURATION &&
        s->timeout_ms >= s->arg.wait.duration_ms;
    case TP_STEP_READ: return s->timeout_ms && (unsigned)s->arg.read.slot < TP_READ_COUNT && s->arg.read.samples &&
        (!s->arg.read.trim_extremes || s->arg.read.samples >= 3);
    default: return true;
    }
}
tp_result tp_validate_recipe(const tp_controller *c, const tp_step *recipe, size_t count) {
    if (!c || !recipe || !count || !c->hal.safe_outputs || !c->hal.interlocks_ok ||
        !c->hal.set_valves || !c->hal.pump_start || !c->hal.pump_poll ||
        !c->hal.read_temperature || !c->hal.set_heater || !c->hal.read_adc ||
        !isfinite(c->max_temperature_c)) return TP_INVALID;
    if (recipe[count-1].kind != TP_STEP_END) return TP_INVALID;
    for (size_t i = 0; i < count; i++) {
        if (!valid_step(&recipe[i]) || (recipe[i].kind == TP_STEP_HEAT &&
             recipe[i].arg.heat.target_c >= c->max_temperature_c) ||
            (i + 1 < count && recipe[i].kind == TP_STEP_END)) return TP_INVALID;
    }
    return TP_OK;
}
tp_result tp_start(tp_controller *c, const tp_step *recipe, size_t count, uint32_t now) {
    if (!c) return TP_INVALID;
    if (c->state == TP_RUNNING || c->state == TP_FAILED) return TP_BUSY;
    if (tp_validate_recipe(c,recipe,count)!=TP_OK) return TP_INVALID;
    if (!c->hal.interlocks_ok(c->hal.ctx)) { fail(c, TP_FAULT_INTERLOCK); return TP_IO_ERROR; }
    c->recipe = recipe; c->recipe_len = count; c->index = 0; c->entered = false;
    c->state = TP_RUNNING; c->fault = TP_FAULT_NONE; c->heating = false;
    c->single_step = false; c->awaiting_advance = false;
    c->result_valid = false; c->valid_readings = 0; c->pid.integral = 0; c->pid.initialized = false;
    c->entered_ms = now; c->last_pid_ms = now - TP_PID_PERIOD_MS;
    return TP_OK;
}
tp_result tp_advance(tp_controller *c, uint32_t now) {
    if (!c || c->state != TP_RUNNING || !c->single_step || !c->awaiting_advance) return TP_INVALID;
    if (!c->hal.interlocks_ok(c->hal.ctx)) { fail(c, TP_FAULT_INTERLOCK); return TP_IO_ERROR; }
    if (expired(now, c->advance_wait_ms, TP_ADVANCE_TIMEOUT_MS)) { fail(c, TP_FAULT_TIMEOUT); return TP_TIMEOUT; }
    c->awaiting_advance = false;
    return TP_OK;
}
void tp_stop(tp_controller *c) { if (c && c->hal.safe_outputs && c->state != TP_FAILED) fail(c, TP_FAULT_STOP); }
tp_result tp_clear_fault(tp_controller *c) {
    if (!c || c->state != TP_FAILED || !c->hal.interlocks_ok(c->hal.ctx)) return TP_INVALID;
    c->hal.safe_outputs(c->hal.ctx); c->fault = TP_FAULT_NONE; c->state = TP_IDLE; return TP_OK;
}
void tp_tick(tp_controller *c, uint32_t now) {
    if (!c || c->state != TP_RUNNING) return;
    if (!c->hal.interlocks_ok(c->hal.ctx)) { fail(c, TP_FAULT_INTERLOCK); return; }
    if (c->index >= c->recipe_len) { fail(c, TP_FAULT_RECIPE); return; }
    const tp_step *s = &c->recipe[c->index];
    if (!c->entered && !c->awaiting_advance) {
        c->entered = true; c->entered_ms = now; c->sample_count = 0; c->sample_sum = 0;
        c->last_sample_ms = now - TP_SAMPLE_PERIOD_MS;
        if (s->kind == TP_STEP_PUMP && c->hal.pump_start(c->hal.ctx, s->arg.pump.steps,
                s->arg.pump.frequency_hz) != TP_OK) { fail(c, TP_FAULT_PUMP); return; }
        /* A new reading supersedes the previous value and any concentration
         * calculated from it. Only a later CALCULATE can publish a result. */
        if (s->kind == TP_STEP_READ) {
            c->valid_readings &= (uint8_t)~(1u << s->arg.read.slot);
            c->result_valid = false;
        }
        if (s->kind == TP_STEP_HEAT) {
            /* Off time is not an integration interval. Start a fresh PID cycle
             * when heating is enabled, including after a prior HEATER_OFF. */
            if (!c->heating) {
                c->pid.integral = 0; c->pid.initialized = false;
                c->last_pid_ms = now - TP_PID_PERIOD_MS;
            }
            c->target_c = s->arg.heat.target_c; c->heating = true;
        }
    }
    float temp = 0;
    if (c->heating && expired(now, c->last_pid_ms, TP_PID_PERIOD_MS)) {
        tp_result r = c->hal.read_temperature(c->hal.ctx, &temp);
        if (r != TP_OK || !isfinite(temp)) { fail(c, TP_FAULT_SENSOR); return; }
        if (temp >= c->max_temperature_c) { fail(c, TP_FAULT_OVERTEMP); return; }
        float dt = (float)(uint32_t)(now - c->last_pid_ms) / 1000.0f;
        c->last_pid_ms = now;
        if (c->hal.set_heater(c->hal.ctx, tp_pid_update(&c->pid, c->target_c, temp, dt)) != TP_OK) {
            fail(c, TP_FAULT_IO); return;
        }
    }
    if (c->awaiting_advance) {
        if (expired(now, c->advance_wait_ms, TP_ADVANCE_TIMEOUT_MS)) fail(c, TP_FAULT_TIMEOUT);
        return;
    }
    if (s->timeout_ms && expired(now, c->entered_ms, s->timeout_ms) &&
        s->kind != TP_STEP_WAIT && s->kind != TP_STEP_HOLD) { fail(c, TP_FAULT_TIMEOUT); return; }
    switch (s->kind) {
    case TP_STEP_VALVES:
        if (c->hal.set_valves(c->hal.ctx, s->arg.valves.mask) != TP_OK) fail(c, TP_FAULT_IO); else next(c, now); break;
    case TP_STEP_PUMP: {
        bool done = false; tp_result r = c->hal.pump_poll(c->hal.ctx, &done);
        if (r != TP_OK && r != TP_BUSY) fail(c, TP_FAULT_PUMP); else if (r == TP_OK && done) next(c, now); break;
    }
    case TP_STEP_HEAT:
        /* Read on each tick so entry and PID scheduling cannot falsely satisfy the target. */
        if (c->hal.read_temperature(c->hal.ctx, &temp) != TP_OK || !isfinite(temp)) fail(c, TP_FAULT_SENSOR);
        else if (temp >= c->max_temperature_c) fail(c, TP_FAULT_OVERTEMP);
        else if (fabsf(temp - c->target_c) <= s->arg.heat.tolerance_c) next(c, now);
        break;
    case TP_STEP_HOLD: case TP_STEP_WAIT:
        if (expired(now, c->entered_ms, s->arg.wait.duration_ms)) next(c, now);
        break;
    case TP_STEP_READ:
        if (expired(now, c->last_sample_ms, TP_SAMPLE_PERIOD_MS)) {
            float sample; tp_result r = c->hal.read_adc(c->hal.ctx, s->arg.read.channel, &sample);
            if (r == TP_OK && isfinite(sample)) {
                if (!c->sample_count) c->sample_min = c->sample_max = sample;
                else {
                    if (sample < c->sample_min) c->sample_min = sample;
                    if (sample > c->sample_max) c->sample_max = sample;
                }
                c->sample_sum += sample; c->sample_count++; c->last_sample_ms = now;
                if (c->sample_count == s->arg.read.samples) {
                    double sum = c->sample_sum;
                    unsigned count = c->sample_count;
                    if (s->arg.read.trim_extremes) { sum -= c->sample_min; sum -= c->sample_max; count -= 2; }
                    float mean = (float)(sum / count);
                    if (!isfinite(mean)) { fail(c, TP_FAULT_SENSOR); return; }
                    c->readings[s->arg.read.slot] = mean;
                    c->valid_readings |= (uint8_t)(1u << s->arg.read.slot); next(c, now);
                }
            } else if (r != TP_BUSY) fail(c, TP_FAULT_SENSOR);
        } break;
    case TP_STEP_CALCULATE:
        if (c->valid_readings != ((1u << TP_READ_COUNT) - 1u) || tp_concentration(&c->calibration,
            c->readings[TP_READ_BLANK], c->readings[TP_READ_REACTED], c->readings[TP_READ_TURBIDITY],
            &c->result_mg_l) != TP_OK) fail(c, TP_FAULT_CALIBRATION); else { c->result_valid = true; next(c, now); } break;
    case TP_STEP_HEATER_OFF:
        c->heating = false;
        if (c->hal.set_heater(c->hal.ctx, 0.0f) != TP_OK) fail(c, TP_FAULT_IO); else next(c, now);
        break;
    case TP_STEP_END:
        c->hal.safe_outputs(c->hal.ctx); c->heating = false; c->state = TP_COMPLETE; break;
    default: fail(c, TP_FAULT_RECIPE); break;
    }
}




