#ifndef TP_FREERTOS_FLASH_CLOCK_H
#define TP_FREERTOS_FLASH_CLOCK_H
#include <stdint.h>
/* For W25Q128 image operations in a running background task only.
 * The project must use a 1 kHz tick with at least 32-bit TickType_t. */
uint32_t tp_freertos_flash_now_ms(void *ctx);
void tp_freertos_flash_sleep_ms(void *ctx);
#endif
