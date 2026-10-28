#ifndef TP_SERVICE_H
#define TP_SERVICE_H
#include "tp_core.h"
#include "tp_schedule.h"
#include "tp_store.h"
/* All service functions must run in the controller task. Communications tasks
 * must marshal requests to that task and read coherent snapshots; they must not
 * invoke callbacks concurrently with tick. Give stop requests priority. */
typedef struct {
    tp_controller *controller;
    const tp_step *recipe;
    size_t recipe_len;
    tp_schedule schedule;
    uint32_t now_ms;
    tp_result last_start_result;
    tp_config config;
    void *save_ctx;
    tp_result (*request_save)(void *, const tp_config *);
    uint8_t save_status; /* 0 unavailable, 1 idle, 2 pending, 3 saved, 4 failed */
} tp_service;
tp_result tp_service_init(tp_service *s, tp_controller *controller,
                          const tp_step *recipe, size_t recipe_len, uint32_t now_ms);
/* request_save must copy the configuration into a worker queue without blocking.
 * Completion is reported back in the owner task with tp_service_save_complete. */
tp_result tp_service_attach_config(tp_service *s, const tp_config *cfg,
    void *ctx, tp_result (*request_save)(void *, const tp_config *));
void tp_service_save_complete(tp_service *s, tp_result result);
void tp_service_tick(tp_service *s, uint32_t now_ms, bool rtc_valid, int64_t utc_seconds);
/* Functions implement the tp_modbus callbacks when called in the owner task. */
tp_result tp_service_read_holding(void *ctx, uint16_t address, uint16_t count, uint16_t *values);
tp_result tp_service_write_holding(void *ctx, uint16_t address, uint16_t count, const uint16_t *values);
#endif

