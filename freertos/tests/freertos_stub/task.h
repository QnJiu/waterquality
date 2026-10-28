#ifndef TASK_H
#define TASK_H
#include "FreeRTOS.h"
BaseType_t xTaskGetSchedulerState(void);
TickType_t xTaskGetTickCount(void);
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint16_t stack,void *arg,UBaseType_t priority,TaskHandle_t *task);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
#endif
