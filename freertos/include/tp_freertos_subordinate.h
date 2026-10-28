#ifndef TP_FREERTOS_SUBORDINATE_H
#define TP_FREERTOS_SUBORDINATE_H
#include "tp_subordinate_link.h"
#include "FreeRTOS.h"
#include "queue.h"

typedef struct {
    tp_sub_link link;
    QueueHandle_t snapshot;
} tp_freertos_subordinate;

/* Initialize before the scheduler. The UART task is the sole owner of link
 * operations; other tasks may only call the two snapshot readers. */
tp_result tp_freertos_subordinate_init(tp_freertos_subordinate *,void *,tp_sub_send,
    uint32_t baud,uint8_t bits,uint32_t period_ms,uint32_t timeout_ms,
    uint32_t freshness_ms,uint32_t first_token);
tp_result tp_freertos_subordinate_byte(tp_freertos_subordinate *,uint8_t,uint32_t us,uint32_t ms);
tp_result tp_freertos_subordinate_byte_at(tp_freertos_subordinate *,uint8_t,uint32_t us,
                                          uint32_t received_ms,uint32_t now_ms);
tp_result tp_freertos_subordinate_error(tp_freertos_subordinate *,uint32_t us,uint32_t ms);
tp_result tp_freertos_subordinate_tick(tp_freertos_subordinate *,uint32_t us,uint32_t ms);
bool tp_freertos_subordinate_healthy(void *);
uint16_t tp_freertos_subordinate_resource_faults(void *);
#endif
