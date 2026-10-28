#include "tp_f407_app.h"
#include "tp_f407_emergency.h"
#include <string.h>
static tp_f407_app *instance;
static tp_result valves(void *p,uint32_t mask) { return tp_f407_valves_set(&((tp_f407_app *)p)->valves,mask); }
static tp_result pump_start(void *p,int32_t steps,uint32_t hz) { return tp_f407_pump_start(&((tp_f407_app *)p)->pump,steps,hz); }
static tp_result pump_poll(void *p,bool *done) { return tp_f407_pump_poll(&((tp_f407_app *)p)->pump,done); }
static tp_result temperature(void *p,float *c) { return tp_f407_temperature_read(&((tp_f407_app *)p)->temperature,c); }
static tp_result heater(void *p,float fraction) { return tp_f407_heater_set(&((tp_f407_app *)p)->heater,fraction); }
static tp_result adc_read(void *p,uint8_t ch,float *v) { return tp_adc_reader_read(&((tp_f407_app *)p)->reader,ch,(uint32_t)xTaskGetTickCount(),v); }
static bool interlocks(void *p) { tp_f407_app *a=p; return a->interlocks_ok(a->interlocks_ctx); }
static void safe(void *p) {
    tp_f407_app *a=p;
    tp_f407_heater_off(&a->heater); tp_f407_pump_stop(&a->pump); tp_f407_valves_safe(&a->valves);
    if (a->reader.adc) (void)tp_adc_reader_cancel(&a->reader);
}
static tp_result send(void *p,unsigned port,const uint8_t *bytes,size_t len) { return tp_f407_usart3_rtu_send(&((tp_f407_app *)p)->uart,port,bytes,len); }
static bool now(void *p,int64_t *utc) { return tp_freertos_clock_now(&((tp_f407_app *)p)->clock,utc); }
static void record_event(void *p,const tp_event *event) {
    (void)tp_freertos_event_log_submit(&((tp_f407_app *)p)->events,event);
}
static void comm_error(void *p,unsigned port) {
    (void)port; tp_f407_app *a=p;
    taskENTER_CRITICAL(); a->communication_errors++; taskEXIT_CRITICAL();
}
tp_result tp_f407_app_init(tp_f407_app *a,const tp_f407_app_options *o) {
    if (!a || !o || instance || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED ||
        !o->recipe || !o->recipe_len || !o->valves || !o->interlocks_ok ||
        !o->channels || !o->channel_count || tp_config_validate(&o->defaults)!=TP_OK ||
        tp_valve_policy_validate(o->valves)!=TP_OK) return TP_INVALID;
    instance=a; memset(a,0,sizeof(*a)); a->interlocks_ok=o->interlocks_ok; a->interlocks_ctx=o->interlocks_ctx;
    tp_result r;
#define INIT(expr) do { r=(expr); if (r!=TP_OK) goto fail; } while (0)
    INIT(tp_f407_valves_init(&a->valves,o->valves));
    INIT(tp_f407_pump_init(&a->pump));
    INIT(tp_f407_heater_init(&a->heater));
    INIT(tp_f407_i2c_init(&a->i2c));
    tp_i2c_io bus=tp_f407_i2c_io(&a->i2c);
    INIT(tp_mb85rc16_init(&a->fram,&bus,0)); a->store=tp_mb85rc16_store(&a->fram);
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
    INIT(tp_service_init(&a->service,&a->controller,o->recipe,o->recipe_len,0));
    tp_runtime_io io={a,send,now,comm_error,record_event};
    INIT(tp_runtime_init(&a->runtime,&a->service,&io,&a->config.slave_address,1,10));
    INIT(tp_runtime_attach_storage(&a->runtime,&a->store,&a->config));
    INIT(tp_f407_temperature_start(&a->temperature,500,256,tskIDLE_PRIORITY+1));
    INIT(tp_freertos_clock_start(&a->clock,&a->rtc,1000,3000,256,tskIDLE_PRIORITY+1));
    INIT(tp_f407_usart3_rtu_init(&a->uart,&a->runtime,0,512,tskIDLE_PRIORITY+2));
    INIT(tp_runtime_start(&a->runtime,768,tskIDLE_PRIORITY+3));
#undef INIT
    return TP_OK;
fail:
    /* The caller must reset, not resume or start partially initialized tasks. */
    safe(a);
    tp_f407_emergency_off();
    return r;
}
