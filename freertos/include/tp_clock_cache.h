#ifndef TP_CLOCK_CACHE_H
#define TP_CLOCK_CACHE_H
#include "tp_core.h"
/* Snapshot is transferred between tasks by value. The monotonic clock must be
 * milliseconds modulo 2^32 and max_age_ms must be less than 2^31. */
typedef struct { int64_t utc_seconds; uint32_t sampled_ms; bool valid; } tp_clock_snapshot;
bool tp_clock_snapshot_now(const tp_clock_snapshot *snapshot, uint32_t now_ms,
                           uint32_t max_age_ms, int64_t *utc_seconds);
#endif
