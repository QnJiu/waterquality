#include "tp_clock_cache.h"
#include <stdint.h>
bool tp_clock_snapshot_now(const tp_clock_snapshot *s,uint32_t now,uint32_t max_age,int64_t *utc) {
    if (!s || !utc || !s->valid || !max_age || max_age>0x7fffffffu) return false;
    uint32_t elapsed=now-s->sampled_ms;
    if (elapsed>max_age || s->utc_seconds>INT64_MAX-(int64_t)(elapsed/1000u)) return false;
    *utc=s->utc_seconds+(int64_t)(elapsed/1000u);
    return true;
}
