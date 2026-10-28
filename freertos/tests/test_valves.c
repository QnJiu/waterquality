#include "tp_valve_policy.h"
#include <assert.h>
int main(void) {
    tp_valve_policy p={{0}};
    p.forbidden[0]=1u<<18; p.forbidden[18]=1u;
    assert(tp_valve_policy_validate(&p)==TP_OK);
    assert(tp_valve_policy_allows(&p,1u));
    assert(!tp_valve_policy_allows(&p,1u | (1u<<18)));
    assert(!tp_valve_policy_allows(&p,1u<<19));
    p.forbidden[18]=0; assert(tp_valve_policy_validate(&p)==TP_INVALID);
    uint16_t b,d,e;
    assert(tp_valve_reference_pins(TP_VALVE_MASK,&b,&d,&e)==TP_OK && b==0x3f8 && d==0xfc && e==0x3f);
    for (unsigned i=0;i<TP_VALVE_COUNT;i++) {
        assert(tp_valve_reference_pins(1u<<i,&b,&d,&e)==TP_OK);
        if (i<6) assert(e==(1u<<(5-i)) && !b && !d);
        else if (i<13) assert(b==(1u<<(15-i)) && !d && !e);
        else assert(d==(1u<<(20-i)) && !b && !e);
    }
    return 0;
}
