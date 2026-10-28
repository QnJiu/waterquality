#ifndef TP_SUBORDINATE_H
#define TP_SUBORDINATE_H
#include "tp_preflight.h"

/* Binary request/reply protocol; one outstanding request, no broadcast.
 * Transport supplies complete frames; CRC detects corruption, not forgery. */
#define TP_SUB_REQUEST_SIZE 8u
#define TP_SUB_RESPONSE_SIZE 16u
#define TP_SUB_REAGENT_FAULT 0x01u
#define TP_SUB_LEVEL_FAULT 0x02u
#define TP_SUB_DEVICE_FAULT 0x04u
#define TP_SUB_LEAK_FAULT 0x08u

typedef struct {
    uint32_t droplet_count;
    uint8_t faults;
} tp_sub_status;
typedef struct {
    uint32_t next_token, outstanding_token, deadline_start, last_success;
    uint32_t response_timeout_ms, freshness_ms;
    tp_sub_status status;
    bool pending, have_status;
} tp_sub_master;

/* Durations are 1..INT32_MAX; a single owner serializes all calls. */
tp_result tp_sub_master_init(tp_sub_master *,uint32_t response_timeout_ms,uint32_t freshness_ms,
                             uint32_t first_token);
tp_result tp_sub_master_request(tp_sub_master *,uint32_t now,uint8_t out[TP_SUB_REQUEST_SIZE]);
tp_result tp_sub_master_receive(tp_sub_master *,uint32_t now,const uint8_t *,size_t);
/* Cancels expired requests and invalidates stale status. */
void tp_sub_master_tick(tp_sub_master *,uint32_t now);
bool tp_sub_master_healthy(const tp_sub_master *,uint32_t now);
uint16_t tp_sub_master_resource_faults(const tp_sub_master *,uint32_t now);
/* Reply snapshots the status provided by the subordinate owner. */
tp_result tp_sub_reply(const uint8_t *,size_t,const tp_sub_status *,uint8_t out[TP_SUB_RESPONSE_SIZE]);
#endif
