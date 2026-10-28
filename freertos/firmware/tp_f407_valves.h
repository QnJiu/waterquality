#ifndef TP_F407_VALVES_H
#define TP_F407_VALVES_H
#include "tp_valve_policy.h"
/* Reference outputs: Y1..6 PE5..0, Y7..13 PB9..3, Y14..19 PD7..2,
 * active low. External pull-ups and power stages must hold all valves OFF
 * during reset. Init copies the explicitly supplied interlock policy. */
typedef struct { tp_valve_policy policy; bool initialized; } tp_f407_valves;
tp_result tp_f407_valves_init(tp_f407_valves *valves,const tp_valve_policy *policy);
tp_result tp_f407_valves_set(tp_f407_valves *valves,uint32_t open_mask);
/* Safe to call after initialization; drives all reference valves inactive. */
void tp_f407_valves_safe(tp_f407_valves *valves);
#endif
