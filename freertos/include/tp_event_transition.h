#ifndef TP_EVENT_TRANSITION_H
#define TP_EVENT_TRANSITION_H
#include "tp_event_log.h"
/* Stable persisted type numbers. Completion value contains IEEE-754 float bits
 * in mg/L; detail bit 0 marks validity. Failure detail holds tp_fault. */
typedef enum { TP_EVENT_STARTED=1, TP_EVENT_COMPLETED=2, TP_EVENT_FAILED=3,
               TP_EVENT_CLEARED=4, TP_EVENT_BOOT=5 } tp_event_type;
/* Produce at most one event per externally observable state transition. */
bool tp_event_transition(tp_run_state before,const tp_controller *after,tp_event *event);
#endif
