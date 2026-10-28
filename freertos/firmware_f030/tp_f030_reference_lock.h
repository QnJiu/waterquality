#ifndef TP_F030_REFERENCE_LOCK_H
#define TP_F030_REFERENCE_LOCK_H
#include "tp_door.h"
/* Reference only: PA5 high energizes an external unlock driver. The driver
 * must hold PA5 low during reset and convert low to the desired safe state. */
tp_result tp_f030_reference_lock_init(void);
tp_door_io tp_f030_reference_lock_io(void);
/* Safe to call with interrupts disabled, including an assertion path. */
void tp_f030_reference_lock_emergency_off(void);
#endif
