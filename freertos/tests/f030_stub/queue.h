#ifndef TEST_QUEUE_H
#define TEST_QUEUE_H
#include "FreeRTOS.h"
#include <stddef.h>
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned,size_t);
void vQueueDelete(QueueHandle_t);
BaseType_t xQueueSendFromISR(QueueHandle_t,const void *,BaseType_t *);
BaseType_t xQueueReceive(QueueHandle_t,void *,TickType_t);
BaseType_t xQueueReset(QueueHandle_t);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t);
#endif
