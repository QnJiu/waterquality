#include "tp_f407_diagnostics.h"
#include <stdint.h>

volatile tp_f407_diagnostics tp_debug_resources;
static TaskHandle_t diagnostics_task;
static uint32_t words(TaskHandle_t task) {
    return task ? (uint32_t)uxTaskGetStackHighWaterMark(task) : UINT32_MAX;
}
static void sample_resources(void *ctx) {
    tp_f407_app *app=ctx;
    for (;;) {
        tp_debug_resources.heap_free_bytes=(uint32_t)xPortGetFreeHeapSize();
        tp_debug_resources.heap_min_bytes=(uint32_t)xPortGetMinimumEverFreeHeapSize();
        tp_debug_resources.control_words=words(app->runtime.owner);
        tp_debug_resources.storage_words=words(app->runtime.storage_task);
        tp_debug_resources.temperature_words=words(app->temperature.task);
        tp_debug_resources.clock_words=words(app->clock.task);
        tp_debug_resources.uart_words=words(app->uart.task);
        tp_debug_resources.health_words=words(app->health.task);
        tp_debug_resources.events_words=words(app->events.task);
        tp_debug_resources.diagnostics_words=words(diagnostics_task);
        tp_debug_resources.updates++;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
tp_result tp_f407_diagnostics_start(tp_f407_app *app) {
    if (!app || diagnostics_task || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED)
        return TP_INVALID;
    return xTaskCreate(sample_resources,"tp-debug",256,app,tskIDLE_PRIORITY,
                       &diagnostics_task)==pdPASS ? TP_OK : TP_IO_ERROR;
}
