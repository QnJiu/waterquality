#ifndef TP_SUBORDINATE_POLL_H
#define TP_SUBORDINATE_POLL_H
#include "tp_subordinate.h"

/* send copies the whole frame before returning and must not call back into poll. */
typedef tp_result (*tp_sub_send)(void *,const uint8_t *,size_t);
typedef struct {
    tp_sub_master master;
    void *ctx;
    tp_sub_send send;
    uint32_t period_ms, last_attempt_ms;
    bool attempted;
} tp_sub_poll;

/* All functions, including safety reads, belong to the same task. */
tp_result tp_sub_poll_init(tp_sub_poll *,void *,tp_sub_send,uint32_t period_ms,
                           uint32_t timeout_ms,uint32_t freshness_ms,uint32_t first_token);
tp_result tp_sub_poll_tick(tp_sub_poll *,uint32_t now_ms);
tp_result tp_sub_poll_receive(tp_sub_poll *,uint32_t now_ms,const uint8_t *,size_t);
bool tp_sub_poll_healthy(const tp_sub_poll *,uint32_t now_ms);
uint16_t tp_sub_poll_resource_faults(const tp_sub_poll *,uint32_t now_ms);
#endif
