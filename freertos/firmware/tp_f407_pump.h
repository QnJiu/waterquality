#ifndef TP_F407_PUMP_H
#define TP_F407_PUMP_H
#include "tp_pulse_counter.h"
/* TIM4 CH2 PD13 STEP, PD14 DIR, PD15 active-low EN. Exclusive TIM4 ownership.
 * 30..1000 Hz nominal, up to 1,000,000 pulses. Positive steps set DIR high.
 * One-shot timer prevents extra pulses if interrupt handling is delayed; actual
 * speed is reduced by ISR latency. Electrical/driver timing must be verified. */
typedef struct { tp_pulse_counter counter; bool initialized; } tp_f407_pump;
tp_result tp_f407_pump_init(tp_f407_pump *pump);
tp_result tp_f407_pump_start(tp_f407_pump *pump,int32_t steps,uint32_t frequency_hz);
tp_result tp_f407_pump_poll(tp_f407_pump *pump,bool *done);
/* Disables the driver immediately and aborts any partial pulse. */
void tp_f407_pump_stop(tp_f407_pump *pump);
#endif
