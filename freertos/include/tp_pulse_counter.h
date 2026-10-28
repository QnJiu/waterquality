#ifndef TP_PULSE_COUNTER_H
#define TP_PULSE_COUNTER_H
#include "tp_core.h"
/* Counts completed timer periods, not motor position or liquid volume. Callers
 * must serialize the counter between control task and timer interrupt. */
typedef enum { TP_PULSE_IDLE, TP_PULSE_RUNNING, TP_PULSE_DONE, TP_PULSE_ABORTED } tp_pulse_state;
typedef struct { uint32_t target, completed; tp_pulse_state state; } tp_pulse_counter;
tp_result tp_pulse_begin(tp_pulse_counter *counter, int32_t steps, uint32_t frequency_hz);
/* True only when the final pulse was completed. */
bool tp_pulse_completed(tp_pulse_counter *counter);
void tp_pulse_abort(tp_pulse_counter *counter);
#endif
