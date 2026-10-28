#include "tp_event_transition.h"
#include <string.h>
_Static_assert(sizeof(float)==sizeof(uint32_t), "event float needs 32 bits");
bool tp_event_transition(tp_run_state before,const tp_controller *after,tp_event *event) {
    if(!after || !event || before==after->state) return false;
    tp_event e={0};
    switch(after->state) {
    case TP_RUNNING: e.type=TP_EVENT_STARTED; break;
    case TP_COMPLETE:
        e.type=TP_EVENT_COMPLETED; e.detail=after->result_valid ? 1u : 0u;
        if(after->result_valid) memcpy(&e.value,&after->result_mg_l,sizeof(e.value));
        break;
    case TP_FAILED: e.type=TP_EVENT_FAILED; e.detail=(uint16_t)after->fault; break;
    case TP_IDLE: if(before!=TP_FAILED) return false; e.type=TP_EVENT_CLEARED; break;
    default: return false;
    }
    *event=e; return true;
}

/* Operation events supplement controller events so standard and calibration
 * runs can be distinguished from measurements even when no concentration is
 * produced by the underlying controller. */
tp_operation_snapshot tp_operation_capture(const tp_service *s) {
    tp_operation_snapshot snapshot={TP_AUTO_CAL_IDLE,TP_STANDARD_IDLE};
    if (s) { snapshot.calibration=s->auto_cal.state; snapshot.standard=s->auto_standard.state; }
    return snapshot;
}
bool tp_operation_cal_event(tp_operation_snapshot before,const tp_service *s,tp_event *out) {
    if (!s || !out || !s->cal_attached || before.calibration==s->auto_cal.state ||
        s->auto_cal.state==TP_AUTO_CAL_IDLE) return false;
    tp_event e={0}; e.type=TP_EVENT_AUTO_CAL; e.detail=(uint16_t)(s->auto_cal.state+1);
    if (s->auto_cal.state==TP_AUTO_CAL_FAILED) e.value=(uint32_t)s->auto_cal.error;
    *out=e; return true;
}
bool tp_operation_standard_event(tp_operation_snapshot before,const tp_service *s,tp_event *out) {
    if (!s || !out || !s->standard_attached || before.standard==s->auto_standard.state ||
        s->auto_standard.state==TP_STANDARD_IDLE) return false;
    tp_event e={0}; e.type=TP_EVENT_AUTO_STANDARD; e.detail=(uint16_t)(s->auto_standard.state+1);
    if (s->auto_standard.state==TP_STANDARD_COMPLETE) {
        if (s->auto_standard.passed) e.detail |= 0x100u;
        memcpy(&e.value,&s->auto_standard.actual,sizeof(e.value));
    } else if (s->auto_standard.state==TP_STANDARD_FAILED) e.value=(uint32_t)s->auto_standard.error;
    *out=e; return true;
}
