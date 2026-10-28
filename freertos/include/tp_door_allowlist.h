#ifndef TP_DOOR_ALLOWLIST_H
#define TP_DOOR_ALLOWLIST_H
#include "tp_core.h"

typedef struct { uint8_t uid[10]; uint8_t length; } tp_door_card;
/* Queue the decision for the door owner; do not call the service reentrantly. */
typedef tp_result (*tp_door_decision_sink)(void *,uint32_t,bool);
typedef struct {
    const tp_door_card *cards;
    size_t count;
    void *ctx;
    tp_door_decision_sink submit;
} tp_door_allowlist;
/* The table is immutable and must outlive this policy. Empty table denies all.
 * UID comparison is identification only; UIDs can be copied. */
tp_result tp_door_allowlist_init(tp_door_allowlist *,const tp_door_card *,size_t,
                                 void *,tp_door_decision_sink);
/* Compatible with tp_door_request; submit must copy and enqueue the decision. */
tp_result tp_door_allowlist_request(void *,uint32_t,const uint8_t *,size_t);
#endif
