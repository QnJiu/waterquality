#ifndef TP_SCHEDULE_H
#define TP_SCHEDULE_H
#include "tp_core.h"
typedef enum { TP_MODE_CONTROLLED, TP_MODE_PERIODIC, TP_MODE_DAILY } tp_mode;
typedef struct {
    tp_mode mode;
    uint32_t period_ms, last_period_ms;
    uint16_t daily_minute; /* local time since midnight, 0..1439 */
    int16_t utc_offset_minutes;
    int64_t last_daily_day;
    bool daily_fired, initialized;
} tp_schedule;
/* Reconfiguration is rejected while a measurement is active. The period starts
 * from configuration time; daily mode needs a valid RTC at each poll. */
tp_result tp_schedule_configure(tp_schedule *s, tp_mode mode, uint32_t period_ms,
    uint16_t daily_minute, int16_t utc_offset_minutes, uint32_t now_ms, bool running);
/* A scheduled occurrence while busy is skipped, never queued. UTC is seconds
 * since 1970 and must be accompanied by a validity flag from the RTC driver. */
bool tp_schedule_poll(tp_schedule *s, uint32_t now_ms, bool rtc_valid,
    int64_t utc_seconds, bool running);
#endif
