#ifndef TP_HEALTH_H
#define TP_HEALTH_H
#include "tp_core.h"
#define TP_HEALTH_MAX_TASKS 8u
/* One owner serializes all calls. Deadlines and check intervals must be less
 * than 2^31 ms; check at least once within that interval, including at wrap. */
typedef struct {
    uint32_t deadline_ms[TP_HEALTH_MAX_TASKS];
    uint32_t last_ms[TP_HEALTH_MAX_TASKS];
    uint32_t failed_mask;
    uint32_t seen_mask;
    uint8_t count;
    bool armed;
} tp_health;
/* Configure once at startup, not as a recovery after timeout; no feeding before arm. */
tp_result tp_health_init(tp_health *, const uint32_t *timeouts_ms, uint8_t count);
/* Starts all deadlines at now, providing one timeout of startup grace. */
tp_result tp_health_arm(tp_health *, uint32_t now_ms);
/* Record progress, but never revive an already expired or failed task. */
tp_result tp_health_heartbeat(tp_health *, uint8_t task, uint32_t now_ms);
/* Latches every expired task and permits feeding only if all remain healthy. */
bool tp_health_can_feed(tp_health *, uint32_t now_ms);
/* Requires a successful heartbeat from every task as well as live deadlines.
 * This is task liveness only, not a substitute for peripheral self-tests. */
bool tp_health_all_started(tp_health *, uint32_t now_ms);
#endif
