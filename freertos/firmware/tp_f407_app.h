#ifndef TP_F407_APP_H
#define TP_F407_APP_H
#include "tp_f407_i2c.h"
#include "tp_f407_ads1220_spi.h"
#include "tp_f407_valves.h"
#include "tp_f407_pump.h"
#include "tp_f407_heater.h"
#include "tp_f407_temperature.h"
#include "tp_f407_usart3_rtu.h"
#include "tp_freertos_clock.h"
#include "tp_adc_reader.h"

typedef struct {
    const tp_step *recipe; size_t recipe_len;
    const tp_valve_policy *valves;
    bool (*interlocks_ok)(void *); void *interlocks_ctx;
    const tp_adc_channel *channels; size_t channel_count;
    uint8_t adc_registers[4]; uint32_t adc_timeout_ms;
    tp_config defaults;
} tp_f407_app_options;
/* Keep this object and all option pointers alive for the lifetime of the system.
 * Single instance, pre-scheduler only. On any error reset before retrying and do
 * not start the scheduler: initialization can have created tasks and queues. */
typedef struct {
    tp_f407_valves valves; tp_f407_pump pump; tp_f407_heater heater;
    tp_f407_i2c i2c; tp_mb85rc16 fram; tp_store_io store; tp_ds3231 rtc;
    tp_f407_ads1220_spi spi; tp_ads1220 adc; tp_adc_reader reader;
    tp_f407_temperature temperature; tp_freertos_clock clock;
    tp_controller controller; tp_service service; tp_freertos_runtime runtime;
    tp_f407_usart3_rtu uart;
    bool (*interlocks_ok)(void *); void *interlocks_ctx;
    tp_config config; tp_config_source config_source; uint32_t config_sequence;
    volatile uint32_t communication_errors;
} tp_f407_app;
tp_result tp_f407_app_init(tp_f407_app *app, const tp_f407_app_options *options);
#endif
