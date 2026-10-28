#include "tp_f407_app.h"
#include "tp_f407_recipe.h"
#include "tp_f407_emergency.h"
#include "tp_f407_iwdg.h"
#include "stm32f4xx.h"
#include <string.h>
static tp_f407_app *instance;
static tp_result valves(void *p,uint32_t mask) { return tp_f407_valves_set(&((tp_f407_app *)p)->valves,mask); }
static tp_result pump_start(void *p,int32_t steps,uint32_t hz) { return tp_f407_pump_start(&((tp_f407_app *)p)->pump,steps,hz); }
static tp_result pump_poll(void *p,bool *done) { return tp_f407_pump_poll(&((tp_f407_app *)p)->pump,done); }
static tp_result temperature(void *p,float *c) { return tp_f407_temperature_read(&((tp_f407_app *)p)->temperature,c); }
static tp_result heater(void *p,float fraction) { return tp_f407_heater_set(&((tp_f407_app *)p)->heater,fraction); }
static tp_result adc_read(void *p,uint8_t ch,float *v) { return tp_adc_reader_read(&((tp_f407_app *)p)->reader,ch,(uint32_t)xTaskGetTickCount(),v); }
static bool interlocks(void *p) {
    tp_f407_app *a=p;
    return a->interlocks_ok(a->interlocks_ctx) &&
           (!a->subordinate || tp_freertos_subordinate_healthy(a->subordinate));
}
static uint16_t resource_faults(void *p) {
    tp_f407_app *a=p; uint16_t faults=a->resource_faults(a->resource_ctx);
    if(a->subordinate) faults|=tp_freertos_subordinate_resource_faults(a->subordinate);
    return faults;
}
static tp_result preflight(void *p,uint16_t *faults) {
    tp_f407_app *a=p;
    tp_preflight_io io={a,interlocks,temperature,resource_faults,a->controller.max_temperature_c};
    tp_result r=tp_preflight_check(&io,faults);
    if (r==TP_INVALID) return r;
    if (!tp_adc_reader_ready(&a->reader)) *faults |= TP_PREFLIGHT_ADC;
    return *faults ? TP_IO_ERROR : TP_OK;
}
static tp_result recover_adc(void *p) {
    tp_f407_app *a=p;
    if (a->reader.active || a->adc.pending) return TP_BUSY;
    return tp_ads1220_recover(&a->adc);
}
static void safe(void *p) {
    tp_f407_app *a=p;
    tp_f407_heater_off(&a->heater); tp_f407_pump_stop(&a->pump); tp_f407_valves_safe(&a->valves);
    if (a->reader.adc) (void)tp_adc_reader_cancel(&a->reader);
}
static tp_result send(void *p,unsigned port,const uint8_t *bytes,size_t len) {
    tp_f407_app *a=p;
#ifdef TP_F407_SECOND_MODBUS
    if (port==1) return tp_f407_usart1_rtu_send(&a->uart1,port,bytes,len);
#endif
    return tp_f407_usart3_rtu_send(&a->uart,port,bytes,len);
}
static tp_result set_clock(void *p,uint32_t utc) { return tp_freertos_clock_set(&((tp_f407_app *)p)->clock,utc); }
static uint8_t clock_status(void *p) { return tp_freertos_clock_set_status(&((tp_f407_app *)p)->clock); }
static bool now(void *p,int64_t *utc) { return tp_freertos_clock_now(&((tp_f407_app *)p)->clock,utc); }
static tp_result request_log(void *p,bool latest,uint32_t sequence) {
    return tp_freertos_event_log_query(&((tp_f407_app *)p)->events,latest,sequence);
}
static bool poll_log(void *p,tp_result *result,tp_event *event) {
    return tp_freertos_event_log_poll(&((tp_f407_app *)p)->events,result,event);
}
static void log_stats(void *p,uint32_t counters[4]) {
    tp_event_log_stats stats;
    tp_freertos_event_log_stats(&((tp_f407_app *)p)->events,&stats);
    counters[0]=stats.queued; counters[1]=stats.written;
    counters[2]=stats.dropped; counters[3]=stats.io_errors;
}
static void record_event(void *p,const tp_event *event) {
    tp_f407_app *a=p;
    if(event->type==TP_EVENT_FAILED) {
        taskENTER_CRITICAL(); tp_trial_health_observe(&a->trial_health,false); taskEXIT_CRITICAL();
    }
    (void)tp_freertos_event_log_submit(&a->events,event);
}
static void control_progress(void *p) {
    tp_f407_app *a=p;
    uint16_t faults=0;
    bool healthy=a->controller.state==TP_IDLE && !a->service.maintenance &&
                 preflight(a,&faults)==TP_OK;
    taskENTER_CRITICAL(); tp_trial_health_observe(&a->trial_health,healthy); taskEXIT_CRITICAL();
    (void)tp_freertos_health_heartbeat(&a->health,0);
}
static void watchdog_feed(void *p) { (void)p; tp_f407_iwdg_feed(); }
static void watchdog_fault(void *p,uint32_t failed) {
    (void)p; (void)failed;
    /* No task or interrupt may reactivate outputs while waiting for reset. */
    __disable_irq(); tp_f407_emergency_off();
    for (;;) { __NOP(); }
}
static void comm_error(void *p,unsigned port) {
    (void)port; tp_f407_app *a=p;
    taskENTER_CRITICAL(); a->communication_errors++; taskEXIT_CRITICAL();
}
bool tp_f407_app_trial_healthy(tp_f407_app *a) {
    if(!a || a!=instance) return false;
    bool alive=tp_freertos_health_all_started(&a->health);
    bool healthy;
    taskENTER_CRITICAL(); healthy=tp_trial_health_take(&a->trial_health); taskEXIT_CRITICAL();
    return alive && healthy;
}
tp_result tp_f407_app_init(tp_f407_app *a,const tp_f407_app_options *o) {
    if (!a || !o || instance || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED ||
        !o->recipe || !o->recipe_len || !o->valves || !o->interlocks_ok || !o->resource_faults ||
        !o->channels || !o->channel_count || (o->subordinate && !o->subordinate->snapshot) ||
        tp_config_validate(&o->defaults)!=TP_OK ||
        tp_valve_policy_validate(o->valves)!=TP_OK) return TP_INVALID;
    instance=a; memset(a,0,sizeof(*a)); a->interlocks_ok=o->interlocks_ok; a->interlocks_ctx=o->interlocks_ctx;
    a->resource_faults=o->resource_faults; a->resource_ctx=o->resource_ctx;
    a->subordinate=o->subordinate;
    tp_result r;
#define INIT(expr) do { r=(expr); if (r!=TP_OK) goto fail; } while (0)
    INIT(tp_f407_valves_init(&a->valves,o->valves));
    INIT(tp_f407_pump_init(&a->pump));
    INIT(tp_f407_heater_init(&a->heater));
    INIT(tp_f407_i2c_init_on(&a->i2c,o->business_i2c ? o->business_i2c : 2));
    tp_i2c_io bus=tp_f407_i2c_io(&a->i2c);
    INIT(tp_mb85rc16_init(&a->fram,&bus,0));
    if (o->config_eeprom) {
        const tp_f407_eeprom_options *e=o->config_eeprom;
        INIT(tp_m24c02_init(&a->eeprom,&e->bus,e->address,e->wait_ctx,e->wait_ready));
        a->store=tp_m24c02_store(&a->eeprom);
    } else {
        a->store=tp_mb85rc16_store(&a->fram);
    }
    INIT(tp_store_bootstrap(&a->store,&o->defaults,&a->config,&a->config_sequence,&a->config_source));
    INIT(tp_mb85rc16_log_init(&a->log_region,&a->fram,256,1792));
    tp_event_log_io event_io=tp_mb85rc16_log_io(&a->log_region);
    INIT(tp_freertos_event_log_start(&a->events,&event_io,512,tskIDLE_PRIORITY));
    tp_event boot={.type=TP_EVENT_BOOT,.detail=(uint16_t)a->config_source,.value=a->config_sequence};
    INIT(tp_freertos_event_log_submit(&a->events,&boot));
    INIT(tp_ds3231_init(&a->rtc,&bus));
    INIT(tp_f407_ads1220_spi_init(&a->spi));
    tp_ads1220_io spi=tp_f407_ads1220_spi_io(&a->spi);
    INIT(tp_ads1220_init(&a->adc,&spi,o->adc_registers,o->adc_timeout_ms));
    INIT(tp_adc_reader_init(&a->reader,&a->adc,o->channels,o->channel_count));
    tp_hal hal={a,valves,pump_start,pump_poll,temperature,heater,adc_read,interlocks,safe};
    tp_pid pid={0}; tp_init(&a->controller,&hal,&a->config.calibration,&pid,a->config.max_temperature_c);
    INIT(tp_f407_recipe_validate(&a->controller,o->recipe,o->recipe_len,o->valves,o->channel_count));
    INIT(tp_f407_recipe_outputs(o->recipe,o->recipe_len,true));
    if (o->auto_calibration) {
        const tp_f407_auto_cal_options *cal=o->auto_calibration;
        INIT(tp_f407_recipe_validate(&a->controller,cal->low_recipe,cal->low_recipe_len,o->valves,o->channel_count));
        INIT(tp_f407_recipe_validate(&a->controller,cal->high_recipe,cal->high_recipe_len,o->valves,o->channel_count));
        INIT(tp_f407_recipe_outputs(cal->low_recipe,cal->low_recipe_len,false));
        INIT(tp_f407_recipe_outputs(cal->high_recipe,cal->high_recipe_len,false));
    }
    if (o->auto_standard) {
        const tp_f407_auto_standard_options *standard=o->auto_standard;
        INIT(tp_f407_recipe_validate(&a->controller,standard->recipe,standard->recipe_len,o->valves,o->channel_count));
        INIT(tp_f407_recipe_outputs(standard->recipe,standard->recipe_len,false));
    }
    INIT(tp_service_init(&a->service,&a->controller,o->recipe,o->recipe_len,0));
    INIT(tp_service_attach_preflight(&a->service,a,preflight));
    INIT(tp_service_attach_adc_recovery(&a->service,a,recover_adc));
    INIT(tp_service_attach_log(&a->service,a,request_log));
    INIT(tp_service_attach_log_stats(&a->service,a,log_stats));
    tp_runtime_io io={a,send,now,comm_error,record_event,poll_log,control_progress};
#ifdef TP_F407_SECOND_MODBUS
    const uint8_t addresses[]={a->config.slave_address,a->config.slave_address};
    INIT(tp_runtime_init(&a->runtime,&a->service,&io,addresses,2,10));
#else
    INIT(tp_runtime_init(&a->runtime,&a->service,&io,&a->config.slave_address,1,10));
#endif
    INIT(tp_runtime_attach_storage(&a->runtime,&a->store,&a->config));
    if (o->auto_calibration) {
        const tp_f407_auto_cal_options *cal=o->auto_calibration;
        INIT(tp_service_attach_auto_calibration(&a->service,
            cal->low_recipe,cal->low_recipe_len,cal->high_recipe,cal->high_recipe_len,
            cal->low_concentration,cal->high_concentration,cal->min_absorbance_delta));
    }
    if (o->auto_standard) {
        const tp_f407_auto_standard_options *standard=o->auto_standard;
        INIT(tp_service_attach_auto_standard(&a->service,standard->recipe,standard->recipe_len,
                                             standard->concentration,standard->tolerance));
    }
    INIT(tp_f407_temperature_start(&a->temperature,500,256,tskIDLE_PRIORITY+1));
    INIT(tp_freertos_clock_start(&a->clock,&a->rtc,1000,3000,256,tskIDLE_PRIORITY+1));
    INIT(tp_service_attach_clock(&a->service,a,set_clock,clock_status));
    INIT(tp_f407_usart3_rtu_init(&a->uart,&a->runtime,0,512,tskIDLE_PRIORITY+2));
#ifdef TP_F407_SECOND_MODBUS
    INIT(tp_f407_usart1_rtu_init(&a->uart1,&a->runtime,1,512,tskIDLE_PRIORITY+2));
#endif
    INIT(tp_runtime_start(&a->runtime,768,tskIDLE_PRIORITY+3));
    const uint32_t control_timeout=1000;
    INIT(tp_freertos_health_start(&a->health,&control_timeout,1,100,256,
                                  tskIDLE_PRIORITY+4,a,watchdog_feed,watchdog_fault));
    if(o->start_services) INIT(o->start_services(o->services_ctx,a));
    /* Start last: scheduler must be started promptly after this returns. */
    INIT(tp_f407_iwdg_start());
#undef INIT
    return TP_OK;
fail:
    /* The caller must reset, not resume or start partially initialized tasks. */
    safe(a);
    tp_f407_emergency_off();
    return r;
}
