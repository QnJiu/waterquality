#ifndef TP_SERVICE_H
#define TP_SERVICE_H
#include "tp_core.h"
#include "tp_auto_calibration.h"
#include "tp_schedule.h"
#include "tp_store.h"
#include "tp_event_log.h"
#include "tp_preflight.h"
/* All service functions must run in the controller task. Communications tasks
 * must marshal requests to that task and read coherent snapshots; they must not
 * invoke callbacks concurrently with tick. Give stop requests priority. */
typedef struct {
    tp_controller *controller;
    tp_auto_calibration auto_cal;
    tp_auto_standard auto_standard;
    const tp_step *standard_recipe;
    size_t standard_length;
    float standard_concentration, standard_tolerance;
    bool standard_attached;
    const tp_step *cal_recipes[2];
    size_t cal_lengths[2];
    float cal_concentrations[2], cal_min_delta;
    bool cal_attached;
    bool maintenance; /* latched until service reinitialization */
    const tp_step *recipe;
    size_t recipe_len;
    tp_schedule schedule;
    uint32_t now_ms;
    tp_result last_start_result;
    tp_config config;
    void *save_ctx;
    tp_result (*request_save)(void *, const tp_config *);
    uint8_t save_status; /* 0 unavailable, 1 idle, 2 pending, 3 saved, 4 failed */
    void *log_ctx;
    tp_result (*request_log)(void *, bool latest, uint32_t sequence);
    uint8_t log_status; /* 0 unavailable, 1 idle, 2 pending, 3 found, 4 absent, 5 I/O error */
    tp_event log_event;
    void *log_stats_ctx;
    void (*log_stats)(void *,uint32_t counters[4]);
    void *preflight_ctx;
    tp_result (*preflight)(void *,uint16_t *);
    uint16_t preflight_faults;
    bool preflight_checked; /* at least one check was attempted */
    uint8_t standard_check_status; /* 0 none, 1 passed, 2 failed */
    float standard_check_actual;
    void *recover_ctx;
    tp_result (*recover_adc)(void *);
    bool clock_valid;
    uint32_t clock_utc;
    void *clock_ctx;
    tp_result (*set_clock)(void *,uint32_t);
    uint8_t (*clock_status)(void *);
    void *version_ctx;
    uint32_t (*confirmed_version)(void *);
} tp_service;
tp_result tp_service_init(tp_service *s, tp_controller *controller,
                          const tp_step *recipe, size_t recipe_len, uint32_t now_ms);
/* request_save must copy the configuration into a worker queue without blocking.
 * Completion is reported back in the owner task with tp_service_save_complete.
 * Reattachment is rejected while running, saving, calibrating, checking a
 * standard, or in upgrade maintenance. Call only from the owner task. */
tp_result tp_service_attach_config(tp_service *s, const tp_config *cfg,
    void *ctx, tp_result (*request_save)(void *, const tp_config *));
void tp_service_save_complete(tp_service *s, tp_result result);
/* Attach immutable recipes and reference values before starting calibration.
 * Recipes must remain valid until service reinitialization. Owner task only. */
tp_result tp_service_attach_auto_calibration(tp_service *s,
    const tp_step *low, size_t low_len, const tp_step *high, size_t high_len,
    float low_concentration, float high_concentration, float min_delta);
/* Attach the standard-liquid recipe and reference parameters before use. */
tp_result tp_service_attach_auto_standard(tp_service *, const tp_step *, size_t,
    float concentration, float tolerance);
/* Optional nonblocking startup check; invoked for every actual start request. */
tp_result tp_service_attach_preflight(tp_service *,void *,tp_result (*)(void *,uint16_t *));
/* Optional bounded ADC recovery, called only by the owner with outputs safe.
 * The callback must not start a conversion or clear a controller fault. */
tp_result tp_service_attach_adc_recovery(tp_service *,void *,tp_result (*)(void *));
/* Nonblocking request and status snapshot; worker owns RTC I/O. */
tp_result tp_service_attach_clock(tp_service *,void *,tp_result (*)(void *,uint32_t),uint8_t (*)(void *));
/* Optional nonblocking snapshot; zero means no confirmed running version. */
tp_result tp_service_attach_version(tp_service *,void *,uint32_t (*)(void *));
/* One asynchronous query at a time; callback copies the request without waiting.
 * Complete only from the service owner task. TP_INVALID means not found. */
tp_result tp_service_attach_log(tp_service *,void *ctx,tp_result (*request)(void *,bool,uint32_t));
void tp_service_log_complete(tp_service *,tp_result,const tp_event *);
/* Nonblocking atomic snapshot: queued, written, dropped and I/O errors. */
tp_result tp_service_attach_log_stats(tp_service *,void *,void (*)(void *,uint32_t counters[4]));
/* Owner task only. Safe outputs are applied before success; latched until reset. */
tp_result tp_service_enter_maintenance(tp_service *s);
void tp_service_tick(tp_service *s, uint32_t now_ms, bool rtc_valid, int64_t utc_seconds);
/* Functions implement the tp_modbus callbacks when called in the owner task. */
tp_result tp_service_read_holding(void *ctx, uint16_t address, uint16_t count, uint16_t *values);
tp_result tp_service_write_holding(void *ctx, uint16_t address, uint16_t count, const uint16_t *values);
#endif

