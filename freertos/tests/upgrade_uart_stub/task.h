#ifndef TASK_H
#define TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 1
BaseType_t xTaskGetSchedulerState(void);
TickType_t xTaskGetTickCount(void);
TickType_t xTaskGetTickCountFromISR(void);
void vTaskDelay(TickType_t);
BaseType_t xTaskCreate(void (*)(void *),const char *,uint16_t,void *,UBaseType_t,TaskHandle_t *);
void vTaskNotifyGiveFromISR(TaskHandle_t,BaseType_t *);
uint32_t ulTaskNotifyTake(BaseType_t,TickType_t);
#endif
