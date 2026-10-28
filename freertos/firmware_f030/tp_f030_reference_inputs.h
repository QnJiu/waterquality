#ifndef TP_F030_REFERENCE_INPUTS_H
#define TP_F030_REFERENCE_INPUTS_H
#include "tp_f030_subordinate.h"
/* Called once by the subordinate task, after the scheduler has started. */
void tp_f030_reference_inputs_start(tp_f030_subordinate *);
/* Task context: acknowledge only after inspecting and resolving the leak.
 * TP_BUSY means the leak pin or an unprocessed leak edge is still active. */
tp_result tp_f030_reference_inputs_acknowledge_leak(void);
#endif
