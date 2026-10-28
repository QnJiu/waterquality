#include "tp_f407_ota_reference.h"

tp_result tp_f407_ota_reference_start(void *ctx,tp_f407_app *app) {
    tp_f407_ota_reference *ota=ctx;
    if(!ota || !app || !ota->stage_finished || ota->attempted ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    ota->attempted=true;
    tp_result r=tp_f407_upgrade_reference_start(&ota->upgrade,app);
    if(r!=TP_OK) return r;
    return tp_f407_stage_reference_start(&ota->stage,&ota->upgrade.service,
        &ota->stage_options,ota->stage_ctx,ota->stage_finished);
}
