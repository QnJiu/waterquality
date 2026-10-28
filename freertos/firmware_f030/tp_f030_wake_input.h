#ifndef TP_F030_WAKE_INPUT_H
#define TP_F030_WAKE_INPUT_H
#include "tp_f030_subordinate.h"
/* Called once by the owner task; owns PA8, EXTI8 and EXTI4_15_IRQn. */
void tp_f030_wake_input_start(tp_f030_subordinate *);
/* True if uninitialized, high level, or an unhandled rising edge. For sleep entry the caller must mask
 * interrupts and recheck this together with every other wake condition. */
bool tp_f030_wake_input_active(void);
#endif
