#include "tp_clock_cache.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    tp_clock_snapshot s={1700000000,0xfffffff0u,true}; int64_t now=1;
    assert(tp_clock_snapshot_now(&s,0x000003d8u,2000,&now) && now==1700000001);
    assert(!tp_clock_snapshot_now(&s,0x00001000u,2000,&now) && now==1700000001);
    s.valid=false; assert(!tp_clock_snapshot_now(&s,0,2000,&now));
    s.valid=true; s.sampled_ms=0; s.utc_seconds=INT64_MAX;
    assert(!tp_clock_snapshot_now(&s,1000,2000,&now));
    assert(!tp_clock_snapshot_now(&s,0,0,&now));
    return 0;
}
