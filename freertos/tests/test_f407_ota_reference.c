#include "tp_f407_ota_reference.h"
#include <assert.h>

static tp_f407_ota_reference *ref;
static tp_f407_app app;
static int scheduler, calls, stage_context;
static tp_result upgrade_result=TP_OK, stage_result=TP_OK;
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
static void finished(void *ctx,tp_result result) { (void)ctx; (void)result; }
tp_result tp_f407_upgrade_reference_start(void *ctx,tp_f407_app *a) {
    assert(ctx==&ref->upgrade && a==&app && calls==0); calls++;
    return upgrade_result;
}
tp_result tp_f407_stage_reference_start(tp_f407_stage_reference *stage,
    tp_f407_upgrade_service *upgrade,const tp_f407_stage_options *options,
    void *ctx,void (*done)(void *,tp_result)) {
    assert(stage==&ref->stage && upgrade==&ref->upgrade.service && calls==1);
    assert(options==&ref->stage_options && ctx==&stage_context && done==finished);
    calls++; return stage_result;
}
int main(void) {
    tp_f407_ota_reference ota={0}; ref=&ota;
    assert(tp_f407_ota_reference_start(NULL,&app)==TP_INVALID);
    assert(tp_f407_ota_reference_start(&ota,NULL)==TP_INVALID);
    assert(tp_f407_ota_reference_start(&ota,&app)==TP_INVALID);
    ota.stage_finished=finished; ota.stage_ctx=&stage_context;
    scheduler=1; assert(tp_f407_ota_reference_start(&ota,&app)==TP_INVALID && !ota.attempted);
    scheduler=0; upgrade_result=TP_IO_ERROR;
    assert(tp_f407_ota_reference_start(&ota,&app)==TP_IO_ERROR && calls==1);
    assert(tp_f407_ota_reference_start(&ota,&app)==TP_INVALID);
    ota=(tp_f407_ota_reference){0}; ota.stage_finished=finished; ota.stage_ctx=&stage_context;
    calls=0; upgrade_result=TP_OK; stage_result=TP_TIMEOUT;
    assert(tp_f407_ota_reference_start(&ota,&app)==TP_TIMEOUT && calls==2);
    assert(tp_f407_ota_reference_start(&ota,&app)==TP_INVALID);
    ota=(tp_f407_ota_reference){0}; ota.stage_finished=finished; ota.stage_ctx=&stage_context;
    calls=0; stage_result=TP_OK;
    assert(tp_f407_ota_reference_start(&ota,&app)==TP_OK && calls==2);
    return 0;
}
