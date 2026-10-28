#ifndef TP_F407_HEATER_H
#define TP_F407_HEATER_H
#include "tp_heater_duty.h"
/* TIM2_CH1 PA15, active low; exclusive TIM2 ownership. External pull-up and
 * independent overtemperature cutoff are required. */
typedef struct { bool initialized, active; } tp_f407_heater;
tp_result tp_f407_heater_init(tp_f407_heater *heater);
tp_result tp_f407_heater_set(tp_f407_heater *heater,float fraction);
/* Force GPIO high immediately, independently of timer update. */
void tp_f407_heater_off(tp_f407_heater *heater);
#endif
