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
