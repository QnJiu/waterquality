#ifndef TASK_H
#define TASK_H
#include "FreeRTOS.h"
#define taskSCHEDULER_RUNNING 2
#define taskSCHEDULER_NOT_STARTED 0
BaseType_t xTaskGetSchedulerState(void);
TickType_t xTaskGetTickCount(void);
BaseType_t xTaskCreate(void (*)(void *),const char *,uint16_t,void *,UBaseType_t,TaskHandle_t *);
void vTaskDelete(TaskHandle_t);
void xTaskNotifyGive(TaskHandle_t);
uint32_t ulTaskNotifyTake(BaseType_t,TickType_t);
#endif
