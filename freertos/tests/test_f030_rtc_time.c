#include "tp_f030_rtc_time.h"
#include <assert.h>
#include <stdint.h>
int main(void) {
    uint16_t target=999, phase=0;
    uint32_t counts=999, ticks=999, sum=0;
    assert(tp_f030_rtc_alarm_target(3,10,&target) && target==1017);
    assert(tp_f030_rtc_alarm_target(0,1,&target) && target==1023);
    assert(!tp_f030_rtc_alarm_target(1024,1,&target));
    assert(!tp_f030_rtc_alarm_target(0,1000,&target));
    assert(!tp_f030_rtc_alarm_target(0,0,&target));
    for (unsigned ms=1;ms<1000;ms++) {
        assert(tp_f030_rtc_alarm_target(511,ms,&target));
        unsigned distance=(511u+1024u-target)%1024u;
        assert(distance>0 && distance*1000u<=ms*1024u);
    }
    assert(tp_f030_rtc_elapsed((tp_f030_rtc_stamp){59,3},
                               (tp_f030_rtc_stamp){0,1017},10,&counts) && counts==10);
    assert(!tp_f030_rtc_elapsed((tp_f030_rtc_stamp){59,3},
                                (tp_f030_rtc_stamp){0,1017},9,&counts) && counts==10);
    assert(!tp_f030_rtc_elapsed((tp_f030_rtc_stamp){0,3},
                                (tp_f030_rtc_stamp){0,4},10,&counts));
    assert(tp_f030_rtc_elapsed((tp_f030_rtc_stamp){12,800},
                               (tp_f030_rtc_stamp){13,799},1025,&counts) && counts==1025);
    assert(!tp_f030_rtc_elapsed((tp_f030_rtc_stamp){0,1024},
                                (tp_f030_rtc_stamp){0,1},10,&counts));
    for (unsigned i=0;i<1024;i++) {
        assert(tp_f030_rtc_to_ticks(1,&phase,&ticks)); sum+=ticks;
    }
    assert(sum==1000 && phase==0);
    assert(tp_f030_rtc_to_ticks(1023,&phase,&ticks) && ticks==999 && phase==24);
    phase=1024; ticks=77;
    assert(!tp_f030_rtc_to_ticks(1,&phase,&ticks) && ticks==77 && phase==1024);
    /* The tickless path keeps a partial SysTick and half-cycle remainder. */
    uint32_t remaining=5000, complete=0; phase=0; sum=0;
    for(unsigned i=0;i<1024;i++) {
        assert(tp_f030_rtc_systick(1,remaining,4,&phase,&complete,&remaining));
        sum+=complete;
    }
    assert(sum==1000 && remaining==5000 && phase==0);
    remaining=8000; phase=0;
    assert(tp_f030_rtc_systick(1,remaining,2,&phase,&complete,&remaining));
    assert(complete==0 && remaining==188 && phase==512);
    assert(tp_f030_rtc_systick(1,remaining,2,&phase,&complete,&remaining));
    assert(complete==1 && remaining==375 && phase==0);
    remaining=8000; complete=123; phase=0;
    assert(!tp_f030_rtc_systick(3,remaining,2,&phase,&complete,&remaining));
    assert(remaining==8000 && complete==123 && phase==0);
    assert(!tp_f030_rtc_systick(1,0,2,&phase,&complete,&remaining));
    assert(!tp_f030_rtc_systick(1,8001,2,&phase,&complete,&remaining));
    return 0;
}
