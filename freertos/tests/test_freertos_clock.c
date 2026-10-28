#include <assert.h>
#include <string.h>
#include "tp_freertos_clock.h"
typedef struct { unsigned char data[32]; size_t size; bool full; } fake_queue;
static fake_queue snapshot, requests;
static int critical;
static int64_t device_time=1700000000;
static bool set_fail, read_fail;
void test_enter(void) { critical++; }
void test_exit(void) { assert(critical>0); critical--; }
QueueHandle_t xQueueCreate(unsigned length,size_t size) {
    assert(length==1);
    fake_queue *q=size==sizeof(tp_clock_snapshot)?&snapshot:&requests;
    memset(q,0,sizeof(*q)); q->size=size; return q;
}
void vQueueDelete(QueueHandle_t q) { (void)q; }
BaseType_t xQueueOverwrite(QueueHandle_t handle,const void *value) {
    fake_queue *q=handle; memcpy(q->data,value,q->size); q->full=true; return pdPASS;
}
BaseType_t xQueuePeek(QueueHandle_t handle,void *value,TickType_t wait) {
    fake_queue *q=handle; (void)wait; if(!q->full) return 0;
    memcpy(value,q->data,q->size); return pdPASS;
}
BaseType_t xQueueSend(QueueHandle_t handle,const void *value,TickType_t wait) {
    fake_queue *q=handle; (void)wait; if(q->full) return 0; return xQueueOverwrite(handle,value);
}
BaseType_t xQueueReceive(QueueHandle_t handle,void *value,TickType_t wait) {
    fake_queue *q=handle; if(!xQueuePeek(q,value,wait)) return 0; q->full=false; return pdPASS;
}
BaseType_t xTaskGetSchedulerState(void) { return taskSCHEDULER_NOT_STARTED; }
TickType_t xTaskGetTickCount(void) { return 100; }
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint16_t stack,void *arg,UBaseType_t priority,TaskHandle_t *task) {
    (void)fn; (void)name; (void)stack; (void)arg; (void)priority; *task=(void *)1; return pdPASS;
}
tp_result tp_ds3231_set_utc(tp_ds3231 *rtc,int64_t seconds) {
    (void)rtc; if(set_fail) return TP_IO_ERROR; device_time=seconds; return TP_OK;
}
tp_result tp_ds3231_read_utc(tp_ds3231 *rtc,int64_t *seconds) {
    (void)rtc; if(read_fail) return TP_IO_ERROR; *seconds=device_time; return TP_OK;
}
#include "../os/freertos_clock.c"
int main(void) {
    tp_freertos_clock c; tp_ds3231 rtc; uint32_t requested; int64_t now=0;
    assert(tp_freertos_clock_start(&c,&rtc,1000,3000,128,1)==TP_OK);
    poll_once(&c,false,0); assert(tp_freertos_clock_now(&c,&now) && now==1700000000);
    assert(tp_freertos_clock_set(&c,0)==TP_INVALID);
    assert(tp_freertos_clock_set(&c,1700001234)==TP_OK);
    assert(!tp_freertos_clock_now(&c,&now));
    assert(tp_freertos_clock_set(&c,1700001235)==TP_BUSY);
    assert(xQueueReceive(c.set_requests,&requested,0)==pdPASS);
    poll_once(&c,true,requested);
    assert(tp_freertos_clock_set_status(&c)==3 && tp_freertos_clock_now(&c,&now) && now==1700001234);
    assert(tp_freertos_clock_set(&c,1700004567)==TP_OK);
    assert(xQueueReceive(c.set_requests,&requested,0)==pdPASS); set_fail=true;
    poll_once(&c,true,requested);
    assert(tp_freertos_clock_set_status(&c)==4 && !tp_freertos_clock_now(&c,&now));
    set_fail=false; read_fail=true; poll_once(&c,false,0);
    assert(!tp_freertos_clock_now(&c,&now));
    /* Even a later successful read cannot prove a failed write was complete. */
    read_fail=false; device_time=1700004560; poll_once(&c,false,0);
    assert(tp_freertos_clock_set_status(&c)==4 && !tp_freertos_clock_now(&c,&now));
    assert(tp_freertos_clock_set(&c,1700004567)==TP_OK);
    assert(xQueueReceive(c.set_requests,&requested,0)==pdPASS);
    poll_once(&c,true,requested);
    assert(tp_freertos_clock_set_status(&c)==3 && tp_freertos_clock_now(&c,&now) && now==1700004567);
    assert(critical==0); return 0;
}
