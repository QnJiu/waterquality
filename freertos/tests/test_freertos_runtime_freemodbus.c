#include <assert.h>
#include <string.h>
#include "../os/freertos_runtime.c"
static int critical, scheduler, notifications;
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

static int safe_count, sent_count, event_count;
static uint16_t faults;
static tp_event last_event;
static tp_result preflight(void *ctx, uint16_t *out) {
    (void)ctx; *out=faults; return faults ? TP_IO_ERROR : TP_OK;
}
static void event(void *ctx, const tp_event *e) { (void)ctx; event_count++; last_event=*e; }
static unsigned sent_port;
static uint8_t sent[256];
static size_t sent_len;
static void safe(void *ctx) { (void)ctx; safe_count++; }
static tp_result send_frame(void *ctx, unsigned port, const uint8_t *data, size_t n) {
    (void)ctx; sent_count++; sent_port=port; sent_len=n; memcpy(sent,data,n); return TP_OK;
}
static void seal(tp_runtime_frame *f) {
    uint16_t crc=crc16(f->bytes,f->length-2);
    f->bytes[f->length-2]=(uint8_t)crc; f->bytes[f->length-1]=(uint8_t)(crc>>8);
}
int main(void) {
    tp_controller c={0}; tp_service s; tp_step recipe[]={ {.kind=TP_STEP_END} };
    tp_freertos_runtime rt={0}; tp_runtime_frame f={0};
    assert(tp_service_init(&s,&c,recipe,1,0)==TP_OK);
    c.hal.safe_outputs=safe; c.state=TP_RUNNING; rt.service=&s; rt.io.send=send_frame;
    rt.ports[0]=(tp_modbus){&s,1,tp_service_read_holding,tp_service_write_holding};
    rt.ports[1]=(tp_modbus){&s,2,tp_service_read_holding,tp_service_write_holding};
    f.port=1; f.length=8; uint8_t q[]={2,6,0,0,0,2,0,0}; memcpy(f.bytes,q,sizeof(q)); seal(&f);
    assert(is_stop(&rt,&f)); process(&rt,&f);
    assert(c.state==TP_FAILED && c.fault==TP_FAULT_STOP && safe_count==1);
    assert(sent_count==1 && sent_port==1 && sent_len==8 && memcmp(sent,f.bytes,8)==0);
    f.bytes[0]=1; seal(&f); assert(!is_stop(&rt,&f));
    f.bytes[0]=2; f.bytes[7]^=1; assert(!is_stop(&rt,&f));
    uint8_t multi[]={2,16,0,0,0,1,2,0,2,0,0}; f.length=sizeof(multi);
    memcpy(f.bytes,multi,sizeof(multi)); seal(&f); assert(is_stop(&rt,&f));
    c.state=TP_RUNNING; process(&rt,&f);
    assert(c.state==TP_FAILED && safe_count==2 && sent_count==2 && sent_port==1 && sent_len==8);
    c.state=TP_FAILED; rt.io.event=event;
    assert(tp_service_attach_preflight(&s,NULL,preflight)==TP_OK);
    uint8_t check[]={2,6,0,0,0,10,0,0}; f.length=sizeof(check);
    memcpy(f.bytes,check,sizeof(check)); seal(&f);
    faults=TP_PREFLIGHT_ADC; process(&rt,&f);
    assert(event_count==1 && last_event.type==TP_EVENT_PREFLIGHT && last_event.detail==TP_PREFLIGHT_ADC);
    process(&rt,&f); assert(event_count==1);
    faults=0; process(&rt,&f);
    assert(event_count==2 && last_event.type==TP_EVENT_PREFLIGHT && last_event.detail==0);
    /* A scheduled preflight failure invalidates the old result and reports
     * its changed fault through the same event path used by the control task. */
    c.state=TP_COMPLETE; c.result_valid=true; c.result_mg_l=1.5f;
    assert(tp_schedule_configure(&s.schedule,TP_MODE_PERIODIC,1000,0,0,0,false)==TP_OK);
    faults=TP_PREFLIGHT_LEVEL;
    tick_service(&rt,1000,false,0);
    assert(c.state==TP_COMPLETE && !c.result_valid && s.last_start_result==TP_IO_ERROR);
    assert(event_count==3 && last_event.type==TP_EVENT_PREFLIGHT && last_event.detail==TP_PREFLIGHT_LEVEL);
    uint16_t valid=1;
    assert(tp_service_read_holding(&s,16,1,&valid)==TP_OK && valid==0);
    tick_service(&rt,2000,false,0);
    assert(event_count==3); /* Unchanged fault does not flood the log. */
    return 0;
}
