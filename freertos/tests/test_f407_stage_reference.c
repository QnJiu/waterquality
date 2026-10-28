#include <assert.h>
#include "tp_f407_stage_reference.h"
static int step,fail_at,scheduler,ctx;
static tp_f407_stage_reference *ref;
static tp_f407_upgrade_service *up;
static tp_result next(int n) { assert(++step==n); return fail_at==n ? TP_IO_ERROR : TP_OK; }
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
uint32_t tp_freertos_flash_now_ms(void *p) { (void)p; return 0; }
void tp_freertos_flash_sleep_ms(void *p) { (void)p; }
tp_result tp_f407_w25q128_spi_init(tp_f407_w25q128_spi *p) { assert(p==&ref->spi); return next(1); }
tp_w25q128_io tp_f407_w25q128_spi_io(tp_f407_w25q128_spi *p) { assert(p==&ref->spi); return (tp_w25q128_io){p,0}; }
tp_result tp_w25q128_init(tp_w25q128 *p,const tp_w25q128_io *io) { assert(p==&ref->flash && io->ctx==&ref->spi); return next(2); }
tp_result tp_f407_stage_storage_init(tp_f407_stage_options *o,tp_f407_upgrade_service *s,
    tp_w25q128_image *i,tp_w25q128 *f,void *c,uint32_t (*now)(void *),void (*sleep)(void *),
    uint32_t base0,uint32_t base1,uint32_t size,uint32_t pt,uint32_t et) {
    assert(s==up && i==&ref->image && f==&ref->flash && !c);
    assert(now==tp_freertos_flash_now_ms && sleep==tp_freertos_flash_sleep_ms);
    assert(base0==0 && base1==TP_F407_IMAGE_BANK_SIZE && size==base1 && pt==100 && et==3000);
    o->capacity[0]=size; return next(3);
}
static void done(void *p,tp_result result) { (void)p; (void)result; }
tp_result tp_f407_stage_uart_init(tp_f407_stage_uart *u,tp_stage_transport_io *io,
                                  void *p,void (*finished)(void *,tp_result)) {
    assert(u==&ref->uart && p==&ctx && finished==done); io->ctx=u; return next(4);
}
tp_result tp_f407_stage_service_start(tp_f407_stage_service *s,tp_f407_upgrade_service *u,
                                       const tp_f407_stage_options *o) {
    assert(s==&ref->service && u==up && o->capacity[0]==TP_F407_IMAGE_BANK_SIZE);
    assert(o->transport.ctx==&ref->uart && o->frame_timeout_ms==40 && o->stack_words==1024);
    return next(5);
}
int main(void) {
    tp_f407_stage_reference r; tp_f407_upgrade_service u; tp_f407_stage_options settings={0};
    ref=&r; up=&u; settings.frame_timeout_ms=40; settings.stack_words=1024;
    for(fail_at=1;fail_at<=5;fail_at++) {
        r=(tp_f407_stage_reference){0}; step=0;
        assert(tp_f407_stage_reference_start(&r,&u,&settings,&ctx,done)==TP_IO_ERROR);
        assert(step==fail_at && r.attempted);
        assert(tp_f407_stage_reference_start(&r,&u,&settings,&ctx,done)==TP_INVALID);
    }
    r=(tp_f407_stage_reference){0}; step=0; fail_at=0;
    scheduler=1; assert(tp_f407_stage_reference_start(&r,&u,&settings,&ctx,done)==TP_INVALID && !r.attempted);
    scheduler=0; assert(tp_f407_stage_reference_start(&r,&u,&settings,&ctx,done)==TP_OK && step==5);
    assert(settings.capacity[0]==0 && settings.transport.ctx==NULL);
    return 0;
}
