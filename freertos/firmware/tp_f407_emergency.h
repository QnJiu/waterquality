#ifndef TP_F407_EMERGENCY_H
#define TP_F407_EMERGENCY_H
/* Interrupts must already be disabled. Terminal fault path; never resume running
 * drivers after this call. At reset it may initialize safe outputs before any
 * driver starts; the application must subsequently initialize its drivers.
 * This forces only the reference actuator pins inactive, without RTOS calls. */
void tp_f407_emergency_off(void);
#endif
