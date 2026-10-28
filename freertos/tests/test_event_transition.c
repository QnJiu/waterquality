#include "tp_event_transition.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    tp_controller c={0}; tp_event e={0};
    assert(!tp_event_transition(TP_IDLE,&c,&e));
    c.state=TP_RUNNING; assert(tp_event_transition(TP_IDLE,&c,&e) && e.type==TP_EVENT_STARTED);
    c.state=TP_COMPLETE; c.result_valid=true; c.result_mg_l=3.25f;
    assert(tp_event_transition(TP_RUNNING,&c,&e) && e.type==TP_EVENT_COMPLETED && e.detail==1);
    float result; memcpy(&result,&e.value,sizeof(result)); assert(result==3.25f);
    c.state=TP_FAILED; c.fault=TP_FAULT_INTERLOCK;
    assert(tp_event_transition(TP_COMPLETE,&c,&e) && e.type==TP_EVENT_FAILED && e.detail==TP_FAULT_INTERLOCK);
    c.state=TP_IDLE; assert(tp_event_transition(TP_FAILED,&c,&e) && e.type==TP_EVENT_CLEARED);
    assert(!tp_event_transition(TP_COMPLETE,&c,&e));
    puts("event transitions passed"); return 0;
}
