#include "tp_f407_upgrade_reference.h"
#include "stm32f4xx.h"
#include <assert.h>

SCB_Type fake_scb;
static tp_f407_app app;
static tp_f407_upgrade_reference ref, failed_ref;
static tp_f407_upgrade_reference *expected=&ref;
static tp_result service_result=TP_OK;
static int scheduler, board_context, finished, called;
static bool ok=true;
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
static bool health(void *ctx) { assert(ctx==&board_context); return ok; }
static void done(void *ctx,tp_result result) {
    assert(ctx==&board_context && result==TP_TIMEOUT); finished++;
}
tp_result tp_f407_upgrade_service_start(tp_f407_upgrade_service *s,const tp_f407_upgrade_service_options *o) {
    assert(s==&expected->service && o->app==&app && o->runtime==&app.runtime);
    assert(o->storage.running_vector==TP_F407_IMAGE1_BASE && o->storage.product_id==42);
    assert(o->ctx==expected && o->try_lock(o->ctx));
    assert(!o->try_lock(o->ctx)); o->unlock(o->ctx);
    assert(o->try_lock(o->ctx)); o->unlock(o->ctx);
    ok=true; assert(o->healthy(o->ctx)); ok=false; assert(!o->healthy(o->ctx));
    o->finished(o->ctx,TP_TIMEOUT); called++; return service_result;
}
int main(void) {
    fake_scb.VTOR=TP_F407_IMAGE1_BASE;
    ref.options.storage.product_id=42;
    ref.options.ctx=&board_context; ref.options.healthy=health; ref.options.finished=done;
    assert(tp_f407_upgrade_reference_start(NULL,&app)==TP_INVALID);
    scheduler=1; assert(tp_f407_upgrade_reference_start(&ref,&app)==TP_INVALID);
    scheduler=0;
    assert(tp_f407_upgrade_reference_start(&ref,&app)==TP_OK);
    assert(called==1 && finished==1);
    assert(tp_f407_upgrade_reference_start(&ref,&app)==TP_INVALID);
    expected=&failed_ref; failed_ref.options=ref.options; service_result=TP_TIMEOUT;
    assert(tp_f407_upgrade_reference_start(&failed_ref,&app)==TP_TIMEOUT);
    assert(tp_f407_upgrade_reference_start(&failed_ref,&app)==TP_INVALID);
    assert(called==2 && finished==2);
    assert(!tp_f407_upgrade_reference_try_lock(NULL));
    return 0;
}
