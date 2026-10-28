#include "tp_f030_rtc_time.h"
#include <stddef.h>

#define RTC_HZ 1024u
#define TICK_HZ 1000u
bool tp_f030_rtc_alarm_target(uint16_t subsecond,uint32_t budget_ms,uint16_t *target) {
    if (!target || subsecond>=RTC_HZ || !budget_ms || budget_ms>=1000u) return false;
    uint32_t count=budget_ms*RTC_HZ/TICK_HZ;
    if (!count || count>=RTC_HZ) return false;
    *target=(uint16_t)((subsecond+RTC_HZ-count)%RTC_HZ);
    return true;
}
bool tp_f030_rtc_elapsed(tp_f030_rtc_stamp start,tp_f030_rtc_stamp end,
                          uint32_t max_counts,uint32_t *counts) {
    if (!counts || start.second>=60u || end.second>=60u ||
        start.subsecond>=RTC_HZ || end.subsecond>=RTC_HZ ||
        !max_counts || max_counts>=60u*RTC_HZ) return false;
    uint32_t seconds=(end.second+60u-start.second)%60u;
    int32_t elapsed=(int32_t)(seconds*RTC_HZ)+(int32_t)start.subsecond-(int32_t)end.subsecond;
    if (elapsed<0 || (uint32_t)elapsed>max_counts) return false;
    *counts=(uint32_t)elapsed;
    return true;
}
bool tp_f030_rtc_to_ticks(uint32_t counts,uint16_t *phase,uint32_t *ticks) {
    if (!phase || !ticks || *phase>=RTC_HZ || counts>=60u*RTC_HZ) return false;
    uint32_t total=counts*TICK_HZ+*phase;
    *ticks=total/RTC_HZ;
    *phase=(uint16_t)(total%RTC_HZ);
    return true;
}

/* 8 MHz / 1024 Hz = 7812.5 CPU cycles per RTC count. */
bool tp_f030_rtc_systick(uint32_t counts,uint32_t remaining_cycles,
                          uint32_t expected_ticks,uint16_t *phase,
                          uint32_t *complete_ticks,uint32_t *next_cycles) {
    if(!phase || !complete_ticks || !next_cycles || *phase>=1024u ||
       counts>=60u*RTC_HZ || !remaining_cycles || remaining_cycles>8000u ||
       !expected_ticks) return false;
    uint32_t fraction=counts*512u+*phase;
    uint32_t cycles=counts*7812u+fraction/1024u;
    uint32_t complete=0, remaining=remaining_cycles;
    if(cycles>=remaining) {
        cycles-=remaining;
        complete=1u+cycles/8000u;
        remaining=8000u-cycles%8000u;
    } else remaining-=cycles;
    if(complete>=expected_ticks) return false;
    *phase=(uint16_t)(fraction%1024u);
    *complete_ticks=complete; *next_cycles=remaining;
    return true;
}
