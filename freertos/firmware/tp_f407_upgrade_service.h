#ifndef TP_F407_UPGRADE_SERVICE_H
#define TP_F407_UPGRADE_SERVICE_H
#include "tp_f407_upgrade.h"
#include "tp_f407_upgrade_uart.h"
#include "tp_freertos_trial.h"
#include "tp_f407_app.h"

/* The callbacks and all hardware contexts remain valid for the life of the
 * tasks. lock/unlock must serialize every upgrade, confirmation and image
 * access. Application trial health is always checked; board healthy must be nonblocking and include latched faults since the
 * prior sample; finished is responsible for safe failure handling/reset.
 * runtime must be started before this service. Board code must ensure safe
 * outputs and watchdog behavior throughout single-bank flash writes. */
typedef struct {
    tp_f407_upgrade_options storage;
    tp_freertos_runtime *runtime;
    tp_f407_app *app; /* must own runtime */
    void *ctx;
    bool (*try_lock)(void *);
    void (*unlock)(void *);
    bool (*healthy)(void *);
    void (*finished)(void *,tp_result);
    uint32_t frame_timeout_ms, stable_ms, max_gap_ms, period_ms, trial_timeout_ms;
    uint16_t uart_stack_words, trial_stack_words;
    UBaseType_t uart_priority, trial_priority;
} tp_f407_upgrade_service_options;
typedef struct {
    tp_f407_upgrade upgrade;
    tp_freertos_trial trial;
    tp_f407_upgrade_uart uart;
    tp_f407_app *app;
    void *ctx;
    bool (*try_lock)(void *);
    void (*unlock)(void *);
    bool (*healthy)(void *);
    void (*finished)(void *,tp_result);
} tp_f407_upgrade_service;
/* Before the scheduler, once per boot. running_vector must come from VTOR.
 * If any step fails, do not start the scheduler or retry: earlier tasks may
 * already have been created; put outputs safe and reset. Do not copy objects. */
tp_result tp_f407_upgrade_service_start(tp_f407_upgrade_service *,const tp_f407_upgrade_service_options *);
#endif
