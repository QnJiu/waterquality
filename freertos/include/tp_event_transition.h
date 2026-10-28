#ifndef TP_EVENT_TRANSITION_H
#define TP_EVENT_TRANSITION_H
#include "tp_event_log.h"
#include "tp_service.h"
/* Stable persisted type numbers. Completion value contains IEEE-754 float bits
 * in mg/L; detail bit 0 marks validity. Failure detail holds tp_fault. */
typedef enum { TP_EVENT_STARTED=1, TP_EVENT_COMPLETED=2, TP_EVENT_FAILED=3,
               TP_EVENT_CLEARED=4, TP_EVENT_BOOT=5,
               TP_EVENT_AUTO_CAL=6, TP_EVENT_AUTO_STANDARD=7 } tp_event_type;
/* Produce at most one event per externally observable state transition. */
bool tp_event_transition(tp_run_state before,const tp_controller *after,tp_event *event);
/* Capture before processing one command, tick or maintenance request, then
 * report meaningful operation state changes. Completion of a standard check
 * stores the float concentration in value and pass flag in detail bit 8;
 * failure stores the tp_result code in value. Low detail byte is state+1. */
typedef struct { tp_auto_cal_state calibration; tp_standard_state standard; } tp_operation_snapshot;
tp_operation_snapshot tp_operation_capture(const tp_service *);
bool tp_operation_cal_event(tp_operation_snapshot, const tp_service *, tp_event *);
bool tp_operation_standard_event(tp_operation_snapshot, const tp_service *, tp_event *);
#endif
