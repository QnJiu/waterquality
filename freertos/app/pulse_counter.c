#include "tp_pulse_counter.h"
tp_result tp_pulse_begin(tp_pulse_counter *c,int32_t steps,uint32_t hz) {
    if (!c || !steps || hz<30u || hz>1000u) return TP_INVALID;
    if (c->state==TP_PULSE_RUNNING) return TP_BUSY;
    uint64_t n=steps<0 ? (uint64_t)(-(int64_t)steps) : (uint64_t)steps;
    if (n>1000000u) return TP_INVALID;
    c->target=(uint32_t)n; c->completed=0; c->state=TP_PULSE_RUNNING;
    return TP_OK;
}
bool tp_pulse_completed(tp_pulse_counter *c) {
    if (!c || c->state!=TP_PULSE_RUNNING || c->completed>=c->target) return false;
    if (++c->completed==c->target) { c->state=TP_PULSE_DONE; return true; }
    return false;
}
void tp_pulse_abort(tp_pulse_counter *c) {
    if (c && c->state==TP_PULSE_RUNNING) c->state=TP_PULSE_ABORTED;
}
