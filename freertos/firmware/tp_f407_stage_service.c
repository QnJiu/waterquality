#include "tp_f407_stage_service.h"
tp_result tp_f407_stage_storage_init(tp_f407_stage_options *o,tp_f407_upgrade_service *s,
    tp_w25q128_image *image,tp_w25q128 *flash,void *clock,uint32_t (*now)(void *),
    void (*sleep)(void *),uint32_t base0,uint32_t base1,uint32_t size,
    uint32_t program_timeout,uint32_t erase_timeout) {
    if(!o || !s || !image || !s->app || !s->upgrade.fram.staging.bus.write ||
       !s->upgrade.fram.staging.bus.write_read || s->upgrade.fram.staging.base!=768 ||
       s->upgrade.running_bank>1 || size>TP_F407_IMAGE_BANK_SIZE ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    tp_result r=tp_w25q128_image_init(image,flash,clock,now,sleep,base0,base1,size,
        s->upgrade.running_bank,program_timeout,erase_timeout);
    if(r!=TP_OK) return r;
    o->storage=(tp_download_io){tp_upgrade_fram_store(&s->upgrade.fram.staging),image,
        tp_w25q128_image_erase,tp_w25q128_image_read,tp_w25q128_image_write,NULL};
    o->capacity[0]=size; o->capacity[1]=size;
    return TP_OK;
}
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
