#include "tp_f407_stage_service.h"
#include <assert.h>
static int scheduler, context, starts; static bool locked;
static tp_f407_upgrade_service service; static tp_f407_app app;
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
static bool take(void *p) { assert(p==&context && !locked); locked=true; return true; }
static void release(void *p) { assert(p==&context && locked); locked=false; }
tp_result tp_runtime_request_maintenance(tp_freertos_runtime *rt) {
    assert(rt==&app.runtime); return TP_LOCK_BUSY;
}
static tp_result erase_image(void *p,uint8_t b) { (void)p; (void)b; return TP_OK; }
static tp_result read_image(void *p,uint8_t b,uint32_t off,void *d,size_t n) { (void)p;(void)b;(void)off;(void)d;(void)n;return TP_OK; }
static tp_result write_image(void *p,uint8_t b,uint32_t off,const void *d,size_t n) { (void)p;(void)b;(void)off;(void)d;(void)n;return TP_OK; }
static tp_result read_meta(void *p,unsigned slot,uint32_t off,void *d,size_t n) { (void)p;(void)slot;(void)off;(void)d;(void)n;return TP_OK; }
static tp_result write_meta(void *p,unsigned slot,uint32_t off,const void *d,size_t n) { (void)p;(void)slot;(void)off;(void)d;(void)n;return TP_OK; }
static tp_result erase_meta(void *p,unsigned slot) { (void)p;(void)slot;return TP_OK; }
static tp_result recv(void *p,uint8_t *b) { (void)p;(void)b;return TP_BUSY; }
static tp_result send(void *p,const uint8_t *b,size_t n) { (void)p;(void)b;(void)n;return TP_OK; }
static void done(void *p,tp_result r) { (void)p;(void)r; }
tp_result tp_freertos_stage_transport_start(tp_freertos_stage_transport *t,tp_stage_session *s,
    const tp_stage_transport_io *io,uint32_t frame,uint32_t idle,uint32_t period,uint16_t stack,UBaseType_t priority) {
    (void)t; starts++; assert(s->upgrade==&service.upgrade.secure && s->ctx==&service);
    assert(s->lock(s->ctx) && locked); s->unlock(s->ctx); assert(!locked);
    assert(s->maintenance(s->ctx)==TP_LOCK_BUSY);
    assert(io->receive==recv && io->send==send && io->finished==done);
    assert(frame==30 && idle==1000 && period==5 && stack==1024 && priority==2);
    return TP_OK;
}
int main(void) {
    tp_f407_stage_service stage={0};
    service.app=&app; app.runtime.owner=&app.runtime; service.ctx=&context;
    service.try_lock=take; service.unlock=release; service.upgrade.secure.config.product_id=42;
    tp_f407_stage_options o={.storage={{&context,read_meta,erase_meta,write_meta},&context,erase_image,read_image,write_image,NULL},
        .capacity={4096,4096},.transport={&context,recv,send,done},.frame_timeout_ms=30,
        .idle_timeout_ms=1000,.period_ms=5,.stack_words=1024,.priority=2};
    assert(tp_f407_stage_service_start(NULL,&service,&o)==TP_INVALID);
    scheduler=1; assert(tp_f407_stage_service_start(&stage,&service,&o)==TP_INVALID); scheduler=0;
    o.capacity[0]=0; assert(tp_f407_stage_service_start(&stage,&service,&o)==TP_INVALID && starts==0);
    o.capacity[0]=4096;
    assert(tp_f407_stage_service_start(&stage,&service,&o)==TP_OK && stage.started && starts==1);
    assert(tp_f407_stage_service_start(&stage,&service,&o)==TP_INVALID && starts==1);
    return 0;
}
