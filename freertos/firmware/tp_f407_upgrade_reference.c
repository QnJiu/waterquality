#include "tp_f407_upgrade_reference.h"
#include "stm32f4xx.h"

bool tp_f407_upgrade_reference_try_lock(void *ctx) {
    tp_f407_upgrade_reference *r=ctx;
    if(!r) return false;
    taskENTER_CRITICAL();
    bool acquired=!r->locked;
    if(acquired) r->locked=true;
    taskEXIT_CRITICAL();
    return acquired;
}
void tp_f407_upgrade_reference_unlock(void *ctx) {
    tp_f407_upgrade_reference *r=ctx;
    if(!r) return;
    taskENTER_CRITICAL(); r->locked=false; taskEXIT_CRITICAL();
}
static bool board_healthy(void *ctx) {
    tp_f407_upgrade_reference *r=ctx;
    return r->options.healthy(r->options.ctx);
}
static void board_finished(void *ctx,tp_result result) {
    tp_f407_upgrade_reference *r=ctx;
    r->options.finished(r->options.ctx,result);
}
tp_result tp_f407_upgrade_reference_start(void *ctx,tp_f407_app *app) {
    tp_f407_upgrade_reference *r=ctx;
    if(!r || !app || r->started || !r->options.healthy || !r->options.finished ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    r->started=true; r->locked=false;
    tp_f407_upgrade_service_options options=r->options;
    options.app=app; options.runtime=&app->runtime;
    options.storage.running_vector=SCB->VTOR;
    options.ctx=r; options.try_lock=tp_f407_upgrade_reference_try_lock;
    options.unlock=tp_f407_upgrade_reference_unlock;
    options.healthy=board_healthy; options.finished=board_finished;
    return tp_f407_upgrade_service_start(&r->service,&options);
}
