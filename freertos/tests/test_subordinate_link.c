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

    /* A delayed queue must not extend the validity of a received status. */
    endpoint delayed={0}; tp_sub_link timed;
    tp_sub_status ok={0}; uint8_t reply[TP_SUB_RESPONSE_SIZE];
    assert(tp_sub_link_master_init(&timed,&delayed,send,115200,10,50,20,10,7)==TP_OK);
    assert(tp_sub_link_tick(&timed,0,0)==TP_OK);
    assert(tp_sub_reply(delayed.bytes,delayed.len,&ok,reply)==TP_OK); delayed.len=0;
    for(size_t i=0;i<sizeof(reply);i++)
        assert(tp_sub_link_byte_at(&timed,reply[i],100+(uint32_t)i*100,2,8)==TP_OK);
    assert(tp_sub_link_tick(&timed,4000,8)==TP_OK);
    assert(tp_sub_link_healthy(&timed,11));
    assert(!tp_sub_link_healthy(&timed,12));

    /* The response arrived before its deadline, despite delayed processing. */
    endpoint queued={0}; tp_sub_link late_processing;
    assert(tp_sub_link_master_init(&late_processing,&queued,send,115200,10,50,20,100,8)==TP_OK);
    assert(tp_sub_link_tick(&late_processing,0,0)==TP_OK);
    assert(tp_sub_reply(queued.bytes,queued.len,&ok,reply)==TP_OK); queued.len=0;
    for(size_t i=0;i<sizeof(reply);i++)
        assert(tp_sub_link_byte_at(&late_processing,reply[i],100+(uint32_t)i*100,10,25)==TP_OK);
    assert(tp_sub_link_tick(&late_processing,4000,25)==TP_OK);
    assert(tp_sub_link_healthy(&late_processing,25));
    assert(!tp_sub_link_healthy(&late_processing,110));

    /* A later byte closes the previous frame; retain the previous timestamp. */
    assert(tp_sub_link_tick(&timed,50000,50)==TP_OK);
    assert(tp_sub_reply(delayed.bytes,delayed.len,&ok,reply)==TP_OK); delayed.len=0;
    for(size_t i=0;i<sizeof(reply);i++)
        assert(tp_sub_link_byte_at(&timed,reply[i],50100+(uint32_t)i*100,52,58)==TP_OK);
    assert(tp_sub_link_byte_at(&timed,0xff,55000,55,58)==TP_OK);
    assert(tp_sub_link_healthy(&timed,61));
    assert(!tp_sub_link_healthy(&timed,62));
    assert(tp_sub_link_byte_at(&timed,0,56000,70,69)==TP_INVALID);

    endpoint wrapped={0}; tp_sub_link wrap;
    assert(tp_sub_link_master_init(&wrap,&wrapped,send,115200,10,50,20,10,9)==TP_OK);
    assert(tp_sub_link_tick(&wrap,0,0xfffffffcu)==TP_OK);
    assert(tp_sub_reply(wrapped.bytes,wrapped.len,&ok,reply)==TP_OK); wrapped.len=0;
    for(size_t i=0;i<sizeof(reply);i++)
        assert(tp_sub_link_byte_at(&wrap,reply[i],100+(uint32_t)i*100,0xfffffffeu,2)==TP_OK);
    assert(tp_sub_link_tick(&wrap,4000,2)==TP_OK);
    assert(tp_sub_link_healthy(&wrap,7));
    assert(!tp_sub_link_healthy(&wrap,8));

    endpoint stale={0}; tp_sub_link expired;
    assert(tp_sub_link_master_init(&expired,&stale,send,115200,10,50,20,5,10)==TP_OK);
    assert(tp_sub_link_tick(&expired,0,0)==TP_OK);
    assert(tp_sub_reply(stale.bytes,stale.len,&ok,reply)==TP_OK); stale.len=0;
    for(size_t i=0;i<sizeof(reply);i++)
        assert(tp_sub_link_byte_at(&expired,reply[i],100+(uint32_t)i*100,1,10)==TP_OK);
    assert(tp_sub_link_tick(&expired,4000,10)==TP_OK);
    assert(!tp_sub_link_healthy(&expired,10));
    return 0;
}
