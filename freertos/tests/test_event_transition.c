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
    tp_service s={0}; s.cal_attached=true; s.standard_attached=true;
    tp_operation_snapshot before=tp_operation_capture(&s);
    s.auto_cal.state=TP_AUTO_CAL_LOW;
    assert(tp_operation_cal_event(before,&s,&e) && e.type==TP_EVENT_AUTO_CAL && e.detail==2);
    before=tp_operation_capture(&s); s.auto_cal.state=TP_AUTO_CAL_HIGH;
    assert(tp_operation_cal_event(before,&s,&e) && e.detail==3);
    before=tp_operation_capture(&s); s.auto_cal.state=TP_AUTO_CAL_FAILED; s.auto_cal.error=TP_IO_ERROR;
    assert(tp_operation_cal_event(before,&s,&e) && e.detail==5 && e.value==TP_IO_ERROR);
    before=tp_operation_capture(&s); s.auto_standard.state=TP_STANDARD_RUNNING;
    assert(tp_operation_standard_event(before,&s,&e) && e.type==TP_EVENT_AUTO_STANDARD && e.detail==2);
    before=tp_operation_capture(&s); s.auto_standard.state=TP_STANDARD_COMPLETE;
    s.auto_standard.actual=2.5f; s.auto_standard.passed=true;
    assert(tp_operation_standard_event(before,&s,&e) && e.detail==0x103);
    memcpy(&result,&e.value,sizeof(result)); assert(result==2.5f);
    before=tp_operation_capture(&s); assert(!tp_operation_standard_event(before,&s,&e));
    s.auto_standard.state=TP_STANDARD_IDLE;
    assert(!tp_operation_standard_event(before,&s,&e));
    s.standard_attached=false; s.auto_standard.state=TP_STANDARD_FAILED;
    assert(!tp_operation_standard_event(before,&s,&e));
    puts("event transitions passed"); return 0;
}
