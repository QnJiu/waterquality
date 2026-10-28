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
    volatile uint32_t droplets_pending;
    volatile bool droplets_overflow;
    uint32_t authorization_id;
    bool authorization_approved, authorization_pending;
    bool processing_rx;
    void (*door_fault_handler)(void *);
    void *door_fault_ctx;
    bool door_fault_reported;
    void *door_recovery_ctx;
    tp_result (*door_recovery)(void *);
    bool recovery_requested, recovery_inflight; /* protected by task critical section */
    bool recovering, verifying_reader; /* owner task only */
} tp_f030_subordinate;

/* Owns USART1, TIM3, PA9/PA10 and their interrupts. Call once before scheduler.
 * input callback must be bounded and nonblocking; all objects outlive the task. */
tp_result tp_f030_subordinate_init(tp_f030_subordinate *, void *input_ctx,
    tp_result (*read_inputs)(void *, tp_sub_inputs *), tp_door_service *door,
    bool door_fault_is_device_fault, uint16_t stack_words, UBaseType_t priority);
/* Install before the scheduler starts. Called once in the owner task on door failure. */
tp_result tp_f030_subordinate_on_door_fault(tp_f030_subordinate *,void (*handler)(void *),void *ctx);
/* Register before scheduler start. Callback is nonblocking, runs in owner task,
 * returns TP_BUSY while resetting/synchronizing, and on TP_OK must have invoked
 * tp_door_service_recover. It must impose its own bounded timeout. */
tp_result tp_f030_subordinate_set_door_recovery(tp_f030_subordinate *,void *,tp_result (*)(void *));
/* Queue a recovery attempt; TP_OK means queued. Task context only. */
tp_result tp_f030_subordinate_recover_door(tp_f030_subordinate *);
/* STOP owner only, with interrupts masked and TIM3 stopped. */
void tp_f030_subordinate_add_sleep_us(uint32_t elapsed_us);
/* Call from an ISR after the scheduler starts. wake is passed to portYIELD_FROM_ISR. */
void tp_f030_subordinate_droplet_from_isr(tp_f030_subordinate *,BaseType_t *wake);
/* Task context only, after scheduler start. TP_OK means queued, not accepted. */
tp_result tp_f030_subordinate_authorize(tp_f030_subordinate *,uint32_t id,bool approved);
#endif
