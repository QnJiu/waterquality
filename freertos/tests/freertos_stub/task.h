#ifndef TASK_H
#define TASK_H
#include "FreeRTOS.h"
BaseType_t xTaskGetSchedulerState(void);
TickType_t xTaskGetTickCount(void);
#endif
