#include "tp_f407_upgrade_service.h"
#include <string.h>

tp_result tp_f407_upgrade_service_start(tp_f407_upgrade_service *s,const tp_f407_upgrade_service_options *o) {
    if(!s || !o || !o->try_lock || !o->unlock || !o->healthy || !o->finished ||
       xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    tp_f407_upgrade_service_options options=*o;
    memset(s,0,sizeof(*s));
    tp_result r=tp_f407_upgrade_init(&s->upgrade,&options.storage);
    if(r!=TP_OK) return r;
    r=tp_freertos_trial_start(&s->trial,&s->upgrade.secure.boot,s->upgrade.running_bank,
        options.stable_ms,options.max_gap_ms,options.period_ms,options.trial_timeout_ms,
        options.trial_stack_words,options.trial_priority,options.ctx,options.try_lock,
        options.unlock,options.healthy,options.finished);
    if(r!=TP_OK) return r;
    return tp_f407_upgrade_uart_init_shared(&s->uart,&s->upgrade.secure.commands,
        options.frame_timeout_ms,options.uart_stack_words,options.uart_priority,
        options.ctx,options.try_lock,options.unlock);
}
