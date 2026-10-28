#include <assert.h>
#include "../os/freertos_runtime.c"
static int critical, scheduler, notifications, safe_calls;
static tp_event captured[4]; static unsigned event_count;
static void record(void *ctx,const tp_event *event) { (void)ctx; assert(event_count<4); captured[event_count++]=*event; }
static tp_freertos_runtime *active;
void test_enter(void) { assert(!critical); critical=1; }
void test_exit(void) { assert(critical); critical=0; }
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
TickType_t xTaskGetTickCount(void) { return 0; }
void xTaskNotifyGive(TaskHandle_t t) { assert(t && !critical); notifications++; }
uint32_t ulTaskNotifyTake(BaseType_t a,TickType_t b) { (void)a;(void)b;return 0; }
BaseType_t xTaskCreate(void (*f)(void *),const char *n,uint16_t s,void *a,UBaseType_t p,TaskHandle_t *t) { (void)f;(void)n;(void)s;(void)a;(void)p;(void)t;return 0; }
void vTaskDelete(TaskHandle_t t) { (void)t; }
QueueHandle_t xQueueCreate(unsigned n,size_t s) { (void)n;(void)s;return NULL; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
BaseType_t xQueueReset(QueueHandle_t q) { (void)q;return pdPASS; }
BaseType_t xQueueReceive(QueueHandle_t q,void *v,TickType_t w) { (void)q;(void)v;(void)w;return 0; }
BaseType_t xQueueSend(QueueHandle_t q,const void *v,TickType_t w) { (void)q;(void)v;(void)w;return 0; }
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { (void)q;return 0; }
static void safe(void *p) {
    (void)p; assert(!critical); safe_calls++;
    /* Even concurrent retries must not observe confirmation until safe returns. */
    assert(tp_runtime_request_maintenance(active)==TP_LOCK_BUSY);
}
int main(void) {
    tp_controller c={0}; tp_service s={0}; tp_freertos_runtime rt={0};
    c.state=TP_IDLE; c.hal.safe_outputs=safe; s.controller=&c; rt.service=&s; rt.owner=&rt; active=&rt;
    assert(tp_runtime_request_maintenance(NULL)==TP_INVALID);
    assert(tp_runtime_request_maintenance(&rt)==TP_INVALID && notifications==0);
    scheduler=taskSCHEDULER_RUNNING;
    assert(tp_runtime_request_maintenance(&rt)==TP_LOCK_BUSY && notifications==1);
    process_maintenance(&rt);
    assert(safe_calls==1 && s.maintenance && tp_runtime_request_maintenance(&rt)==TP_OK);
    process_maintenance(&rt); assert(safe_calls==1);
    rt.maintenance_state=MAINTENANCE_NONE; s.maintenance=false;
    c.state=TP_RUNNING; s.standard_attached=true; s.auto_standard.controller=&c;
    s.auto_standard.state=TP_STANDARD_RUNNING; rt.io.event=record;
    assert(tp_runtime_request_maintenance(&rt)==TP_LOCK_BUSY);
    process_maintenance(&rt);
    assert(event_count==2 && captured[0].type==TP_EVENT_FAILED);
    assert(captured[1].type==TP_EVENT_AUTO_STANDARD && captured[1].detail==5);
    assert(s.auto_standard.state==TP_STANDARD_CANCELLED);
    rt.maintenance_state=MAINTENANCE_NONE; s.maintenance=false; c.hal.safe_outputs=NULL;
    assert(tp_runtime_request_maintenance(&rt)==TP_LOCK_BUSY);
    process_maintenance(&rt);
    assert(tp_runtime_request_maintenance(&rt)==TP_INVALID);
    return 0;
}
