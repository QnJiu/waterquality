#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
typedef enum { eAbortSleep, eStandardSleep, eNoTasksWaitingTimeout } eSleepModeStatus;
eSleepModeStatus eTaskConfirmSleepModeStatus(void);
void vTaskStepTick(TickType_t ticks);
#endif
