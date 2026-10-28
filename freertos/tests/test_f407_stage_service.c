#include "tp_f407_stage_service.h"
#include <assert.h>
#include <string.h>
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
static tp_result recv(void *p,uint8_t *b,uint32_t *ms) { (void)p;(void)b;(void)ms;return TP_BUSY; }
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
static uint8_t fram[2048];
static tp_result bus_write(void *p,uint8_t a,const uint8_t *data,size_t n) {
    (void)p; if(n<1) return TP_INVALID;
    size_t off=((size_t)(a&7)<<8)|data[0]; memcpy(fram+off,data+1,n-1); return TP_OK;
}
static tp_result bus_read(void *p,uint8_t a,const uint8_t *tx,size_t n,uint8_t *out,size_t len) {
    (void)p; if(n!=1) return TP_INVALID;
    size_t off=((size_t)(a&7)<<8)|tx[0]; memcpy(out,fram+off,len); return TP_OK;
}
static uint32_t now(void *p) { (void)p; return 0; }
static void sleep_ms(void *p) { (void)p; }
static void test_storage(void) {
    tp_i2c_io bus={NULL,bus_write,bus_read}; tp_w25q128 flash={.ready=true};
    tp_w25q128_image image; tp_f407_stage_options opt={0};
    assert(tp_upgrade_fram_init(&service.upgrade.fram,&bus)==TP_OK);
    service.upgrade.running_bank=1;
    assert(tp_f407_stage_storage_init(&opt,&service,&image,&flash,NULL,now,sleep_ms,0,0,4096,10,10)==TP_INVALID);
    assert(tp_f407_stage_storage_init(&opt,&service,&image,&flash,NULL,now,sleep_ms,0,4096,TP_F407_IMAGE_BANK_SIZE+4096,10,10)==TP_INVALID);
    scheduler=1;
    assert(tp_f407_stage_storage_init(&opt,&service,&image,&flash,NULL,now,sleep_ms,0,4096,4096,10,10)==TP_INVALID);
    scheduler=0;
    assert(tp_f407_stage_storage_init(&opt,&service,&image,&flash,NULL,now,sleep_ms,0,4096,4096,10,10)==TP_OK);
    assert(opt.capacity[0]==4096 && opt.capacity[1]==4096 && opt.storage.image_ctx==&image);
    assert(opt.storage.metadata.ctx==&service.upgrade.fram.staging);
    assert(opt.storage.erase_bank==tp_w25q128_image_erase && opt.storage.read_bank==tp_w25q128_image_read && opt.storage.write_bank==tp_w25q128_image_write);
    assert(opt.storage.erase_bank(opt.storage.image_ctx,1)==TP_INVALID);
    memset(fram,0xaa,sizeof(fram)); uint8_t v=0x53, out=0;
    assert(opt.storage.metadata.write(opt.storage.metadata.ctx,1,5,&v,1)==TP_OK);
    assert(fram[768+128+5]==v && fram[512+128+5]==0xaa);
    assert(opt.storage.metadata.read(opt.storage.metadata.ctx,1,5,&out,1)==TP_OK && out==v);
}
int main(void) {
    tp_f407_stage_service stage={0};
    service.app=&app; app.runtime.owner=&app.runtime; service.ctx=&context;
    service.try_lock=take; service.unlock=release; service.upgrade.secure.config.product_id=42;
    test_storage();
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
