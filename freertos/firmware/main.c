/* Scheduler bring-up image only: no actuator or measurement is enabled. */
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
volatile uint32_t tp_scheduler_heartbeat;
static void heartbeat(void *arg) {
    (void)arg;
    for (;;) { tp_scheduler_heartbeat++; vTaskDelay(pdMS_TO_TICKS(1000)); }
}
void tp_platform_assert(const char *file, int line) {
    (void)file; (void)line;
    __disable_irq();
    for (;;) { __NOP(); }
}
void vApplicationMallocFailedHook(void) { tp_platform_assert(__FILE__, __LINE__); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task; (void)name; tp_platform_assert(__FILE__, __LINE__);
}
int main(void) {
    SystemCoreClockUpdate();
    if (xTaskCreate(heartbeat, "heartbeat", configMINIMAL_STACK_SIZE, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS)
        tp_platform_assert(__FILE__, __LINE__);
    vTaskStartScheduler();
    tp_platform_assert(__FILE__, __LINE__);
}
