#include "tp_f407_diagnostics.h"
#include "tp_f407_timebase.h"
#include "tp_cpu_usage.h"
#include "stm32f4xx.h"
#include <stdint.h>

volatile tp_f407_diagnostics tp_debug_resources;
static TaskHandle_t diagnostics_task;
/* Called by the scheduler or in a critical section. Sample at least once per
 * TIM5 wrap (about 71 minutes at 1 MHz); the diagnostic task samples every 5 s. */
uint64_t tp_f407_runtime_counter(void) {
    static tp_counter_extension counter;
    return tp_counter_extend(&counter,TIM5->CNT);
}
static uint32_t words(TaskHandle_t task) {
    return task ? (uint32_t)uxTaskGetStackHighWaterMark(task) : UINT32_MAX;
}
static void sample_resources(void *ctx) {
    tp_f407_app *app=ctx;
    uint64_t last_time=0, last_idle=0;
    bool have_sample=false;
    for (;;) {
        uint64_t now, idle;
        taskENTER_CRITICAL();
        now=tp_f407_runtime_counter();
        idle=ulTaskGetIdleRunTimeCounter();
        taskEXIT_CRITICAL();
        uint64_t elapsed=now-last_time, idle_elapsed=idle-last_idle;
        tp_debug_resources.cpu_busy_percent=UINT32_MAX;
        if (have_sample) tp_debug_resources.cpu_busy_percent=tp_cpu_busy_percent(elapsed,idle_elapsed);
        last_time=now; last_idle=idle; have_sample=true;
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
    tp_result result=tp_f407_timebase_init();
    if (result!=TP_OK) return result;
    return xTaskCreate(sample_resources,"tp-debug",256,app,tskIDLE_PRIORITY,
                       &diagnostics_task)==pdPASS ? TP_OK : TP_IO_ERROR;
}
