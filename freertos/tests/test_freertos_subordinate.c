#include "tp_freertos_subordinate.h"
#include <assert.h>
#include <string.h>

static tp_sub_master queue_data;
static bool has_data;
static TickType_t clock_ms;
static uint8_t request[TP_SUB_REQUEST_SIZE];
QueueHandle_t xQueueCreate(unsigned length,size_t size) {
    assert(length==1 && size==sizeof(queue_data)); has_data=false; return &queue_data;
}
BaseType_t xQueueOverwrite(QueueHandle_t q,const void *data) {
    assert(q==&queue_data); memcpy(q,data,sizeof(queue_data)); has_data=true; return pdPASS;
}
BaseType_t xQueuePeek(QueueHandle_t q,void *out,TickType_t wait) {
    assert(q==&queue_data && wait==0);
    if(!has_data) return 0;
    memcpy(out,q,sizeof(queue_data)); return pdPASS;
}
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_NOT_STARTED; }
TickType_t xTaskGetTickCount(void) { return clock_ms; }
static tp_result send(void *ctx,const uint8_t *data,size_t n) {
    (void)ctx; assert(n==sizeof(request)); memcpy(request,data,n); return TP_OK;
}
int main(void) {
    tp_freertos_subordinate s;
    assert(tp_freertos_subordinate_init(&s,NULL,send,115200,10,50,20,100,9)==TP_OK);
    assert(!tp_freertos_subordinate_healthy(&s));
    assert(tp_freertos_subordinate_resource_faults(&s)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    assert(tp_freertos_subordinate_tick(&s,0,0)==TP_OK);
    tp_sub_status status={.droplet_count=12,.faults=0}; uint8_t reply[TP_SUB_RESPONSE_SIZE];
    assert(tp_sub_reply(request,sizeof(request),&status,reply)==TP_OK);
    for(size_t i=0;i<sizeof(reply);i++)
        assert(tp_freertos_subordinate_byte(&s,reply[i],100+(uint32_t)i*100,1)==TP_OK);
    clock_ms=4; assert(tp_freertos_subordinate_tick(&s,4000,clock_ms)==TP_OK);
    assert(tp_freertos_subordinate_healthy(&s));
    assert(tp_freertos_subordinate_resource_faults(&s)==0);
    clock_ms=104; assert(!tp_freertos_subordinate_healthy(&s));
    assert(tp_freertos_subordinate_resource_faults(&s)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    clock_ms=105; assert(tp_freertos_subordinate_tick(&s,105000,clock_ms)==TP_OK);
    status.faults=TP_SUB_LEVEL_FAULT;
    assert(tp_sub_reply(request,sizeof(request),&status,reply)==TP_OK);
    for(size_t i=0;i<sizeof(reply);i++)
        assert(tp_freertos_subordinate_byte(&s,reply[i],105100+(uint32_t)i*100,105)==TP_OK);
    clock_ms=109; assert(tp_freertos_subordinate_tick(&s,109000,clock_ms)==TP_OK);
    assert(!tp_freertos_subordinate_healthy(&s));
    assert(tp_freertos_subordinate_resource_faults(&s)==TP_PREFLIGHT_LEVEL);
    assert(tp_freertos_subordinate_error(&s,110000,110)==TP_OK);
    assert(tp_freertos_subordinate_resource_faults(&s)==TP_PREFLIGHT_RESOURCE_UNKNOWN);
    clock_ms=155; assert(tp_freertos_subordinate_tick(&s,155000,clock_ms)==TP_OK);
    status.faults=0;
    assert(tp_sub_reply(request,sizeof(request),&status,reply)==TP_OK);
    /* The bytes arrived in time, but the task processes them after the deadline. */
    clock_ms=180;
    for(size_t i=0;i<sizeof(reply);i++)
        assert(tp_freertos_subordinate_byte_at(&s,reply[i],155100+(uint32_t)i*100,156,clock_ms)==TP_OK);
    assert(tp_freertos_subordinate_tick(&s,180000,clock_ms)==TP_OK);
    assert(tp_freertos_subordinate_healthy(&s));
    assert(tp_freertos_subordinate_resource_faults(&s)==0);
    clock_ms=255; assert(tp_freertos_subordinate_healthy(&s));
    clock_ms=256; assert(!tp_freertos_subordinate_healthy(&s));
    return 0;
}
