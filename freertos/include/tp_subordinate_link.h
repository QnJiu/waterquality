#ifndef TP_SUBORDINATE_LINK_H
#define TP_SUBORDINATE_LINK_H
#include "tp_rtu_receiver.h"
#include "tp_subordinate_poll.h"

/* status takes a coherent snapshot. send copies the entire frame. Both
 * callbacks must be bounded, nonblocking, and must not reenter this link. */
typedef tp_result (*tp_sub_snapshot)(void *,tp_sub_status *);
typedef enum { TP_SUB_LINK_MASTER, TP_SUB_LINK_SLAVE } tp_sub_link_role;
typedef struct {
    tp_rtu_receiver receiver;
    tp_sub_poll poll;
    void *ctx;
    tp_sub_send send;
    tp_sub_snapshot snapshot;
    tp_sub_link_role role;
    uint32_t now_ms;
    uint32_t last_byte_ms;
    tp_result frame_result;
    bool last_byte_timed;
    bool initialized;
} tp_sub_link;

/* One owner serializes all operations. Times use the same monotonic clock. */
tp_result tp_sub_link_master_init(tp_sub_link *,void *,tp_sub_send,uint32_t baud,uint8_t bits,
                                  uint32_t period_ms,uint32_t timeout_ms,uint32_t freshness_ms,
                                  uint32_t first_token);
tp_result tp_sub_link_slave_init(tp_sub_link *,void *,tp_sub_send,tp_sub_snapshot,
                                 uint32_t baud,uint8_t bits);
tp_result tp_sub_link_byte(tp_sub_link *,uint8_t,uint32_t now_us,uint32_t now_ms);
/* received_ms and now_ms use the same clock, with less than 2^31 ms delay. */
tp_result tp_sub_link_byte_at(tp_sub_link *,uint8_t,uint32_t received_us,
                              uint32_t received_ms,uint32_t now_ms);
tp_result tp_sub_link_error(tp_sub_link *,uint32_t now_us,uint32_t now_ms);
/* Deliver all queued UART bytes before tick, and do not tick with a byte pending. */
tp_result tp_sub_link_tick(tp_sub_link *,uint32_t now_us,uint32_t now_ms);
bool tp_sub_link_healthy(const tp_sub_link *,uint32_t now_ms);
uint16_t tp_sub_link_resource_faults(const tp_sub_link *,uint32_t now_ms);
#endif
