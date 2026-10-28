#ifndef TP_F407_STAGE_SERVICE_H
#define TP_F407_STAGE_SERVICE_H
#include "tp_f407_upgrade_service.h"
#include "tp_freertos_stage_transport.h"
/* Optional staging endpoint sharing the existing service lock and maintenance
 * gate. Initialize object to zero; never move it or retry after task startup.
 * Staging metadata and image partitions must be independent from all other
 * stores and remain valid. Board code ensures safe single-bank flash writing,
 * access control, watchdog policy and reports/handles installation results. */
typedef struct {
    tp_stage_session session;
    tp_freertos_stage_transport transport;
    bool started;
} tp_f407_stage_service;
typedef struct {
    tp_download_io storage;
    uint32_t capacity[2];
    tp_stage_transport_io transport;
    uint32_t frame_timeout_ms, idle_timeout_ms, period_ms;
    uint16_t stack_words;
    UBaseType_t priority;
} tp_f407_stage_options;
/* Call once before scheduler after tp_f407_upgrade_service_start. On failure
 * do not start the scheduler; reset before retrying partial assembly. */
tp_result tp_f407_stage_service_start(tp_f407_stage_service *,tp_f407_upgrade_service *,
                                      const tp_f407_stage_options *);
#endif
