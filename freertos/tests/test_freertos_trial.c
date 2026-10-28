#include "tp_freertos_trial.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t slots[2][128]; bool locked, healthy, fail_read; unsigned reads; } fake;
static int scheduler, create_ok=1;
static tp_result rd(void *p,unsigned slot,uint32_t off,void *out,size_t n) {
    fake *f=p; f->reads++; if(f->fail_read) return TP_IO_ERROR;
    assert(slot<2 && off+n<=128); memcpy(out,f->slots[slot]+off,n); return TP_OK;
}
static tp_result erase(void *p,unsigned slot) { fake *f=p; memset(f->slots[slot],0xff,128); return TP_OK; }
static tp_result wr(void *p,unsigned slot,uint32_t off,const void *in,size_t n) { fake *f=p; memcpy(f->slots[slot]+off,in,n); return TP_OK; }
static tp_result verify(void *p,uint8_t bank,uint32_t v) { (void)p; return ((bank==0 && v==1)||(bank==1 && v==2)) ? TP_OK : TP_INVALID; }
static bool lock(void *p) { return !((fake *)p)->locked; }
static void unlock(void *p) { (void)p; }
static bool healthy(void *p) { return ((fake *)p)->healthy; }
static void finished(void *p,tp_result r) { (void)p; (void)r; }
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
TickType_t xTaskGetTickCount(void) { return 0; }
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint16_t stack,void *arg,UBaseType_t prio,TaskHandle_t *task) {
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio; *task=(void *)1; return create_ok ? pdPASS : 0;
}
void vTaskDelete(TaskHandle_t t) { (void)t; }
void vTaskDelay(TickType_t ticks) { (void)ticks; }
static void setup(fake *f,tp_boot_io *boot) {
    memset(f,0,sizeof(*f)); memset(f->slots,0xff,sizeof(f->slots)); f->healthy=true;
    *boot=(tp_boot_io){{f,rd,erase,wr},f,verify};
    assert(tp_boot_provision(boot,0,1)==TP_OK); assert(tp_boot_stage(boot,1,2)==TP_OK);
    uint8_t bank; assert(tp_boot_select(boot,&bank)==TP_OK && bank==1);
}
static tp_result start(tp_freertos_trial *t,tp_boot_io *b,fake *f,uint8_t bank) {
    return tp_freertos_trial_start(t,b,bank,100,40,20,300,256,1,f,lock,unlock,healthy,finished);
}
int main(void) {
    fake f; tp_boot_io b; tp_freertos_trial t; tp_boot_state state;
    setup(&f,&b);
    scheduler=1; assert(start(&t,&b,&f,1)==TP_INVALID); scheduler=0;
    create_ok=0; assert(start(&t,&b,&f,1)==TP_IO_ERROR); create_ok=1;
    assert(tp_freertos_trial_start(&t,&b,1,100,40,41,300,256,1,&f,lock,unlock,healthy,finished)==TP_INVALID);
    assert(start(&t,&b,&f,1)==TP_OK);
    assert(tp_freertos_trial_step(&t,0)==TP_BUSY);
    unsigned reads=f.reads; f.locked=true; assert(tp_freertos_trial_step(&t,20)==TP_BUSY); assert(f.reads==reads); f.locked=false;
    assert(tp_freertos_trial_step(&t,40)==TP_BUSY); assert(tp_freertos_trial_step(&t,60)==TP_BUSY);
    f.healthy=false; assert(tp_freertos_trial_step(&t,80)==TP_BUSY); f.healthy=true;
    assert(tp_freertos_trial_step(&t,100)==TP_BUSY); f.fail_read=true;
    assert(tp_freertos_trial_step(&t,120)==TP_IO_ERROR); f.fail_read=false;
    for(uint32_t now=140;now<240;now+=20) assert(tp_freertos_trial_step(&t,now)==TP_BUSY);
    assert(tp_freertos_trial_step(&t,240)==TP_OK && t.done);
    assert(tp_boot_read(&b,&state)==TP_OK && state.phase==TP_BOOT_IDLE && state.active_bank==1);
    setup(&f,&b); assert(start(&t,&b,&f,1)==TP_OK); f.locked=true;
    assert(tp_freertos_trial_step(&t,0)==TP_BUSY); assert(tp_freertos_trial_step(&t,300)==TP_TIMEOUT && t.done);
    assert(tp_boot_read(&b,&state)==TP_OK && state.phase==TP_BOOT_TRIAL);
    setup(&f,&b); assert(start(&t,&b,&f,0)==TP_OK);
    assert(tp_freertos_trial_step(&t,0)==TP_INVALID && t.done);
    return 0;
}
