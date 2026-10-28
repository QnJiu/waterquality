#ifndef TP_DOOR_H
#define TP_DOOR_H
#include "tp_core.h"

typedef enum { TP_DOOR_LOCKED, TP_DOOR_UNLOCKED, TP_DOOR_FAULT } tp_door_state;
typedef struct {
    void *ctx;
    /* TP_OK means the command was accepted, not that physical position is verified. */
    tp_result (*set_unlocked)(void *,bool unlocked);
} tp_door_io;
typedef struct {
    tp_door_io io;
    tp_door_state state;
    uint32_t pulse_ms, freshness_ms, unlocked_at, observed_at, next_id, active_id;
    bool observed, present, auth_pending;
    uint8_t card_id[10], card_id_len;
} tp_door;

/* Single owner; callbacks must be bounded and non-reentrant. */
tp_result tp_door_init(tp_door *,const tp_door_io *,uint32_t pulse_ms,
                       uint32_t freshness_ms,uint32_t first_id);
/* NULL and length zero means no card. A changed identifier starts a new request.
 * Zero is a valid request ID; no new request leaves *request unchanged. */
tp_result tp_door_observe(tp_door *,uint32_t now,const uint8_t *id,size_t id_len,
                          bool *new_request,uint32_t *request);
/* A response is valid only for the current pending observation. */
tp_result tp_door_authorize(tp_door *,uint32_t now,uint32_t request,bool approved);
/* Run frequently; stale reader observations revoke requests and close the output. */
tp_result tp_door_tick(tp_door *,uint32_t now);
/* Reader failure closes the output and latches a fault until reinitialized. */
tp_result tp_door_reader_fault(tp_door *);
#endif
