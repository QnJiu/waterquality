#ifndef TP_F030_SUBORDINATE_H
#define TP_F030_SUBORDINATE_H

#include "tp_subordinate_app.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

typedef struct {
    uint32_t us;
    uint8_t byte;
    bool error;
} tp_f030_rx_event;

typedef struct {
    tp_sub_app app;
    QueueHandle_t rx;
    TaskHandle_t task;
    void *input_ctx;
    tp_result (*read_inputs)(void *, tp_sub_inputs *);
    uint8_t tx[TP_SUB_RESPONSE_SIZE];
    volatile uint8_t tx_length, tx_position;
    volatile bool transmitting, overflow;
    bool processing_rx;
} tp_f030_subordinate;

/* Owns USART1, TIM3, PA9/PA10 and their interrupts. Call once before scheduler.
 * input callback must be bounded and nonblocking; all objects outlive the task. */
tp_result tp_f030_subordinate_init(tp_f030_subordinate *, void *input_ctx,
    tp_result (*read_inputs)(void *, tp_sub_inputs *), tp_door_service *door,
    bool door_fault_is_device_fault, uint16_t stack_words, UBaseType_t priority);
#endif
