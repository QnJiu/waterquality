#include "tp_valve_policy.h"
tp_result tp_valve_policy_validate(const tp_valve_policy *p) {
    if (!p) return TP_INVALID;
    for (unsigned i=0;i<TP_VALVE_COUNT;i++) {
        if ((p->forbidden[i] & ~TP_VALVE_MASK) || (p->forbidden[i] & (1u<<i))) return TP_INVALID;
        for (unsigned j=0;j<TP_VALVE_COUNT;j++)
            if (((p->forbidden[i]>>j)&1u)!=((p->forbidden[j]>>i)&1u)) return TP_INVALID;
    }
    return TP_OK;
}
bool tp_valve_policy_allows(const tp_valve_policy *p,uint32_t mask) {
    if (!p || (mask & ~TP_VALVE_MASK)) return false;
    for (unsigned i=0;i<TP_VALVE_COUNT;i++)
        if ((mask & (1u<<i)) && (mask & p->forbidden[i])) return false;
    return true;
}
tp_result tp_valve_reference_pins(uint32_t mask,uint16_t *b,uint16_t *d,uint16_t *e) {
    if (!b || !d || !e || (mask & ~TP_VALVE_MASK)) return TP_INVALID;
    uint16_t pb=0,pd=0,pe=0;
    for (unsigned i=0;i<6;i++) if (mask & (1u<<i)) pe|=(uint16_t)(1u<<(5-i));
    for (unsigned i=6;i<13;i++) if (mask & (1u<<i)) pb|=(uint16_t)(1u<<(15-i));
    for (unsigned i=13;i<19;i++) if (mask & (1u<<i)) pd|=(uint16_t)(1u<<(20-i));
    *b=pb; *d=pd; *e=pe; return TP_OK;
}
