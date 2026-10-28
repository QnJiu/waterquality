#ifndef TP_TRIAL_CONFIRM_H
#define TP_TRIAL_CONFIRM_H
#include "tp_boot.h"
/* Single serialized owner. Caller must supply a trustworthy running bank and a
 * fresh health snapshot on every poll; no image or metadata concurrent writes.
 * Interval and maximum time between polls must be less than 2^31 ms. */
typedef struct {
    tp_boot_io boot;
    uint32_t stable_ms, max_gap_ms, healthy_since, last_sample;
    uint8_t running_bank;
    bool timing, complete;
} tp_trial_confirm;
tp_result tp_trial_confirm_init(tp_trial_confirm *,const tp_boot_io *,uint8_t running_bank,uint32_t stable_ms,uint32_t max_gap_ms);
/* TP_BUSY means wait; a false health sample restarts the stability interval.
 * A poll gap exceeding max_gap_ms restarts the stability interval. A false
 * health snapshot must include any latched fault since the preceding poll.
 * After I/O failure poll again: a prior commit may already have succeeded. */
tp_result tp_trial_confirm_poll(tp_trial_confirm *,uint32_t now_ms,bool healthy);
#endif
