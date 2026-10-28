#ifndef TP_SUBORDINATE_NODE_H
#define TP_SUBORDINATE_NODE_H
#include "tp_subordinate_link.h"

typedef struct {
    bool reagent_fault, level_fault, device_fault, leak_fault;
} tp_sub_inputs;
typedef struct {
    void *ctx;
    tp_sub_send send;
    /* One coherent snapshot. Failure or unknown input is reported as device fault. */
    tp_result (*read_inputs)(void *,tp_sub_inputs *);
} tp_sub_node_io;
typedef struct {
    tp_sub_link link;
    tp_sub_node_io io;
    uint32_t droplets;
    bool count_fault;
} tp_sub_node;

/* Single owner, including droplet and error events. Callbacks are nonblocking. */
tp_result tp_sub_node_init(tp_sub_node *,const tp_sub_node_io *,uint32_t baud,uint8_t bits);
/* Read the same fault snapshot used by UART; owner task only. */
tp_result tp_sub_node_snapshot(tp_sub_node *,tp_sub_status *);
void tp_sub_node_droplet(tp_sub_node *);
/* Add a batch of observed edges in constant time; overflow is latched. */
void tp_sub_node_droplets(tp_sub_node *,uint32_t count);
/* Report a lost edge, input queue overflow, or other invalid count. Latched. */
void tp_sub_node_count_error(tp_sub_node *);
tp_result tp_sub_node_byte(tp_sub_node *,uint8_t,uint32_t us,uint32_t ms);
tp_result tp_sub_node_error(tp_sub_node *,uint32_t us,uint32_t ms);
tp_result tp_sub_node_tick(tp_sub_node *,uint32_t us,uint32_t ms);
#endif
