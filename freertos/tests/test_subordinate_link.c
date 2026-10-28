#include "tp_subordinate_link.h"
#include <assert.h>
#include <string.h>

typedef struct {
    uint8_t bytes[TP_SUB_RESPONSE_SIZE]; size_t len;
    tp_sub_status status; unsigned snapshots; bool fail_send;
} endpoint;
static tp_result send(void *ctx,const uint8_t *data,size_t len) {
    endpoint *e=ctx;
    if(e->fail_send) return TP_IO_ERROR;
    assert(!e->len && len<=sizeof(e->bytes)); memcpy(e->bytes,data,len); e->len=len; return TP_OK;
}
static tp_result snapshot(void *ctx,tp_sub_status *status) {
    endpoint *e=ctx; e->snapshots++; *status=e->status; return TP_OK;
}
static void deliver(endpoint *source,tp_sub_link *dest,uint32_t start_us,uint32_t ms) {
    assert(source->len);
    for(size_t i=0;i<source->len;i++)
        assert(tp_sub_link_byte(dest,source->bytes[i],start_us+(uint32_t)i*100u,ms)==TP_OK);
    source->len=0;
}
int main(void) {
    endpoint m={0},s={0}; tp_sub_link master,slave;
    endpoint noise={0}; tp_sub_link waiting;
    assert(tp_sub_link_master_init(&waiting,&noise,send,115200,10,50,20,100,0)==TP_OK);
    assert(tp_sub_link_byte(&waiting,0x99,0,0)==TP_OK);
    assert(tp_sub_link_tick(&waiting,1000,1)==TP_OK && !noise.len);
    assert(tp_sub_link_tick(&waiting,2000,2)==TP_INVALID && !noise.len);
    assert(tp_sub_link_tick(&waiting,2001,2)==TP_OK && noise.len==TP_SUB_REQUEST_SIZE);
    assert(tp_sub_link_master_init(&master,&m,send,115200,10,50,20,100,42)==TP_OK);
    assert(tp_sub_link_slave_init(&slave,&s,send,snapshot,115200,10)==TP_OK);
    assert(!tp_sub_link_healthy(&master,0));
    assert(tp_sub_link_tick(&master,0,0)==TP_OK && m.len==TP_SUB_REQUEST_SIZE);
    deliver(&m,&slave,100,1);
    assert(tp_sub_link_tick(&slave,4000,4)==TP_OK && s.snapshots==1);
    deliver(&s,&master,4100,5);
    assert(tp_sub_link_tick(&master,8000,8)==TP_OK && tp_sub_link_healthy(&master,8));
    assert(tp_sub_link_error(&master,9000,9)==TP_OK && !tp_sub_link_healthy(&master,9));
    assert(tp_sub_link_tick(&master,50000,50)==TP_OK && m.len==TP_SUB_REQUEST_SIZE);
    m.bytes[2]^=1; deliver(&m,&slave,50100,50);
    assert(tp_sub_link_tick(&slave,54000,54)==TP_INVALID && s.snapshots==1 && !s.len);
    assert(tp_sub_link_tick(&master,70000,70)==TP_OK && !tp_sub_link_healthy(&master,70));
    assert(tp_sub_link_tick(&master,100000,100)==TP_OK);
    s.status.faults=TP_SUB_LEVEL_FAULT; s.status.droplet_count=123;
    deliver(&m,&slave,100100,100); assert(tp_sub_link_tick(&slave,104000,104)==TP_OK);
    deliver(&s,&master,104100,105); assert(tp_sub_link_tick(&master,108000,108)==TP_OK);
    assert(!tp_sub_link_healthy(&master,108));
    assert(tp_sub_link_resource_faults(&master,108)==TP_PREFLIGHT_LEVEL);
    assert(master.poll.master.status.droplet_count==123);
    assert(tp_sub_link_resource_faults(&master,208)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    assert(tp_sub_link_tick(&master,150000,150)==TP_OK);
    s.fail_send=true; deliver(&m,&slave,150100,150);
    assert(tp_sub_link_tick(&slave,154000,154)==TP_IO_ERROR);
    return 0;
}
