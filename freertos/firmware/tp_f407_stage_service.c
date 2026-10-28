#include "tp_f407_stage_service.h"
static bool lock(void *ctx) {
    tp_f407_upgrade_service *s=ctx; return s->try_lock(s->ctx);
}
static void unlock(void *ctx) {
    tp_f407_upgrade_service *s=ctx; s->unlock(s->ctx);
}
static tp_result maintenance(void *ctx) {
    tp_f407_upgrade_service *s=ctx; return tp_runtime_request_maintenance(&s->app->runtime);
}
tp_result tp_f407_stage_service_start(tp_f407_stage_service *stage,tp_f407_upgrade_service *s,
                                      const tp_f407_stage_options *o) {
    if(!stage || !s || !o || stage->started || !s->app || !s->app->runtime.owner ||
       !s->try_lock || !s->unlock || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)
        return TP_INVALID;
    tp_result r=tp_stage_session_init(&stage->session,&s->upgrade.secure,&o->storage,o->capacity,
                                      s,lock,unlock,maintenance);
    if(r!=TP_OK) return r;
    r=tp_freertos_stage_transport_start(&stage->transport,&stage->session,&o->transport,
        o->frame_timeout_ms,o->idle_timeout_ms,o->period_ms,o->stack_words,o->priority);
    if(r==TP_OK) stage->started=true;
    return r;
}
