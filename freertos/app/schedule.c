#include "tp_schedule.h"
#include <string.h>
tp_result tp_schedule_configure(tp_schedule *s, tp_mode mode, uint32_t period,
    uint16_t minute, int16_t offset, uint32_t now, bool running) {
    if (!s || mode > TP_MODE_DAILY || running || minute >= 1440 || offset < -720 || offset > 840 ||
        (mode == TP_MODE_PERIODIC && (!period || period > 0x7fffffffu))) return TP_INVALID;
    memset(s, 0, sizeof(*s)); s->mode = mode; s->period_ms = period; s->last_period_ms = now;
    s->daily_minute = minute; s->utc_offset_minutes = offset; s->initialized = true; return TP_OK;
}
bool tp_schedule_poll(tp_schedule *s, uint32_t now, bool rtc_valid, int64_t utc, bool running) {
    if (!s || !s->initialized) return false;
    if (s->mode == TP_MODE_PERIODIC) {
        uint32_t elapsed = now - s->last_period_ms;
        if (elapsed < s->period_ms) return false;
        s->last_period_ms += (elapsed / s->period_ms) * s->period_ms;
        return !running;
    }
    if (s->mode != TP_MODE_DAILY || !rtc_valid || utc < 0 || utc > 253402300799LL) return false;
    int64_t local = utc + (int64_t)s->utc_offset_minutes * 60;
    /* Floor division also handles local times just before the epoch. */
    int64_t day = local / 86400, second = local % 86400;
    if (second < 0) { second += 86400; day--; }
    if ((uint16_t)(second / 60) != s->daily_minute ||
        (s->daily_fired && day <= s->last_daily_day)) return false;
    s->daily_fired = true; s->last_daily_day = day;
    return !running;
}
