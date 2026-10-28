#ifndef QUEUE_H
#define QUEUE_H
#include "FreeRTOS.h"
#include <stddef.h>
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned,size_t);
BaseType_t xQueueOverwrite(QueueHandle_t,const void *);
BaseType_t xQueuePeek(QueueHandle_t,void *,TickType_t);
#endif
