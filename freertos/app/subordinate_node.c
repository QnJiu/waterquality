#include "tp_subordinate_node.h"
#include <string.h>

static tp_result send(void *ctx,const uint8_t *data,size_t length) {
    tp_sub_node *n=ctx;
    return n->io.send(n->io.ctx,data,length);
}
static tp_result snapshot(void *ctx,tp_sub_status *status) {
    tp_sub_node *n=ctx; tp_sub_inputs input={0};
    tp_result r=n->io.read_inputs(n->io.ctx,&input);
    status->droplet_count=n->droplets;
    status->faults=0;
    if(r!=TP_OK || n->count_fault || input.device_fault) status->faults|=TP_SUB_DEVICE_FAULT;
    if(r==TP_OK && input.reagent_fault) status->faults|=TP_SUB_REAGENT_FAULT;
    if(r==TP_OK && input.level_fault) status->faults|=TP_SUB_LEVEL_FAULT;
    return TP_OK;
}
tp_result tp_sub_node_init(tp_sub_node *n,const tp_sub_node_io *io,uint32_t baud,uint8_t bits) {
    if(!n || !io || !io->send || !io->read_inputs) return TP_INVALID;
    tp_sub_node_io copy=*io;
    memset(n,0,sizeof(*n)); n->io=copy;
    tp_result r=tp_sub_link_slave_init(&n->link,n,send,snapshot,baud,bits);
    if(r!=TP_OK) memset(n,0,sizeof(*n));
    return r;
}
void tp_sub_node_droplet(tp_sub_node *n) {
    tp_sub_node_droplets(n,1);
}
void tp_sub_node_droplets(tp_sub_node *n,uint32_t count) {
    if(!n || !n->link.initialized) return;
    if(count>UINT32_MAX-n->droplets) { n->droplets=UINT32_MAX; n->count_fault=true; }
    else n->droplets+=count;
}
void tp_sub_node_count_error(tp_sub_node *n) {
    if(n && n->link.initialized) n->count_fault=true;
}
tp_result tp_sub_node_byte(tp_sub_node *n,uint8_t b,uint32_t us,uint32_t ms) {
    return n ? tp_sub_link_byte(&n->link,b,us,ms) : TP_INVALID;
}
tp_result tp_sub_node_error(tp_sub_node *n,uint32_t us,uint32_t ms) {
    return n ? tp_sub_link_error(&n->link,us,ms) : TP_INVALID;
}
tp_result tp_sub_node_tick(tp_sub_node *n,uint32_t us,uint32_t ms) {
    return n ? tp_sub_link_tick(&n->link,us,ms) : TP_INVALID;
}
