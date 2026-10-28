#ifndef TP_F030_RTC_TIME_H
#define TP_F030_RTC_TIME_H
#include <stdbool.h>
#include <stdint.h>

/* For LSE=32768 Hz and RTC prescalers 31/1023: the SSR counts down at
 * 1024 Hz. Seconds must be coherently sampled with SSR in 24-hour mode. */
typedef struct { uint8_t second; uint16_t subsecond; } tp_f030_rtc_stamp;
/* Returns an Alarm A SSR target no later than budget_ms after the sampled
 * counter edge. The caller must ensure programming completes before target. */
bool tp_f030_rtc_alarm_target(uint16_t subsecond,uint32_t budget_ms,uint16_t *target);
/* Compare stamps less than 60 seconds apart. More than max_counts, an invalid
 * stamp, or time moving backwards is rejected without changing outputs. */
bool tp_f030_rtc_elapsed(tp_f030_rtc_stamp start,tp_f030_rtc_stamp end,
                          uint32_t max_counts,uint32_t *counts);
/* Convert elapsed RTC counts to whole 1 kHz ticks, retaining the remainder.
 * phase is initially zero and remains in [0,1023]. This accounts only for the
 * measured RTC interval; the SysTick phase must be handled by its caller. */
bool tp_f030_rtc_to_ticks(uint32_t counts,uint16_t *phase,uint32_t *ticks);
#endif
