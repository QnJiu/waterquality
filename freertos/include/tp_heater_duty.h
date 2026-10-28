#ifndef TP_HEATER_DUTY_H
#define TP_HEATER_DUTY_H
#include "tp_core.h"
/* Converts a finite 0..1 fraction to timer ticks; output unchanged on error. */
tp_result tp_heater_duty_ticks(float fraction,uint32_t period_ticks,uint32_t *on_ticks);
#endif
