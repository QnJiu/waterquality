#include "tp_f407_upgrade_service.h"
#include <string.h>

static tp_result request_maintenance(void *ctx) {
    return tp_runtime_request_maintenance((tp_freertos_runtime *)ctx);
}
static bool trial_lock(void *ctx) {
    tp_f407_upgrade_service *s=ctx; return s->try_lock(s->ctx);
}
static void trial_unlock(void *ctx) {
    tp_f407_upgrade_service *s=ctx; s->unlock(s->ctx);
}
static bool trial_healthy(void *ctx) {
    tp_f407_upgrade_service *s=ctx;
    /* Both callbacks consume latched faults, including when either is false. */
    bool app=tp_f407_app_trial_healthy(s->app);
    bool board=s->healthy(s->ctx);
    return app && board;
}
static void trial_finished(void *ctx,tp_result result) {
    tp_f407_upgrade_service *s=ctx; s->finished(s->ctx,result);
}
tp_result tp_f407_upgrade_service_start(tp_f407_upgrade_service *s,const tp_f407_upgrade_service_options *o) {
    if(!s || !o || !o->runtime || !o->app || o->runtime!=&o->app->runtime || !o->runtime->owner || !o->try_lock || !o->unlock || !o->healthy || !o->finished ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    tp_f407_upgrade_service_options options=*o;
    memset(s,0,sizeof(*s));
    s->app=options.app; s->ctx=options.ctx; s->try_lock=options.try_lock;
    s->unlock=options.unlock; s->healthy=options.healthy; s->finished=options.finished;
    tp_result r=tp_f407_upgrade_init(&s->upgrade,&options.storage);
    if(r!=TP_OK) return r;
    r=tp_freertos_trial_start(&s->trial,&s->upgrade.secure.boot,s->upgrade.running_bank,
        options.stable_ms,options.max_gap_ms,options.period_ms,options.trial_timeout_ms,
        options.trial_stack_words,options.trial_priority,s,trial_lock,
        trial_unlock,trial_healthy,trial_finished);
    if(r!=TP_OK) return r;
    return tp_f407_upgrade_uart_init_guarded(&s->uart,&s->upgrade.secure.commands,
        options.frame_timeout_ms,options.uart_stack_words,options.uart_priority,
        options.ctx,options.try_lock,options.unlock,options.runtime,request_maintenance);
}
