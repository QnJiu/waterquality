#include "tp_subordinate_node.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t response[TP_SUB_RESPONSE_SIZE]; size_t length;
    tp_sub_inputs input; bool fail_read, fail_send; unsigned reads;
} fake;
static tp_result send(void *ctx,const uint8_t *data,size_t length) {
    fake *f=ctx;
    if(f->fail_send) return TP_IO_ERROR;
    assert(length==sizeof(f->response)); memcpy(f->response,data,length); f->length=length;
    return TP_OK;
}
static tp_result read_inputs(void *ctx,tp_sub_inputs *inputs) {
    fake *f=ctx; f->reads++;
    if(f->fail_read) return TP_IO_ERROR;
    *inputs=f->input; return TP_OK;
}
static void deliver(tp_sub_node *node,const uint8_t *data,uint32_t start,uint32_t ms) {
    for(size_t i=0;i<TP_SUB_REQUEST_SIZE;i++)
        assert(tp_sub_node_byte(node,data[i],start+(uint32_t)i*100u,ms)==TP_OK);
}
static void cycle(tp_sub_node *node,tp_sub_master *master,fake *f,uint32_t ms) {
    uint8_t request[TP_SUB_REQUEST_SIZE]; f->length=0;
    assert(tp_sub_master_request(master,ms,request)==TP_OK);
    deliver(node,request,ms*1000u,ms);
    assert(tp_sub_node_tick(node,ms*1000u+4000u,ms+4)==TP_OK);
    assert(f->length==TP_SUB_RESPONSE_SIZE);
    assert(tp_sub_master_receive(master,ms+4,f->response,f->length)==TP_OK);
}
int main(void) {
    fake f={0}; tp_sub_node node; tp_sub_master master;
    tp_sub_node_io io={&f,send,read_inputs};
    assert(tp_sub_node_init(&node,&io,115200,10)==TP_OK);
    tp_sub_status direct;
    assert(tp_sub_node_snapshot(NULL,&direct)==TP_INVALID);
    assert(tp_sub_node_snapshot(&node,NULL)==TP_INVALID);
    assert(tp_sub_node_snapshot(&node,&direct)==TP_OK && direct.faults==0);
    assert(tp_sub_master_init(&master,20,100,1)==TP_OK);
    tp_sub_node_droplet(&node); tp_sub_node_droplet(&node);
    cycle(&node,&master,&f,0);
    assert(tp_sub_master_healthy(&master,4) && master.status.droplet_count==2);
    f.input.level_fault=true; f.input.reagent_fault=true; f.input.leak_fault=true;
    assert(tp_sub_node_snapshot(&node,&direct)==TP_OK);
    assert(direct.faults==(TP_SUB_LEVEL_FAULT|TP_SUB_REAGENT_FAULT|TP_SUB_LEAK_FAULT));
    cycle(&node,&master,&f,50);
    assert(master.status.faults==(TP_SUB_LEVEL_FAULT|TP_SUB_REAGENT_FAULT|TP_SUB_LEAK_FAULT));
    assert(tp_sub_master_resource_faults(&master,54)==(TP_PREFLIGHT_LEVEL|TP_PREFLIGHT_REAGENT|TP_PREFLIGHT_LEAK));
    f.fail_read=true;
    assert(tp_sub_node_snapshot(&node,&direct)==TP_OK && direct.faults==TP_SUB_DEVICE_FAULT);
    cycle(&node,&master,&f,100);
    assert(master.status.faults==TP_SUB_DEVICE_FAULT);
    f.fail_read=false; f.input=(tp_sub_inputs){0};
    tp_sub_node_count_error(&node); cycle(&node,&master,&f,150);
    assert(master.status.faults==TP_SUB_DEVICE_FAULT);

    assert(tp_sub_node_init(&node,&io,115200,10)==TP_OK);
    tp_sub_node_droplets(&node,UINT32_MAX-1u);
    assert(node.droplets==UINT32_MAX-1u && !node.count_fault);
    tp_sub_node_droplets(&node,1);
    assert(node.droplets==UINT32_MAX && !node.count_fault);
    tp_sub_node_droplets(&node,2);
    assert(node.droplets==UINT32_MAX && node.count_fault);
    assert(tp_sub_node_init(&node,&io,115200,10)==TP_OK);
    node.droplets=UINT32_MAX; tp_sub_node_droplet(&node);
    cycle(&node,&master,&f,200);
    assert(master.status.droplet_count==UINT32_MAX && master.status.faults==TP_SUB_DEVICE_FAULT);

    uint8_t request[TP_SUB_REQUEST_SIZE];
    assert(tp_sub_master_request(&master,250,request)==TP_OK);
    request[2]^=1; unsigned reads=f.reads; f.length=0;
    deliver(&node,request,250000,250);
    assert(tp_sub_node_tick(&node,254000,254)==TP_INVALID);
    assert(f.reads==reads && f.length==0);
    request[2]^=1; f.fail_send=true;
    deliver(&node,request,300000,300);
    assert(tp_sub_node_tick(&node,304000,304)==TP_IO_ERROR);
    assert(f.length==0);
    return 0;
}
