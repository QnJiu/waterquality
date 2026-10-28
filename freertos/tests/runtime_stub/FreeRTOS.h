#ifndef FREERTOS_H
#define FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef unsigned UBaseType_t;
typedef int BaseType_t;
typedef void *TaskHandle_t;
#define configTICK_RATE_HZ 1000
#define pdPASS 1
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY UINT32_MAX
void test_enter(void);
void test_exit(void);
#define taskENTER_CRITICAL() test_enter()
#define taskEXIT_CRITICAL() test_exit()
#endif
