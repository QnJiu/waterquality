#ifndef TP_VALVE_POLICY_H
#define TP_VALVE_POLICY_H
#include "tp_core.h"
#define TP_VALVE_COUNT 19u
#define TP_VALVE_MASK ((1u << TP_VALVE_COUNT)-1u)
/* Bit i in forbidden[j] means valves i and j may not be open together.
 * Policy is supplied from a verified fluidic design. */
typedef struct { uint32_t forbidden[TP_VALVE_COUNT]; } tp_valve_policy;
tp_result tp_valve_policy_validate(const tp_valve_policy *policy);
bool tp_valve_policy_allows(const tp_valve_policy *policy, uint32_t mask);
/* Pure translation to the reference F407 valve pins, useful for review/tests. */
tp_result tp_valve_reference_pins(uint32_t mask,uint16_t *port_b,uint16_t *port_d,uint16_t *port_e);
#endif
