#include "tp_freertos_flash_clock.h"
#include "FreeRTOS.h"
#include "task.h"
#if configTICK_RATE_HZ != 1000
#error "tp_freertos_flash_clock requires a 1 kHz tick"
#endif
_Static_assert(sizeof(TickType_t)>=4, "32-bit FreeRTOS ticks required");
uint32_t tp_freertos_flash_now_ms(void *ctx) {
    (void)ctx;
    return (uint32_t)xTaskGetTickCount();
}
void tp_freertos_flash_sleep_ms(void *ctx) {
    (void)ctx;
    /* Two ticks ensure at least 1 ms even when called just before a tick. */
    vTaskDelay(2);
}
