#ifndef FREERTOS_H
#define FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef void *TaskHandle_t;
#define configTICK_RATE_HZ 1000
#define pdPASS 1
#define configMAX_PRIORITIES 8
#define pdMS_TO_TICKS(x) (x)
#define taskSCHEDULER_NOT_STARTED 0
#endif
