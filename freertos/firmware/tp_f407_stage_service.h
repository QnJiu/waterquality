#ifndef TP_F407_STAGE_SERVICE_H
#define TP_F407_STAGE_SERVICE_H
#include "tp_f407_upgrade_service.h"
#include "tp_freertos_stage_transport.h"
#include "tp_w25q128_image.h"
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
/* Assemble the reference storage after the upgrade service and W25Q128 have
 * been initialized. The image and service must stay at fixed addresses. The
 * caller owns the whole flash and must reserve both ranges against other uses. */
tp_result tp_f407_stage_storage_init(tp_f407_stage_options *,tp_f407_upgrade_service *,
    tp_w25q128_image *,tp_w25q128 *,void *,uint32_t (*)(void *),void (*)(void *),
    uint32_t base0,uint32_t base1,uint32_t size,uint32_t program_timeout_ms,uint32_t erase_timeout_ms);
/* Call once before scheduler after tp_f407_upgrade_service_start. On failure
 * do not start the scheduler; reset before retrying partial assembly. */
tp_result tp_f407_stage_service_start(tp_f407_stage_service *,tp_f407_upgrade_service *,
                                      const tp_f407_stage_options *);
#endif
