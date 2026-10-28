#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include <stdint.h>
typedef uint32_t TickType_t;
typedef unsigned UBaseType_t;
typedef int BaseType_t;
#define pdPASS 1
#define pdTRUE 1
#define pdFALSE 0
#define configMAX_PRIORITIES 8
#define pdMS_TO_TICKS(ms) (ms)
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 1
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define portYIELD_FROM_ISR(wake) ((void)(wake))
#define portSET_INTERRUPT_MASK_FROM_ISR() 0u
#define portCLEAR_INTERRUPT_MASK_FROM_ISR(mask) ((void)(mask))
#endif
