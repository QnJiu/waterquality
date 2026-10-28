#include "tp_health.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    tp_health h={0}; uint32_t t[]={100,200};
    assert(!tp_health_can_feed(&h,0));
    assert(tp_health_init(&h,t,0)==TP_INVALID);
    uint32_t bad[]={0x80000000u}; assert(tp_health_init(&h,bad,1)==TP_INVALID);
    assert(tp_health_init(&h,t,2)==TP_OK);
    assert(tp_health_heartbeat(&h,0,0)==TP_INVALID);
    assert(tp_health_arm(&h,0xfffffff0u)==TP_OK);
    assert(tp_health_arm(&h,0)==TP_INVALID);
    assert(!tp_health_all_started(&h,0xfffffff0u));
    assert(tp_health_heartbeat(&h,0,0x20)==TP_OK);
    assert(tp_health_can_feed(&h,0x60));
    assert(!tp_health_all_started(&h,0x60));
    assert(tp_health_heartbeat(&h,1,0x60)==TP_OK);
    assert(tp_health_all_started(&h,0x60));
    assert(!tp_health_can_feed(&h,0x84)); /* task 0 exactly 100 ms */
    assert(h.failed_mask==1);
    assert(!tp_health_all_started(&h,0x84));
    assert(tp_health_heartbeat(&h,0,0x84)==TP_TIMEOUT);
    assert(!tp_health_can_feed(&h,0x85));
    assert(tp_health_init(&h,t,2)==TP_OK && tp_health_arm(&h,10)==TP_OK);
    assert(tp_health_heartbeat(&h,0,210)==TP_TIMEOUT);
    assert(h.failed_mask==3);
    assert(!tp_health_all_started(&h,210));
    return 0;
}
