#ifndef TP_F407_APP_H
#define TP_F407_APP_H
#include "tp_f407_i2c.h"
#include "tp_m24c02.h"
#include "tp_f407_ads1220_spi.h"
#include "tp_f407_valves.h"
#include "tp_f407_pump.h"
#include "tp_f407_heater.h"
#include "tp_f407_temperature.h"
#include "tp_f407_usart3_rtu.h"
#include "tp_freertos_clock.h"
#include "tp_adc_reader.h"
#include "tp_freertos_event_log.h"
#include "tp_freertos_health.h"
#include "tp_freertos_subordinate.h"

typedef struct tp_f407_app tp_f407_app;
typedef struct tp_f407_eeprom_options {
    tp_i2c_io bus;
    uint8_t address;
    void *wait_ctx;
    tp_result (*wait_ready)(void *, uint8_t);
} tp_f407_eeprom_options;
typedef struct {
    const tp_step *low_recipe; size_t low_recipe_len;
    const tp_step *high_recipe; size_t high_recipe_len;
    float low_concentration, high_concentration, min_absorbance_delta;
} tp_f407_auto_cal_options;
typedef struct {
    const tp_step *recipe; size_t recipe_len;
    float concentration, tolerance;
} tp_f407_auto_standard_options;
typedef struct {
    const tp_step *recipe; size_t recipe_len;
    /* 0 or 2: I2C2; 3: I2C3. Use I2C3 with the reference boot upgrade bus. */
    unsigned business_i2c;
    const tp_valve_policy *valves;
    bool (*interlocks_ok)(void *); void *interlocks_ctx;
    uint16_t (*resource_faults)(void *); void *resource_ctx;
    const tp_adc_channel *channels; size_t channel_count;
    uint8_t adc_registers[4]; uint32_t adc_timeout_ms;
    tp_config defaults;
    /* NULL selects the built-in FRAM configuration slots. When present,
     * EEPROM uses an independent or isolated I2C bus: MB85RC16 occupies
     * all addresses 0x50..0x57. The bus and wait context must remain valid. */
    const struct tp_f407_eeprom_options *config_eeprom;
    /* Optional; recipes must remain valid and immutable for the application lifetime.
     * Board configuration supplies the standard-selection and sampling steps. */
    const tp_f407_auto_cal_options *auto_calibration;
    /* Optional standard verification; recipe remains valid and immutable. */
    const tp_f407_auto_standard_options *auto_standard;
    /* Optional initialized master; UART task owns its link. */
    tp_freertos_subordinate *subordinate;
    /* Optional pre-scheduler board services, after app initialization and
     * before the watchdog starts. Failure requires a safe halt/reset. */
    tp_result (*start_services)(void *ctx,tp_f407_app *app);
    void *services_ctx;
} tp_f407_app_options;
/* Keep this object and all option pointers alive for the lifetime of the system.
 * Single instance, pre-scheduler only. On any error reset before retrying and do
 * not start the scheduler: initialization can have created tasks and queues. */
struct tp_f407_app {
    tp_f407_valves valves; tp_f407_pump pump; tp_f407_heater heater;
    tp_f407_i2c i2c; tp_mb85rc16 fram; tp_m24c02 eeprom; tp_store_io store; tp_ds3231 rtc;
    tp_mb85rc16_log_region log_region; tp_freertos_event_log events;
    tp_f407_ads1220_spi spi; tp_ads1220 adc; tp_adc_reader reader;
    tp_f407_temperature temperature; tp_freertos_clock clock;
    tp_controller controller; tp_service service; tp_freertos_runtime runtime;
    tp_f407_usart3_rtu uart; tp_freertos_health health;
    tp_trial_health trial_health; /* protected by task critical sections */
    bool (*interlocks_ok)(void *); void *interlocks_ctx;
    uint16_t (*resource_faults)(void *); void *resource_ctx;
    tp_freertos_subordinate *subordinate;
    tp_config config; tp_config_source config_source; uint32_t config_sequence;
    volatile uint32_t communication_errors;
};
tp_result tp_f407_app_init(tp_f407_app *app, const tp_f407_app_options *options);
/* Task context, one trial task only: consumes faults since previous sample.
 * Checks idle preflight and task liveness; board-specific self-tests remain required. */
bool tp_f407_app_trial_healthy(tp_f407_app *app);
#endif
