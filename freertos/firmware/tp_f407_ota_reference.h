#ifndef TP_F407_OTA_REFERENCE_H
#define TP_F407_OTA_REFERENCE_H
#include "tp_f407_upgrade_reference.h"
#include "tp_f407_stage_reference.h"

/* Optional combined wired and staged OTA assembly. Fill upgrade.options and
 * stage_options, stage_finished and stage_ctx before startup. All objects,
 * hardware contexts and callbacks must remain valid until reset. */
typedef struct {
    tp_f407_upgrade_reference upgrade;
    tp_f407_stage_reference stage;
    tp_f407_stage_options stage_options;
    void *stage_ctx;
    void (*stage_finished)(void *,tp_result);
    bool attempted;
} tp_f407_ota_reference;

/* Suitable for tp_f407_app_options.start_services. Failure may leave tasks
 * or peripherals initialized; halt and reset, never start or retry them. */
tp_result tp_f407_ota_reference_start(void *ctx,tp_f407_app *app);
#endif
