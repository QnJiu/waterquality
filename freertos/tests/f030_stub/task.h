#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
BaseType_t xTaskCreate(void (*)(void *),const char *,uint16_t,void *,UBaseType_t,TaskHandle_t *);
BaseType_t xTaskGetSchedulerState(void);
TickType_t xTaskGetTickCount(void);
void vTaskNotifyGiveFromISR(TaskHandle_t,BaseType_t *);
BaseType_t xTaskNotifyGive(TaskHandle_t);
uint32_t ulTaskNotifyTake(BaseType_t,TickType_t);
#endif
