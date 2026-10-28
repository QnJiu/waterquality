#include "tp_pulse_counter.h"
#include <assert.h>
#include <limits.h>
int main(void) {
    tp_pulse_counter c={0};
    assert(tp_pulse_begin(&c,0,500)==TP_INVALID);
    assert(tp_pulse_begin(&c,1,1001)==TP_INVALID);
    assert(tp_pulse_begin(&c,INT_MIN,500)==TP_INVALID);
    assert(tp_pulse_begin(&c,-3,500)==TP_OK && c.target==3);
    assert(tp_pulse_begin(&c,1,500)==TP_BUSY);
    assert(!tp_pulse_completed(&c) && !tp_pulse_completed(&c));
    assert(tp_pulse_completed(&c) && c.state==TP_PULSE_DONE && c.completed==3);
    assert(!tp_pulse_completed(&c) && c.completed==3);
    assert(tp_pulse_begin(&c,5,30)==TP_OK); tp_pulse_abort(&c);
    assert(c.state==TP_PULSE_ABORTED && !tp_pulse_completed(&c));
    assert(tp_pulse_begin(&c,1,1000)==TP_OK && tp_pulse_completed(&c));
    return 0;
}
