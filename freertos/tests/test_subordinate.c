#include "tp_subordinate.h"
#include <assert.h>
#include <string.h>

int main(void) {
    tp_sub_master m; uint8_t request[TP_SUB_REQUEST_SIZE], response[TP_SUB_RESPONSE_SIZE], old[TP_SUB_RESPONSE_SIZE];
    tp_sub_status status={123,0};
    assert(tp_sub_master_init(&m,0,100,1)==TP_INVALID);
    assert(tp_sub_master_init(&m,20,100,0xffffffffu)==TP_OK);
    assert(!tp_sub_master_healthy(&m,0));
    assert(tp_sub_master_resource_faults(&m,0)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    assert(tp_sub_master_request(&m,0xfffffff0u,request)==TP_OK);
    assert(tp_sub_master_request(&m,0xfffffff1u,request)==TP_BUSY);
    assert(tp_sub_reply(request,sizeof(request),&status,response)==TP_OK);
    memcpy(old,response,sizeof(old)); response[9]^=1;
    assert(tp_sub_master_receive(&m,0xfffffff2u,response,sizeof(response))==TP_INVALID);
    assert(!tp_sub_master_healthy(&m,0xfffffff2u));
    assert(tp_sub_master_receive(&m,0xfffffff3u,old,sizeof(old))==TP_OK);
    assert(m.status.droplet_count==123 && tp_sub_master_healthy(&m,0xfffffff3u));
    assert(tp_sub_master_receive(&m,0xfffffff4u,old,sizeof(old))==TP_INVALID);
    assert(tp_sub_master_request(&m,0xfffffff4u,request)==TP_OK);
    assert(tp_sub_master_receive(&m,0xfffffff5u,old,sizeof(old))==TP_INVALID);
    status.faults=TP_SUB_REAGENT_FAULT|TP_SUB_LEVEL_FAULT;
    assert(tp_sub_reply(request,sizeof(request),&status,response)==TP_OK);
    assert(tp_sub_master_receive(&m,0xfffffff6u,response,sizeof(response))==TP_OK);
    assert(!tp_sub_master_healthy(&m,0xfffffff6u));
    assert(tp_sub_master_resource_faults(&m,0xfffffff6u)==(TP_PREFLIGHT_REAGENT|TP_PREFLIGHT_LEVEL));
    assert(tp_sub_master_request(&m,0xfffffff7u,request)==TP_OK);
    status.faults=TP_SUB_DEVICE_FAULT;
    assert(tp_sub_reply(request,sizeof(request),&status,response)==TP_OK);
    assert(tp_sub_master_receive(&m,0xfffffff8u,response,sizeof(response))==TP_OK);
    assert(tp_sub_master_resource_faults(&m,0xfffffff8u)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    assert(tp_sub_master_request(&m,0xfffffff9u,request)==TP_OK);
    status.faults=0; assert(tp_sub_reply(request,sizeof(request),&status,response)==TP_OK);
    assert(tp_sub_master_receive(&m,13,response,sizeof(response))==TP_INVALID);
    assert(tp_sub_master_request(&m,13,request)==TP_OK);
    assert(tp_sub_master_receive(&m,14,response,sizeof(response))==TP_INVALID);
    tp_sub_master_tick(&m,93);
    assert(!tp_sub_master_healthy(&m,93));
    assert(tp_sub_master_resource_faults(&m,93)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    request[0]^=1; assert(tp_sub_reply(request,sizeof(request),&status,response)==TP_INVALID);
    status.faults=0x80; assert(tp_sub_reply(request,sizeof(request),&status,response)==TP_INVALID);
    return 0;
}
